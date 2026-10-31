#include "result_io.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>
namespace minic::detail {
namespace {
std::string quote(const std::string& s) {
    std::ostringstream out; out << '"';
    const char* hex = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << c;
    }
    out << '"'; return out.str();
}
void error(std::ostream& o, const Error& e) {
    o << "{\"step\":" << quote(e.step) << ",\"errno_value\":" << e.errno_value
      << ",\"message\":" << quote(e.message) << '}';
}
void optional_error(std::ostream& o, const std::optional<Error>& e) { if (e) error(o, *e); else o << "null"; }
template<class T> void number(std::ostream& o, const std::optional<T>& n) { if (n) o << *n; else o << "null"; }
}
int exit_status(const RunResult& r) {
    if (r.kind == ResultKind::exited) return r.exit_code.value_or(125);
    if (r.kind == ResultKind::signaled) return 128 + r.signal.value_or(0);
    if (r.primary_error && r.primary_error->step == "exec")
        return r.primary_error->errno_value == ENOENT || r.primary_error->errno_value == ENOTDIR ? 127 : 126;
    return 125;
}
bool save_result(const RunConfig& c, RunResult& r) {
    auto failed = [&](const std::string& message, int e) {
        r.evidence_errors.push_back({"evidence", e, message});
        if (r.kind == ResultKind::exited && r.exit_code == 0) r.kind = ResultKind::cleanup_error;
        std::cerr << "evidence: " << message << '\n'; return false;
    };
    if (!c.evidence_dir.is_absolute()) return failed("absolute evidence directory required", 0);
    std::error_code ec;
    std::filesystem::create_directories(c.evidence_dir, ec);
    if (ec) return failed(ec.message(), ec.value());
    std::ofstream out(c.evidence_dir / "result.json", std::ios::trunc);
    if (!out) return failed("cannot open result.json", errno);
    const char* kind = r.kind == ResultKind::exited ? "exited" : r.kind == ResultKind::signaled ? "signaled" :
        r.kind == ResultKind::cleanup_error ? "cleanup_error" : "startup_error";
    const char* mode = c.mode == RunMode::v0 ? "v0" : c.mode == RunMode::isolate ? "isolate" : "full";
    out << "{\"schema_version\":1,\"run_id\":" << quote(r.run_id) << ",\"mode\":" << quote(mode)
        << ",\"kind\":" << quote(kind) << ",\"host_pid\":";
    number(out, r.host_pid); out << ",\"exit_code\":"; number(out, r.exit_code);
    out << ",\"signal\":"; number(out, r.signal);
    out << ",\"primary_error\":"; optional_error(out, r.primary_error);
    out << ",\"snapshot_error\":"; optional_error(out, r.snapshot_error);
    out << ",\"enabled\":{\"pid_namespace\":" << (r.enabled.pid_namespace ? "true" : "false")
        << ",\"uts_namespace\":" << (r.enabled.uts_namespace ? "true" : "false")
        << ",\"mount_namespace\":" << (r.enabled.mount_namespace ? "true" : "false")
        << ",\"rootfs\":" << (r.enabled.rootfs ? "true" : "false") << ",\"cgroup\":false,"
           "\"limits\":{\"memory_bytes\":null,\"swap_max_bytes\":null,\"cpu_quota_us\":null,\"cpu_period_us\":100000}},"
           "\"cleanup\":{\"attempted\":" << (r.cleanup.attempted ? "true" : "false")
        << ",\"complete\":" << (r.cleanup.complete ? "true" : "false") << ",\"errors\":[";
    for (std::size_t i = 0; i < r.cleanup.errors.size(); ++i) { if (i) out << ','; error(out, r.cleanup.errors[i]); }
    out << "]},\"snapshot\":null,\"evidence_errors\":[]}\n";
    out.close();
    if (!out) return failed("cannot finish result.json", errno);
    return true;
}
}
