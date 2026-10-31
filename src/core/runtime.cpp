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
#include <sched.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <fstream>

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
struct IsolateContext {
    int control_fd, error_fd;
    char* const* argv;
    const sigset_t* mask;
    long max_fd;
    const char* rootfs;
    const char* hostname;
    std::size_t hostname_size;
};
int isolate_child_entry(void* opaque) {
    const auto& c = *static_cast<IsolateContext*>(opaque);
    auto failure = [&](int stage) {
        int e = errno; write_record(c.error_fd, {stage, e}); _exit(125);
    };
    // Close host directory and unrelated descriptors before chroot; retain only protocol ends.
    for (long fd = 3; fd < c.max_fd; ++fd)
        if (fd != c.control_fd && fd != c.error_fd) close(static_cast<int>(fd));
    char token = 0;
    ssize_t n;
    do { n = read(c.control_fd, &token, 1); } while (n < 0 && errno == EINTR);
    if (n != 1 || token != 'G') {
        int e = n < 0 ? errno : EPROTO; write_record(c.error_fd, {4, e}); _exit(125);
    }
    close(c.control_fd);
    // Open stdin before entering a minimal Rootfs, which need not contain device nodes.
    int null_fd = open("/dev/null", O_RDONLY);
    if (null_fd < 0) failure(2);
    if (dup2(null_fd, STDIN_FILENO) < 0) failure(2);
    if (null_fd != STDIN_FILENO) close(null_fd);
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) < 0) failure(5);
    if (sethostname(c.hostname, c.hostname_size) < 0) failure(6);
    if (chroot(c.rootfs) < 0) failure(7);
    if (chdir("/") < 0) failure(7);
    if (!write_record(c.error_fd, {10, 0})) _exit(125);
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) < 0) failure(8);
    if (sigprocmask(SIG_SETMASK, c.mask, nullptr) < 0) failure(2);
    if (!write_record(c.error_fd, {1, 0})) _exit(125);
    execvp(c.argv[0], c.argv);
    int e = errno; write_record(c.error_fd, {3, e});
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
    const bool isolate = config.mode == RunMode::isolate;
    if ((config.mode != RunMode::v0 && !isolate) || config.argv.empty() || config.argv[0].empty() ||
        !config.evidence_dir.is_absolute() || !config.cgroup_base.empty() || config.limits.memory_bytes ||
        config.limits.swap_max_bytes || config.limits.cpu_quota_us || config.limits.cpu_period_us != 100000 ||
        (!isolate && (!config.rootfs.empty() || !config.hostname.empty()))) {
        fail("validate", 0, "requires v0/isolate, argv, absolute evidence directory and no resource limits");
        return result;
    }
    for (const auto& arg : config.argv) if (arg.find('\0') != std::string::npos) {
        fail("validate", 0, "argv contains NUL"); return result;
    }
    std::string rootfs;
    if (isolate) {
        if (!config.rootfs.is_absolute() || config.hostname.empty() || config.hostname.size() > 64 ||
            config.hostname.find('\0') != std::string::npos) {
            fail("validate", 0, "isolate requires absolute rootfs and hostname of 1..64 bytes"); return result;
        }
        std::error_code ec;
        auto canonical = std::filesystem::canonical(config.rootfs, ec);
        if (ec) { fail("rootfs", ec.value(), "cannot resolve Rootfs"); return result; }
        std::ifstream marker(canonical / ".minic-rootfs");
        std::string tag; std::getline(marker, tag);
        if (canonical == "/" || tag != "minic-rootfs-v1" ||
            !std::filesystem::is_directory(canonical / "proc", ec) || ec ||
            std::filesystem::is_symlink(canonical / "proc", ec) || ec) {
            fail("rootfs", 0, "requires dedicated marked Rootfs and real proc directory"); return result;
        }
        rootfs = canonical.string();
    }
    std::cerr << (isolate ? "isolate PID/UTS/Mount/Rootfs；无 cgroup，非完整 V1\n" : "V0 非隔离运行；非完整 V1\n");
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
    // socketpair is the parent-to-child control pipe; MSG_NOSIGNAL avoids process-wide SIGPIPE changes.
    Fd control_parent, control_child;
    if (isolate) {
        int control[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, control) < 0) {
            int e = errno; fail("pipe", e, "control channel"); return result;
        }
        control_parent.reset(control[0]); control_child.reset(control[1]);
    }
    std::vector<char> clone_stack(isolate ? 1024 * 1024 : 0);
    IsolateContext context{control_child.get(), writer.get(), args.data(), &mask.old, max_fd,
                           rootfs.c_str(), config.hostname.c_str(), config.hostname.size()};
    pid_t pid = isolate ? clone(isolate_child_entry, clone_stack.data() + clone_stack.size(),
                               CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, &context) : fork();
    if (pid < 0) { int e = errno; fail(isolate ? "clone" : "fork", e, "create child"); return result; }
    if (!isolate && pid == 0) v0_child_exec(writer.get(), args.data(), mask.old, max_fd);
    result.host_pid = pid;
    writer.reset();
    control_child.reset();
    int release_error = 0;
    if (isolate) {
        result.enabled.pid_namespace = result.enabled.uts_namespace = result.enabled.mount_namespace = true;
        ssize_t n;
        do { n = send(control_parent.get(), "G", 1, MSG_NOSIGNAL); } while (n < 0 && errno == EINTR);
        if (n != 1) { release_error = n < 0 ? errno : EIO; kill(pid, SIGKILL); }
        control_parent.reset();
    }
    // No allocations until the direct child has been reaped.
    Record records[8]{};
    std::size_t received = 0;
    bool eof = false, killed = false, reaped = false;
    int status = 0, io_error = release_error, wait_error = 0;
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
        bool prepared = false, protocol_error = false;
        const Record* failure = nullptr;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& record = records[i];
            if (record.stage == 10 && isolate && i == 0 && record.error == 0) result.enabled.rootfs = true;
            else if (record.stage == 1 && !prepared && !failure && record.error == 0 &&
                     i == (isolate ? 1u : 0u)) prepared = true;
            else if (record.stage >= 2 && record.stage <= 8 && !failure && record.error > 0 &&
                     i + 1 == count && ((record.stage == 3) == prepared)) failure = &record;
            else protocol_error = true;
        }
        const char* steps[] = {"sync", "sync", "sync", "exec", "sync", "mount.private", "hostname", "rootfs", "proc"};
        if (protocol_error) fail("sync", EPROTO, "invalid child protocol");
        else if (failure) fail(steps[failure->stage], failure->error, std::strerror(failure->error));
        else if (!prepared) fail("sync", EPROTO, "child died before exec preparation");
        else if (WIFEXITED(status)) {
            result.kind = ResultKind::exited; result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.kind = ResultKind::signaled; result.signal = WTERMSIG(status);
        } else fail("wait", 0, "unexpected child status");
    }
    return result;
}
}
