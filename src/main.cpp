#include "gpu.hpp"
#include "invariant_checker.hpp"
#include "occupancy.hpp"
#include "parser.hpp"
#include "reference_gpu.hpp"

#include <exception>
#include <filesystem>
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
        << "  --timeline PATH             Write cycle timeline CSV\n"
        << "  --config PATH               Load GPU architecture config file\n"
        << "  --occupancy                 Print occupancy report before running\n"
        << "  --grid N                    Number of CTAs / blocks\n"
        << "  --block N                   Threads per CTA / block\n"
        << "  --sms N                     Number of streaming multiprocessors\n"
        << "  --warps-per-sm N            Resident warps per SM\n"
        << "  --lanes N                   SIMT lanes per warp\n"
        << "  --scheduler NAME            round_robin, greedy_then_oldest, oldest_ready\n"
        << "  --global-latency N          Global memory latency in cycles\n"
        << "  --shared-latency N          Shared memory latency in cycles\n"
        << "  --diff                      Compare emulator output against reference interpreter\n"
        << "  --check-invariants          Run invariant checks after execution\n"
        << "  --dump-on-fail              Dump GPU state when diff or invariant checks fail\n"
        << "  --version                   Print simulator version\n";
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
        bool occupancyMode = false;
        bool diffMode = false;
        bool checkInvariants = false;
        bool dumpOnFail = false;
        std::string timelinePath;
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
            } else if (arg == "--diff") {
                diffMode = true;
            } else if (arg == "--check-invariants") {
                checkInvariants = true;
            } else if (arg == "--dump-on-fail") {
                dumpOnFail = true;
            } else if (arg == "--version") {
                std::cout << "simt_gpu 0.1.0\n";
                return 0;
            } else if (arg == "--timeline") {
                timelinePath = requireValue(arg);
            } else if (arg == "--config") {
                config = simt::loadGPUConfigFile(requireValue(arg), config);
            } else if (arg == "--occupancy") {
                occupancyMode = true;
            } else if (arg == "--grid") {
                config.gridDim = parseSize(requireValue(arg), arg);
            } else if (arg == "--block") {
                config.blockDim = parseSize(requireValue(arg), arg);
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
            } else if (arg == "--cache-size") {
                config.cacheSizeBytes = parseSize(requireValue(arg), arg);
            } else if (arg == "--cache-line-size") {
                config.cacheLineBytes = parseSize(requireValue(arg), arg);
                config.memoryLineBytes = config.cacheLineBytes;
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
        if (occupancyMode) {
            simt::calculateOccupancy(config, program).print(std::cout);
        }

        if (diffMode) {
            const auto result = simt::runGpuDifferentialTest(program, config);
            std::cout << simt::formatGpuDiffResult(result);
            if (!result.passed && dumpOnFail) {
                simt::GPU dumpGpu(config);
                simt::initializeGlobalMemory(dumpGpu.globalMemory());
                dumpGpu.loadProgram(program);
                dumpGpu.run(false, 1'000'000, {});
                std::cout << dumpGpu.dumpSMState()
                          << dumpGpu.dumpWarpState()
                          << dumpGpu.dumpScoreboard()
                          << dumpGpu.dumpDivergenceStack()
                          << dumpGpu.dumpMemoryQueue()
                          << dumpGpu.dumpSharedMemorySummary()
                          << dumpGpu.dumpSchedulerState();
            }
            return result.passed ? 0 : 1;
        }

        if (!timelinePath.empty()) {
            const auto parent = std::filesystem::path(timelinePath).parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
        }

        simt::GPU gpu(config);

        simt::initializeGlobalMemory(gpu.globalMemory());

        gpu.loadProgram(program);
        const simt::Stats stats = gpu.run(traceEnabled, 1'000'000, timelinePath);
        if (checkInvariants) {
            const auto report = simt::InvariantChecker::check(gpu);
            std::cout << report.summary();
            if (!report.passed()) {
                if (dumpOnFail) {
                    std::cout << gpu.dumpSMState()
                              << gpu.dumpWarpState()
                              << gpu.dumpScoreboard()
                              << gpu.dumpDivergenceStack()
                              << gpu.dumpMemoryQueue()
                              << gpu.dumpSharedMemorySummary()
                              << gpu.dumpSchedulerState();
                }
                return 1;
            }
        }

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
