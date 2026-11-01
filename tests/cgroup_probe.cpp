#include "minic/cgroup.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
std::uint64_t counter(const std::string& data, const std::string& key) {
    std::istringstream lines(data);
    std::string name;
    std::uint64_t value;
    while (lines >> name >> value) if (name == key) return value;
    return 0;
}
bool contains_pid(const std::filesystem::path& path, pid_t pid) {
    std::ifstream in(path / "cgroup.procs");
    pid_t found;
    while (in >> found) if (found == pid) return true;
    return false;
}
void print_error(const minic::Error& error) {
    std::cerr << error.step << ": " << error.message << " (errno=" << error.errno_value << ")\n";
}
int wait_child(pid_t pid) {
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3 || (std::strcmp(argv[2], "smoke") && std::strcmp(argv[2], "attach-fail") && std::strcmp(argv[2], "baseline") && std::strcmp(argv[2], "cpu") &&
                      std::strcmp(argv[2], "memory"))) {
        std::cerr << "usage: cgroup_probe ABSOLUTE_DELEGATED_BASE smoke|attach-fail|baseline|cpu|memory\n";
        return 2;
    }
    std::string mode = argv[2];
    minic::ResourceLimits limits;
    if (mode == "cpu") limits.cpu_quota_us = 20000;
    if (mode == "memory") {
        limits.memory_bytes = 64ULL * 1024 * 1024;
        limits.swap_max_bytes = 0;
    }
    std::string run_id = "minic-probe-" + std::to_string(::getpid());
    auto outcome = minic::CgroupSession::create(argv[1], run_id, limits);
    if (auto error = std::get_if<minic::Error>(&outcome)) {
        print_error(*error);
        return 1;
    }
    auto session = std::move(std::get<minic::CgroupSession>(outcome));
    int gate[2];
    if (::pipe(gate) != 0) {
        std::perror("pipe");
        return 1;
    }
    pid_t child = ::fork();
    if (child < 0) {
        std::perror("fork");
        ::close(gate[0]); ::close(gate[1]);
        return 1;
    }
    if (child == 0) {
        ::close(gate[1]);
        char byte;
        ssize_t n;
        do { n = ::read(gate[0], &byte, 1); } while (n < 0 && errno == EINTR);
        ::close(gate[0]);
        if (n != 1 || byte != 'g') _exit(80);
        if (mode == "cpu" || mode == "baseline") {
            auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            volatile std::uint64_t value = 0;
            while (std::chrono::steady_clock::now() < end) ++value;
            _exit(value == 0 ? 1 : 0);
        }
        if (mode == "memory") {
            constexpr std::size_t page = 4096;
            std::vector<char> data(96ULL * 1024 * 1024);
            volatile char* pages = data.data();
            for (std::size_t i = 0; i < data.size(); i += page) pages[i] = 1;
            _exit(0);
        }
        _exit(0);
    }
    ::close(gate[0]);
    if (mode == "attach-fail") {
        auto failure = session.attach(0);
        ::close(gate[1]);
        int blocked_exit = wait_child(child);
        auto cleanup = session.cleanup();
        if (auto error = std::get_if<minic::Error>(&cleanup)) print_error(*error);
        std::cout << "invalid attach rejected=" << std::holds_alternative<minic::Error>(failure)
                  << " blocked child exit=" << blocked_exit << '\n';
        return std::holds_alternative<minic::Error>(failure) && blocked_exit == 80 &&
               std::holds_alternative<std::monostate>(cleanup) ? 0 : 1;
    }
    auto attached = session.attach(child);
    if (auto error = std::get_if<minic::Error>(&attached)) {
        print_error(*error);
        ::close(gate[1]);
        std::cout << "blocked child exit=" << wait_child(child) << '\n';
        auto cleanup = session.cleanup();
        if (auto e = std::get_if<minic::Error>(&cleanup)) print_error(*e);
        return 1;
    }
    auto leaf = std::filesystem::path(argv[1]) / run_id;
    if (!contains_pid(leaf, child)) {
        std::cerr << "attached child is absent from cgroup.procs\n";
        ::close(gate[1]);
        (void)wait_child(child);
        (void)session.cleanup();
        return 1;
    }
    auto before = session.snapshot();
    if (auto error = std::get_if<minic::Error>(&before)) {
        print_error(*error);
        ::close(gate[1]);
        (void)wait_child(child);
        (void)session.cleanup();
        return 1;
    }
    std::cout << "child host PID=" << child << " cgroup=" << leaf << '\n';
    char go = 'g';
    if (::write(gate[1], &go, 1) != 1) {
        ::kill(child, SIGKILL);
        ::close(gate[1]);
        (void)wait_child(child);
        return 1;
    }
    ::close(gate[1]);
    auto start = std::chrono::steady_clock::now();
    int child_status = wait_child(child);
    auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "child exit=" << child_status << " wall_seconds=" << seconds << '\n';
    auto snapshot = session.snapshot();
    if (auto error = std::get_if<minic::Error>(&snapshot)) print_error(*error);
    else for (const auto& [name, data] : std::get<minic::CgroupSnapshot>(snapshot).files)
        std::cout << "=== " << name << " ===\n" << data;
    auto cleanup = session.cleanup();
    if (auto error = std::get_if<minic::Error>(&cleanup)) {
        print_error(*error);
        return 1;
    }
    if (std::holds_alternative<minic::Error>(snapshot)) return 1;
    const auto& first = std::get<minic::CgroupSnapshot>(before).files;
    const auto& last = std::get<minic::CgroupSnapshot>(snapshot).files;
    if (mode == "memory" &&
        (child_status != 128 + SIGKILL || counter(last.at("memory.events"), "oom_kill") <=
                                         counter(first.at("memory.events"), "oom_kill"))) {
        std::cerr << "memory probe: expected SIGKILL and an oom_kill increment\n";
        return 1;
    }
    if (mode == "cpu" && counter(last.at("cpu.stat"), "nr_throttled") <=
                         counter(first.at("cpu.stat"), "nr_throttled")) {
        std::cerr << "cpu probe: nr_throttled did not increase\n";
        return 1;
    }
    if ((mode == "smoke" || mode == "baseline" || mode == "cpu") && child_status != 0) return 1;
    return 0;
}
