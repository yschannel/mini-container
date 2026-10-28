#include "minic/runtime.hpp"
#include "minic/cgroup.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "minic: S00 skeleton only (C++17)\n"
                     "Usage: minic --help | --version\n"
                     "Runtime, run command, namespaces, Rootfs and cgroup are not implemented.\n";
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "minic 0.0.0 (S00 skeleton)\n";
        return 0;
    }
    std::cerr << "S00 skeleton: only --help and --version are supported\n";
    return 125;
}
