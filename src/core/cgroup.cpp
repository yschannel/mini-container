#include "minic/cgroup.hpp"

#include <cerrno>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <linux/magic.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

namespace minic {
namespace {
Error error(const std::string& step, const std::string& what, int number = errno) {
    return {step, number, what + (number ? ": " + std::string(std::strerror(number)) : "")};
}
Status write_file(const std::filesystem::path& path, const std::string& value,
                  const std::string& step) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return error(step, "open " + path.string());
    std::size_t offset = 0;
    while (offset < value.size()) {
        ssize_t n = ::write(fd, value.data() + offset, value.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            auto e = error(step, "write " + path.string(), n == 0 ? EIO : errno);
            ::close(fd);
            return e;
        }
        offset += static_cast<std::size_t>(n);
    }
    if (::close(fd) != 0) return error(step, "close " + path.string());
    return std::monostate{};
}
Outcome<std::string> read_file(const std::filesystem::path& path, const std::string& step) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return error(step, "open " + path.string());
    std::string value;
    char buffer[4096];
    for (;;) {
        ssize_t n = ::read(fd, buffer, sizeof buffer);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            auto e = error(step, "read " + path.string());
            ::close(fd);
            return e;
        }
        if (n == 0) break;
        value.append(buffer, static_cast<std::size_t>(n));
    }
    if (::close(fd) != 0) return error(step, "close " + path.string());
    return value;
}
bool safe_component(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.') return false;
    return true;
}
bool contains_token(const std::string& content, const std::string& token) {
    std::istringstream words(content);
    std::string word;
    while (words >> word) if (word == token) return true;
    return false;
}
} // namespace

struct CgroupSession::Impl {
    std::filesystem::path path;
    bool owns = true;
    bool swap_requested = false;
};

