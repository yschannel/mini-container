#pragma once
#include "minic/result.hpp"
namespace minic {
class Runtime {
public:
    // Declaration only; synchronous, owns the complete run lifecycle.
    RunResult run(const RunConfig& config);
};
} // namespace minic
