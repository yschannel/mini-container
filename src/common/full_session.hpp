#pragma once
#include "minic/result.hpp"
#include <exception>

namespace minic::detail {
// Session is the existing CgroupSession in production; test doubles exercise ordering only.
template<class Session> class FullSession {
    std::optional<Session> session_;
public:
    std::optional<Error> prepare(const RunConfig& config, RunResult& result, pid_t pid) {
        auto created = Session::create(config.cgroup_base, result.run_id, config.limits);
        if (auto e = std::get_if<Error>(&created)) return *e;
        session_.emplace(std::move(std::get<Session>(created)));
        auto attached = session_->attach(pid);
        if (auto e = std::get_if<Error>(&attached)) return *e;
        result.enabled.cgroup = true;
        result.enabled.limits = config.limits;
        return std::nullopt;
    }
    // Caller must terminate/wait its child before invoking this, including attach failure.
    void finish(RunResult& result) {
        if (!session_) return;
        try {
            auto snapshot = session_->snapshot();
            if (auto e = std::get_if<Error>(&snapshot)) result.snapshot_error = *e;
            else result.snapshot = std::move(std::get<CgroupSnapshot>(snapshot));
        } catch (const std::exception& e) {
            result.snapshot_error = Error{"snapshot", 0, e.what()};
        }
        try {
            auto status = session_->cleanup();
            if (auto e = std::get_if<Error>(&status)) {
                result.cleanup.errors.push_back(*e);
                result.cleanup.complete = false;
            }
        } catch (const std::exception& e) {
            result.cleanup.errors.push_back({"cleanup", 0, e.what()});
            result.cleanup.complete = false;
        }
        if ((result.snapshot_error || !result.cleanup.complete) &&
            result.kind == ResultKind::exited && result.exit_code == 0)
            result.kind = ResultKind::cleanup_error;
    }
};
}
