#include "minic/runtime.hpp"
#include "../common/fd.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <limits.h>

namespace minic {
namespace {
using detail::Fd;
struct Record { int stage; int error; }; // 1: about to exec; 2: stdin; 3: exec
bool write_record(int fd, Record record) {
    const char* p = reinterpret_cast<const char*>(&record);
    std::size_t n = sizeof(record);
    while (n) {
        ssize_t count = write(fd, p, n);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        p += count; n -= static_cast<std::size_t>(count);
    }
    return true;
}
// Named execution point for the S02 GDB exercise. No C++ allocation after fork.
[[noreturn]] void v0_child_exec(int error_fd, char* const* argv,
                              const sigset_t& old_mask, long max_fd) {
    if (sigprocmask(SIG_SETMASK, &old_mask, nullptr) < 0) {
        int e = errno; write_record(error_fd, {2, e}); _exit(125);
    }
    bool closed = false;
#ifdef SYS_close_range
    bool low = error_fd <= 3 || syscall(SYS_close_range, 3u, static_cast<unsigned>(error_fd - 1), 0u) == 0;
    bool high = syscall(SYS_close_range, static_cast<unsigned>(error_fd + 1), UINT_MAX, 0u) == 0;
    closed = low && high;
#endif
    if (!closed) for (long fd = 3; fd < max_fd; ++fd)
        if (fd != error_fd) close(static_cast<int>(fd));
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd < 0) { int e = errno; write_record(error_fd, {2, e}); _exit(125); }
    if (dup2(null_fd, STDIN_FILENO) < 0) {
        int e = errno; write_record(error_fd, {2, e}); _exit(125);
    }
    if (null_fd != STDIN_FILENO) close(null_fd);
    if (!write_record(error_fd, {1, 0})) _exit(125);
    execvp(argv[0], argv);
    int e = errno;
    write_record(error_fd, {3, e});
    _exit(e == ENOENT || e == ENOTDIR ? 127 : 126);
}
struct MaskGuard {
    sigset_t old{};
    bool active = false;
    ~MaskGuard() { if (active) sigprocmask(SIG_SETMASK, &old, nullptr); }
};
}
RunResult Runtime::run(const RunConfig& config) {
    RunResult result;
    result.run_id = std::to_string(getpid()) + "-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto fail = [&](const char* step, int e, const char* message) {
        if (!result.primary_error) result.primary_error = Error{step, e, message};
    };
    if (config.mode != RunMode::v0 || config.argv.empty() || config.argv[0].empty() ||
        !config.evidence_dir.is_absolute() || !config.rootfs.empty() ||
        !config.hostname.empty() || !config.cgroup_base.empty() || config.limits.memory_bytes ||
        config.limits.swap_max_bytes || config.limits.cpu_quota_us || config.limits.cpu_period_us != 100000) {
        fail("validate", 0, "S02 requires v0, nonempty argv, absolute evidence directory and no isolation/limits");
        return result;
    }
    for (const auto& arg : config.argv) if (arg.find('\0') != std::string::npos) {
        fail("validate", 0, "argv contains NUL"); return result;
    }
    std::cerr << "V0 非隔离运行；非完整 V1\n";
    std::vector<char*> args;
    for (const auto& arg : config.argv) args.push_back(const_cast<char*>(arg.c_str()));
    args.push_back(nullptr);
    sigset_t blocked; sigemptyset(&blocked); sigaddset(&blocked, SIGINT); sigaddset(&blocked, SIGTERM);
    MaskGuard mask;
    if (sigprocmask(SIG_BLOCK, &blocked, &mask.old) < 0) {
        int e = errno; fail("sync", e, "block signals"); return result;
    }
    mask.active = true;
    Fd signals(signalfd(-1, &blocked, SFD_CLOEXEC | SFD_NONBLOCK));
    if (signals.get() < 0) { int e = errno; fail("sync", e, "signalfd"); return result; }
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) < 0) { int e = errno; fail("pipe", e, "error pipe"); return result; }
    Fd reader(pipe_fds[0]), writer(pipe_fds[1]);
    // Keep the protocol descriptor above stdio even if caller closed stdio.
    int elevated = fcntl(writer.get(), F_DUPFD_CLOEXEC, 3);
    if (elevated < 0) { int e = errno; fail("pipe", e, "duplicate error writer"); return result; }
    writer.reset(elevated);
    long max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd < 0) { fail("validate", errno, "open descriptor limit"); return result; }
    pid_t pid = fork();
    if (pid < 0) { int e = errno; fail("fork", e, "fork"); return result; }
    if (pid == 0) v0_child_exec(writer.get(), args.data(), mask.old, max_fd);
    result.host_pid = pid;
    writer.reset();
    // No allocations until the direct child has been reaped.
    Record records[3]{};
    std::size_t received = 0;
    bool eof = false, killed = false, reaped = false;
    int status = 0, io_error = 0, wait_error = 0;
    while (!reaped || !eof) {
        pollfd fds[2] = {{reader.get(), POLLIN, 0}, {signals.get(), POLLIN, 0}};
        int ready = poll(fds, 2, 50);
        if (ready < 0 && errno != EINTR) { io_error = errno; eof = true; }
        if (fds[1].revents & POLLIN) {
            signalfd_siginfo notice{};
            ssize_t count;
            do { count = read(signals.get(), &notice, sizeof(notice)); } while (count < 0 && errno == EINTR);
            // Bounded termination of this direct child; handler-free normal control flow.
            if (count == sizeof(notice) && !reaped && !killed) {
                if (kill(pid, SIGKILL) < 0 && errno != ESRCH) io_error = errno;
                killed = true;
            }
        }
        if (!eof && (fds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            if (received == sizeof(records)) { io_error = EPROTO; eof = true; }
            else {
                ssize_t n = read(reader.get(), reinterpret_cast<char*>(records) + received, sizeof(records) - received);
                if (n > 0) received += static_cast<std::size_t>(n);
                else if (n == 0) eof = true;
                else if (errno != EINTR) { io_error = errno; eof = true; }
            }
        }
        if (io_error && !reaped && !killed) { kill(pid, SIGKILL); killed = true; }
        if (!reaped) {
            pid_t waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid) reaped = true;
            else if (waited < 0 && errno != EINTR) { wait_error = errno; reaped = true; eof = true; }
        }
    }
    reader.reset(); signals.reset();
    result.cleanup.attempted = true;
    result.cleanup.complete = wait_error == 0;
    if (wait_error) fail("wait", wait_error, "waitpid");
    else if (io_error || received % sizeof(Record)) fail("sync", io_error ? io_error : EPROTO, "invalid error pipe record");
    else {
        std::size_t count = received / sizeof(Record);
        bool prepared = count && records[0].stage == 1 && records[0].error == 0;
        const Record* failure = nullptr;
        if (count == 1 && records[0].stage == 2) failure = &records[0];
        else if (count == 2 && prepared && records[1].stage == 3) failure = &records[1];
        if (failure) fail(failure->stage == 3 ? "exec" : "sync", failure->error, std::strerror(failure->error));
        else if (!prepared || count != 1) fail("sync", EPROTO, "child died before exec preparation or invalid protocol");
        else if (WIFEXITED(status)) {
            result.kind = ResultKind::exited; result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.kind = ResultKind::signaled; result.signal = WTERMSIG(status);
        } else fail("wait", 0, "unexpected child status");
    }
    return result;
}
}
