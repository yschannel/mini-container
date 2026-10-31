#include "minic/runtime.hpp"
#include "common/result_io.hpp"
#include <iostream>
#include <string>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Usage: minic run --mode v0|isolate --rootfs PATH --hostname NAME --evidence-dir PATH -- PROGRAM ARGS...\n"
                         "S02: V0 非隔离运行，非完整 V1; stdin=/dev/null.\n"
                         "isolate supports PID/UTS/Mount/Rootfs; full/cgroup unavailable.\n"; return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "minic 0.0.0 (S02 V0)\n"; return 0;
        }
        minic::RunConfig config;
        std::string parse_error;
        int i = 2;
        if (argc < 2 || std::string(argv[1]) != "run") parse_error = "expected run";
        for (; parse_error.empty() && i < argc; ++i) {
            std::string option = argv[i];
            if (option == "--") { ++i; break; }
            if (i + 1 >= argc) { parse_error = "missing option value"; break; }
            if (option == "--mode") {
                std::string mode = argv[++i];
                if (mode == "v0") config.mode = minic::RunMode::v0;
                else if (mode == "isolate") config.mode = minic::RunMode::isolate;
                else if (mode == "full") config.mode = minic::RunMode::full;
                else parse_error = "invalid mode";
            } else if (option == "--evidence-dir") config.evidence_dir = std::filesystem::absolute(argv[++i]);
            else if (option == "--rootfs") config.rootfs = argv[++i];
            else if (option == "--hostname") config.hostname = argv[++i];
            else parse_error = "unsupported option: " + option;
        }
        for (; i < argc; ++i) config.argv.emplace_back(argv[i]);
        minic::RunResult result;
        if (!parse_error.empty()) result.primary_error = minic::Error{"validate", 0, parse_error};
        else result = minic::Runtime{}.run(config);
        if (result.primary_error) std::cerr << result.primary_error->step << ": " << result.primary_error->message
            << " (errno=" << result.primary_error->errno_value << ")\n";
        bool saved = minic::detail::save_result(config, result);
        int status = minic::detail::exit_status(result);
        return !saved && status == 0 ? 125 : status;
    } catch (const std::exception& e) {
        std::cerr << "minic: " << e.what() << '\n'; return 125;
    }
}
