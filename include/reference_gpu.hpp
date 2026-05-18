#pragma once

#include "config.hpp"
#include "global_memory.hpp"
#include "parser.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace simt {

struct ReferenceThreadState {
    std::vector<std::int32_t> registers;
    std::vector<bool> predicates;
};

struct ReferenceGpuState {
    GlobalMemory globalMemory;
    std::vector<ReferenceThreadState> threads;
    std::uint64_t instructionCount = 0;
};

class ReferenceGPU {
public:
    explicit ReferenceGPU(const GPUConfig& config = GPUConfig{});

    void loadProgram(const Program& program);
    void initializeGlobalMemory();
    void run(std::uint64_t maxInstructions = 1'000'000);

    const ReferenceGpuState& state() const { return state_; }
    const GlobalMemory& globalMemory() const { return state_.globalMemory; }
    std::int32_t threadRegister(std::size_t threadId, std::size_t reg) const;

private:
    void executeThread(std::size_t threadId, std::size_t ctaId, std::size_t localThreadId);

    GPUConfig config_;
    Program program_;
    ReferenceGpuState state_;
};

struct GpuDiffMismatch {
    std::string location;
    std::int32_t emulatorValue = 0;
    std::int32_t referenceValue = 0;
    std::size_t threadId = 0;
    std::size_t laneId = 0;
    std::size_t warpId = 0;
};

struct GpuDiffResult {
    std::string kernelName;
    bool passed = false;
    std::vector<GpuDiffMismatch> mismatches;
    std::uint64_t cycleCount = 0;
    std::uint64_t emulatorInstructionCount = 0;
    std::uint64_t referenceInstructionCount = 0;
};

void initializeGlobalMemory(GlobalMemory& memory);
GpuDiffResult runGpuDifferentialTest(const Program& program, const GPUConfig& config = GPUConfig{});
GpuDiffResult runGpuDifferentialTestFromFile(const std::string& path,
                                             const GPUConfig& config = GPUConfig{});
std::string formatGpuDiffResult(const GpuDiffResult& result);

} // namespace simt
