#include "gpu.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    configureLaunchWarps();
    for (auto& sm : sms_) {
        sm.loadProgram(&program_);
    }
}

Stats GPU::run(bool traceEnabled, std::uint64_t maxCycles, const std::string& timelinePath) {
    trace_ = Trace(traceEnabled, timelinePath);

    Stats stats;
    for (const auto& sm : sms_) {
        stats.residentWarps += sm.warpCount();
    }
    stats.maxResidentWarps =
        std::max<std::uint64_t>(stats.residentWarps, config_.smCount * config_.warpsPerSM);

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
    trace_.writeTimelineCsv();
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

void GPU::configureLaunchWarps() {
    const std::size_t gridDim = config_.gridDim == 0 ? config_.smCount : config_.gridDim;
    const std::size_t blockDim = config_.blockDim == 0
                                     ? config_.warpsPerSM * config_.lanesPerWarp
                                     : config_.blockDim;
    const std::size_t warpsPerCTA =
        (blockDim + config_.lanesPerWarp - 1) / config_.lanesPerWarp;

    std::vector<std::vector<Warp>> smWarps(config_.smCount);
    std::size_t globalWarpId = 0;
    for (std::size_t cta = 0; cta < gridDim; ++cta) {
        const std::size_t smIndex = cta % config_.smCount;
        for (std::size_t blockWarp = 0; blockWarp < warpsPerCTA; ++blockWarp) {
            const std::size_t warpStartThread = blockWarp * config_.lanesPerWarp;
            smWarps[smIndex].emplace_back(config_.lanesPerWarp, config_.registersPerLane,
                                          config_.predicatesPerLane, globalWarpId++, cta,
                                          blockWarp, warpStartThread, blockDim, gridDim);
            std::vector<bool> mask(config_.lanesPerWarp, false);
            for (std::size_t lane = 0; lane < config_.lanesPerWarp; ++lane) {
                mask[lane] = warpStartThread + lane < blockDim;
            }
            smWarps[smIndex].back().setActiveMask(mask);
        }
    }

    for (std::size_t sm = 0; sm < sms_.size(); ++sm) {
        if (smWarps[sm].empty()) {
            smWarps[sm].emplace_back(config_.lanesPerWarp, config_.registersPerLane,
                                     config_.predicatesPerLane, globalWarpId++, sm, 0, 0,
                                     blockDim, gridDim);
        }
        sms_[sm].setWarps(std::move(smWarps[sm]));
    }
}

} // namespace simt
