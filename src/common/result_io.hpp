#pragma once
#include "minic/result.hpp"
namespace minic::detail {
int exit_status(const RunResult& result);
bool save_result(const RunConfig& config, RunResult& result);
}
