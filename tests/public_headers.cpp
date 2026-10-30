#include "minic/config.hpp"
#include "minic/result.hpp"
#include "minic/runtime.hpp"
#include "minic/cgroup.hpp"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<minic::CgroupSession>);
static_assert(std::is_nothrow_move_constructible_v<minic::CgroupSession>);
static_assert(!std::is_move_assignable_v<minic::CgroupSession>);

// Compile only: the runtime and cgroup methods have no definitions in S01.
