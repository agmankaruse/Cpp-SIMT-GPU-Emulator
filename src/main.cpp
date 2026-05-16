#include "gpu.hpp"
#include "parser.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void printUsage(const char* exe) {
    std::cerr
        << "Usage: " << exe << " [options] program.gpuasm\n\n"
        << "Options:\n"
        << "  --trace                     Print cycle-by-cycle issue trace\n"
        << "  --sms N                     Number of streaming multiprocessors\n"
        << "  --warps-per-sm N            Resident warps per SM\n"
        << "  --lanes N                   SIMT lanes per warp\n"
        << "  --scheduler NAME            round-robin or oldest-ready\n"
        << "  --global-latency N          Global memory latency in cycles\n"
        << "  --shared-latency N          Shared memory latency in cycles\n";
}

std::size_t parseSize(const std::string& value, const std::string& option) {
    try {
        return static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + option + ": " + value);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        simt::GPUConfig config;
        bool traceEnabled = false;
        std::string programPath;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto requireValue = [&](const std::string& option) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error(option + " requires a value");
                }
                return argv[++i];
            };

            if (arg == "--trace") {
                traceEnabled = true;
            } else if (arg == "--sms") {
                config.smCount = parseSize(requireValue(arg), arg);
            } else if (arg == "--warps-per-sm") {
                config.warpsPerSM = parseSize(requireValue(arg), arg);
            } else if (arg == "--lanes") {
                config.lanesPerWarp = parseSize(requireValue(arg), arg);
            } else if (arg == "--scheduler") {
                config.schedulerPolicy = simt::schedulerPolicyFromString(requireValue(arg));
            } else if (arg == "--global-latency") {
                config.globalMemoryLatency = parseSize(requireValue(arg), arg);
            } else if (arg == "--shared-latency") {
                config.sharedMemoryLatency = parseSize(requireValue(arg), arg);
            } else if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else if (!arg.empty() && arg[0] == '-') {
                throw std::runtime_error("unknown option: " + arg);
            } else {
                programPath = arg;
            }
        }

        if (programPath.empty()) {
            printUsage(argv[0]);
            return 1;
        }

        simt::Program program = simt::Parser::parseFile(programPath);
        simt::GPU gpu(config);

        for (std::uint32_t address = 0; address + 4 <= config.globalMemoryBytes; address += 4) {
            gpu.globalMemory().write32(address, static_cast<std::int32_t>(address / 4));
        }

        gpu.loadProgram(program);
        const simt::Stats stats = gpu.run(traceEnabled);

        if (traceEnabled) {
            gpu.trace().print(std::cout);
        }
        stats.print(std::cout, config.memoryLineBytes / 4);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
