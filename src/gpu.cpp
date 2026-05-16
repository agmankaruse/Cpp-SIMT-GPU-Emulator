#include "gpu.hpp"

#include <iostream>
#include <stdexcept>

namespace simt {

GPU::GPU(const GPUConfig& config)
    : config_(config), globalMemory_(config.globalMemoryBytes), trace_(false) {
    sms_.reserve(config_.smCount);
    for (std::size_t sm = 0; sm < config_.smCount; ++sm) {
        sms_.emplace_back(sm, config_);
    }
}

void GPU::loadProgram(const Program& program) {
    program_ = program;
    for (auto& sm : sms_) {
        sm.loadProgram(&program_);
    }
}

Stats GPU::run(bool traceEnabled, std::uint64_t maxCycles) {
    trace_ = Trace(traceEnabled);

    Stats stats;
    stats.residentWarps = config_.smCount * config_.warpsPerSM;
    stats.maxResidentWarps = stats.residentWarps;

    std::uint64_t cycle = 0;
    while (active() && cycle < maxCycles) {
        for (auto& sm : sms_) {
            sm.tick(cycle, globalMemory_, stats, &trace_);
        }
        ++cycle;
    }

    stats.totalCycles = cycle;
    if (cycle >= maxCycles && active()) {
        throw std::runtime_error("simulation exceeded max cycle limit");
    }
    return stats;
}

bool GPU::active() const {
    for (const auto& sm : sms_) {
        if (sm.active()) {
            return true;
        }
    }
    return false;
}

} // namespace simt
