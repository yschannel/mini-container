#pragma once
#include "minic/result.hpp"
#include <memory>
namespace minic {
class CgroupSession {
public:
    static Outcome<CgroupSession> create(const std::filesystem::path& base,
                                        const std::string& run_id,
                                        const ResourceLimits& limits);
    CgroupSession(const CgroupSession&) = delete;
    CgroupSession& operator=(const CgroupSession&) = delete;
    CgroupSession(CgroupSession&&) noexcept;
    CgroupSession& operator=(CgroupSession&&) = delete;
    ~CgroupSession() noexcept;
    Status attach(pid_t host_pid);
    Outcome<CgroupSnapshot> snapshot() const;
    Status cleanup();
private:
    struct Impl;
    explicit CgroupSession(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace minic
