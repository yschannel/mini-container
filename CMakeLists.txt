cmake_minimum_required(VERSION 3.16)
project(mini_container VERSION 0.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(minic src/main_cli.cpp)
target_include_directories(minic PRIVATE include)
target_compile_options(minic PRIVATE -Wall -Wextra -Wpedantic)
# S00 后由 B 维护。Runtime/cgroup 尚无实现，不链接虚假成功桩。
