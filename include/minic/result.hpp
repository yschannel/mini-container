#pragma once
#include "minic/config.hpp"
#include <sys/types.h>
#include <map>
#include <variant>

namespace minic {
struct Error {
    std::string step;
    int errno_value = 0;
    std::string message;
};
template<class T> using Outcome = std::variant<T, Error>;
using Status = Outcome<std::monostate>;
struct CgroupSnapshot {
    std::filesystem::path path;
    // Raw file contents, before removal. Optional swap file may be absent.
    std::map<std::string, std::string> files;
};
struct EnabledFeatures {
    bool pid_namespace = false;
    bool uts_namespace = false;
    bool mount_namespace = false;
    bool rootfs = false;
    bool cgroup = false;
    ResourceLimits limits;
};
struct CleanupResult {
    bool attempted = false;
    bool complete = false;
    std::vector<Error> errors;
};
enum class ResultKind { exited, signaled, startup_error, cleanup_error };
struct RunResult {
    ResultKind kind = ResultKind::startup_error;
    std::optional<int> exit_code;
    std::optional<int> signal;
    std::optional<Error> primary_error;
    std::optional<Error> snapshot_error;
    std::optional<pid_t> host_pid;
    std::string run_id;
    EnabledFeatures enabled;
    CleanupResult cleanup;
    std::optional<CgroupSnapshot> snapshot;
    std::vector<Error> evidence_errors;
};
} // namespace minic
