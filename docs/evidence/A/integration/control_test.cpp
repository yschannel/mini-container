// Test double only: no Linux cgroup or namespace capability is asserted here.
#include "src/common/full_session.hpp"
#include "src/common/result_io.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace minic;
namespace {
std::string fault;
std::vector<std::string> events;
struct FakeSession {
    static Outcome<FakeSession> create(const std::filesystem::path&, const std::string&, const ResourceLimits&) {
        events.push_back("create");
        if (fault == "create") return Error{"cgroup.create", 5, "injected"};
        return FakeSession{};
    }
    Status attach(pid_t) {
        events.push_back("attach");
        if (fault == "attach") return Error{"cgroup.attach", 3, "injected"};
        return std::monostate{};
    }
    Outcome<CgroupSnapshot> snapshot() const {
        events.push_back("snapshot");
        if (fault == "snapshot" || fault == "both") return Error{"snapshot", 5, "injected"};
        return CgroupSnapshot{"/fake/run", {{"memory.events", "oom_kill 1\n"}, {"cpu.max", "20000 100000\n"}}};
    }
    Status cleanup() {
        events.push_back("cleanup");
        if (fault == "cleanup" || fault == "both") return Error{"cleanup", 16, "injected"};
        return std::monostate{};
    }
};
}
int main() {
    RunConfig config;
    config.cgroup_base = "/fake";
    config.evidence_dir = std::filesystem::absolute("artifacts/integration/json");
    config.limits.memory_bytes = 67108864; config.limits.swap_max_bytes = 0;
    config.limits.cpu_quota_us = 20000;
    for (const char* inject : {"", "create", "attach", "snapshot", "cleanup", "both"}) {
        fault = inject; events.clear();
        RunResult r; r.run_id = "control-test";
        detail::FullSession<FakeSession> session;
        auto error = session.prepare(config, r, getpid());
        if (error) r.primary_error = *error;
        else { events.push_back("release"); r.kind = ResultKind::exited; r.exit_code = 0; }
        events.push_back("wait"); r.cleanup = {true, true, {}};
        session.finish(r);
        if (fault == "create") assert((events == std::vector<std::string>{"create", "wait"}));
        else if (fault == "attach") {
            assert((events == std::vector<std::string>{"create", "attach", "wait", "snapshot", "cleanup"}));
            assert(r.primary_error->step == "cgroup.attach" && !r.enabled.cgroup);
        } else {
            assert((events == std::vector<std::string>{"create", "attach", "release", "wait", "snapshot", "cleanup"}));
            assert(r.enabled.cgroup && r.enabled.limits.cpu_quota_us == 20000);
            assert(r.kind == (fault.empty() ? ResultKind::exited : ResultKind::cleanup_error));
        }
        if (fault == "both") assert(r.snapshot_error && r.cleanup.errors.size() == 1 && !r.cleanup.complete);
        if (fault == "cleanup") assert(r.snapshot && !r.cleanup.complete);
        if (fault.empty()) {
            assert(detail::save_result(config,r));
            assert(detail::exit_status(r) == 0);
        }
    }
    // A nonzero payload or primary startup failure survives independent finishing errors.
    fault = "both";
    for (int code : {0, 7, 143}) {
        RunResult r; detail::FullSession<FakeSession> session;
        assert(!session.prepare(config,r,getpid()));
        r.cleanup = {true,true,{}};
        if (code == 0) { r.kind = ResultKind::startup_error; r.primary_error = Error{"exec",2,"missing"}; }
        else if (code == 7) { r.kind = ResultKind::exited; r.exit_code = 7; }
        else { r.kind = ResultKind::signaled; r.signal = 15; }
        session.finish(r);
        assert(detail::exit_status(r) == (code == 0 ? 127 : code));
        assert(r.snapshot_error && r.cleanup.errors.size() == 1);
    }
    std::cout << "PASS: 9 simulated lifecycle/error-preservation cases; no real cgroup validation\n";
}
