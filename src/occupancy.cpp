#include "occupancy.hpp"

#include <algorithm>
#include <ostream>

namespace simt {
namespace {

void scanInstructionRegister(int reg, int& maxReg) {
    if (reg >= 0) {
        maxReg = std::max(maxReg, reg);
    }
}

} // namespace

void OccupancyReport::print(std::ostream& os) const {
    os << "\n=== Occupancy Report ===\n";
    os << "Active CTAs per SM: " << activeCTAsPerSM << '\n';
    os << "Active warps per SM: " << activeWarpsPerSM << '\n';
    os << "Occupancy percentage: " << occupancyPercent << "%\n";
    os << "Register pressure: " << registerPressurePercent << "% (" << registersPerThread
       << " registers/thread)\n";
    os << "Shared memory pressure: " << sharedMemoryPressurePercent << "% ("
       << sharedMemoryPerCTA << " bytes/CTA)\n";
    os << "Limiting resource: " << limitingFactor << '\n';
    os << "Suggested optimization: " << suggestedOptimization << '\n';
}

OccupancyReport calculateOccupancy(const GPUConfig& config, const Program& program) {
    int maxReg = 0;
    for (const Instruction& inst : program.instructions) {
        scanInstructionRegister(inst.dst, maxReg);
        scanInstructionRegister(inst.srcA, maxReg);
        scanInstructionRegister(inst.srcB, maxReg);
        scanInstructionRegister(inst.srcC, maxReg);
    }

    OccupancyReport report;
    report.registersPerThread = static_cast<std::size_t>(maxReg + 1);
    report.sharedMemoryPerCTA = config.maxSharedMemoryPerCTA;
    const std::size_t blockDim = config.blockDim == 0
                                     ? config.warpsPerSM * config.lanesPerWarp
                                     : config.blockDim;
    report.warpsPerCTA = (blockDim + config.lanesPerWarp - 1) / config.lanesPerWarp;

    const std::size_t byWarpSlots = config.warpsPerSM;
    const std::size_t regsPerWarp = std::max<std::size_t>(
        1, report.registersPerThread * config.lanesPerWarp);
    const std::size_t byRegisters = config.registerFileEntriesPerSM / regsPerWarp;
    const std::size_t ctasByShared = report.sharedMemoryPerCTA == 0
                                         ? config.warpsPerSM
                                         : config.sharedMemoryBytesPerSM / report.sharedMemoryPerCTA;
    const std::size_t byShared = ctasByShared * report.warpsPerCTA;

    report.theoreticalActiveWarpsPerSM = std::min({byWarpSlots, byRegisters, byShared});
    report.activeWarpsPerSM = report.theoreticalActiveWarpsPerSM;
    report.activeCTAsPerSM = report.warpsPerCTA == 0 ? 0 : report.activeWarpsPerSM / report.warpsPerCTA;
    if (config.warpsPerSM != 0) {
        report.occupancyPercent =
            100.0 * static_cast<double>(report.theoreticalActiveWarpsPerSM) /
            static_cast<double>(config.warpsPerSM);
    }
    if (config.registerFileEntriesPerSM != 0) {
        report.registerPressurePercent =
            100.0 * static_cast<double>(regsPerWarp * report.activeWarpsPerSM) /
            static_cast<double>(config.registerFileEntriesPerSM);
    }
    if (config.sharedMemoryBytesPerSM != 0) {
        report.sharedMemoryPressurePercent =
            100.0 * static_cast<double>(report.sharedMemoryPerCTA * std::max<std::size_t>(1, report.activeCTAsPerSM)) /
            static_cast<double>(config.sharedMemoryBytesPerSM);
    }

    if (report.theoreticalActiveWarpsPerSM == byRegisters && byRegisters <= byShared &&
        byRegisters <= byWarpSlots) {
        report.limitingFactor = "registers";
        report.suggestedOptimization = "Reduce live registers per thread or use smaller blocks.";
    } else if (report.theoreticalActiveWarpsPerSM == byShared && byShared <= byWarpSlots) {
        report.limitingFactor = "shared memory";
        report.suggestedOptimization = "Reduce shared memory per CTA or split the tile.";
    } else {
        report.limitingFactor = "warp slots";
        report.suggestedOptimization = "Increase independent work per warp or tune block size.";
    }
    return report;
}

} // namespace simt
