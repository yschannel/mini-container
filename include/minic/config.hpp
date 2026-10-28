#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace minic {
enum class RunMode { v0, isolate, full };
struct ResourceLimits {
    std::optional<std::uint64_t> memory_bytes;
    std::optional<std::uint64_t> swap_max_bytes;
    std::optional<std::uint64_t> cpu_quota_us;
    std::uint64_t cpu_period_us = 100000;
};
struct RunConfig {
    RunMode mode = RunMode::full;
    std::filesystem::path rootfs;
    std::string hostname;
    std::vector<std::string> argv;
    ResourceLimits limits;
    std::filesystem::path cgroup_base;
    std::filesystem::path evidence_dir;
};
} // namespace minic