CgroupSession::CgroupSession(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
CgroupSession::CgroupSession(CgroupSession&&) noexcept = default;
CgroupSession::~CgroupSession() noexcept {
    try { if (impl_ && impl_->owns) (void)cleanup(); }
    catch (...) { /* explicit cleanup is the reportable path */ }
}

Outcome<CgroupSession> CgroupSession::create(const std::filesystem::path& base,
                                             const std::string& run_id,
                                             const ResourceLimits& limits) {
    if (!base.is_absolute() || !safe_component(run_id))
        return error("cgroup.create", "base must be absolute and run_id a safe component", 0);
    if (limits.memory_bytes && *limits.memory_bytes == 0)
        return error("cgroup.create", "memory limit must be positive", 0);
    if (limits.cpu_quota_us && (*limits.cpu_quota_us == 0 || limits.cpu_period_us < 1000 ||
                                limits.cpu_period_us > 1000000))
        return error("cgroup.create", "invalid CPU quota or period", 0);
    struct stat st{};
    if (::lstat(base.c_str(), &st) != 0) return error("cgroup.create", "stat base " + base.string());
    if (!S_ISDIR(st.st_mode)) return error("cgroup.create", "base is not a directory", 0);
    struct statfs fs{};
    if (::statfs(base.c_str(), &fs) != 0) return error("cgroup.create", "statfs base " + base.string());
    if (fs.f_type != CGROUP2_SUPER_MAGIC)
        return error("cgroup.create", "base is not on cgroup v2", 0);
    auto ctrls = read_file(base / "cgroup.controllers", "cgroup.create");
    if (auto e = std::get_if<Error>(&ctrls)) return *e;
    auto available = std::get<std::string>(ctrls);
    if ((limits.memory_bytes || limits.swap_max_bytes) && !contains_token(available, "memory"))
        return error("cgroup.create", "memory controller unavailable", 0);
    if (limits.cpu_quota_us && !contains_token(available, "cpu"))
        return error("cgroup.create", "cpu controller unavailable", 0);
    auto enabled = read_file(base / "cgroup.subtree_control", "cgroup.create");
    if (auto e = std::get_if<Error>(&enabled)) return *e;
    auto subtree = std::get<std::string>(enabled);
    if ((limits.memory_bytes || limits.swap_max_bytes) && !contains_token(subtree, "memory"))
        return error("cgroup.create", "memory controller not enabled for children", 0);
    if (limits.cpu_quota_us && !contains_token(subtree, "cpu"))
        return error("cgroup.create", "cpu controller not enabled for children", 0);
    auto path = base / run_id;
    if (::mkdir(path.c_str(), 0755) != 0) return error("cgroup.create", "mkdir " + path.string());
    CgroupSession session(std::unique_ptr<Impl>(new Impl{path, true, limits.swap_max_bytes.has_value()}));
    auto set = [&](const char* file, const std::string& value) -> Status {
        auto status = write_file(path / file, value, "cgroup.create");
        if (std::holds_alternative<Error>(status)) return status;
        auto actual = read_file(path / file, "cgroup.create");
        if (auto e = std::get_if<Error>(&actual)) return *e;
        std::istringstream expected_words(value), actual_words(std::get<std::string>(actual));
        std::string wanted, got;
        while (expected_words >> wanted) {
            if (!(actual_words >> got) || got != wanted)
                return error("cgroup.create", "limit readback differs at " + (path / file).string(), 0);
        }
        return std::monostate{};
    };
    Status status = std::monostate{};
    if (limits.memory_bytes) status = set("memory.max", std::to_string(*limits.memory_bytes));
    if (std::holds_alternative<std::monostate>(status) && limits.swap_max_bytes)
        status = set("memory.swap.max", std::to_string(*limits.swap_max_bytes));
    if (std::holds_alternative<std::monostate>(status) && limits.cpu_quota_us)
        status = set("cpu.max", std::to_string(*limits.cpu_quota_us) + " " + std::to_string(limits.cpu_period_us));
    if (auto e = std::get_if<Error>(&status)) {
        Error original = *e;
        auto rollback = session.cleanup();
        if (auto r = std::get_if<Error>(&rollback)) original.message += "; rollback: " + r->message;
        return original;
    }
    return session;
}

Status CgroupSession::attach(pid_t host_pid) {
    if (!impl_ || !impl_->owns || host_pid <= 0)
        return error("cgroup.attach", "invalid session or PID", 0);
    auto status = write_file(impl_->path / "cgroup.procs", std::to_string(host_pid), "cgroup.attach");
    if (std::holds_alternative<Error>(status)) return status;
    auto members = read_file(impl_->path / "cgroup.procs", "cgroup.attach");
    if (auto e = std::get_if<Error>(&members)) return *e;
    if (!contains_token(std::get<std::string>(members), std::to_string(host_pid)))
        return error("cgroup.attach", "PID absent after attach", 0);
    return std::monostate{};
}

Outcome<CgroupSnapshot> CgroupSession::snapshot() const {
    if (!impl_ || !impl_->owns) return error("snapshot", "session already cleaned", 0);
    CgroupSnapshot snapshot;
    snapshot.path = impl_->path;
    for (const char* name : {"memory.events", "memory.current", "cpu.stat", "cpu.max", "memory.max", "cgroup.events"}) {
        auto data = read_file(impl_->path / name, "snapshot");
        if (auto e = std::get_if<Error>(&data)) return *e;
        snapshot.files[name] = std::get<std::string>(std::move(data));
    }
    auto swap = read_file(impl_->path / "memory.swap.max", "snapshot");
    if (auto data = std::get_if<std::string>(&swap)) snapshot.files["memory.swap.max"] = *data;
    else if (impl_->swap_requested || std::get<Error>(swap).errno_value != ENOENT) return std::get<Error>(swap);
    return snapshot;
}

Status CgroupSession::cleanup() {
    if (!impl_ || !impl_->owns) return std::monostate{};
    if (::rmdir(impl_->path.c_str()) != 0) return error("cleanup", "rmdir " + impl_->path.string());
    impl_->owns = false;
    return std::monostate{};
}
} // namespace minic
