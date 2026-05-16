#include "occupancy.hpp"

#include <algorithm>
#include <ostream>

namespace simt {
namespace {

void scanInstructionRegister(const Instruction& inst, int reg, int& maxReg) {
    if (reg >= 0) {
        maxReg = std::max(maxReg, reg);
    }
}

} // namespace

void OccupancyReport::print(std::ostream& os) const {
    os << "\n=== Occupancy Report ===\n";
    os << "Registers per thread: " << registersPerThread << '\n';
    os << "Shared memory per CTA: " << sharedMemoryPerCTA << " bytes\n";
    os << "Warps per CTA: " << warpsPerCTA << '\n';
    os << "Theoretical active warps per SM: " << theoreticalActiveWarpsPerSM << '\n';
    os << "Occupancy: " << occupancyPercent << "%\n";
    os << "Limiting factor: " << limitingFactor << '\n';
}

OccupancyReport calculateOccupancy(const GPUConfig& config, const Program& program) {
    int maxReg = 0;
    for (const Instruction& inst : program.instructions) {
        scanInstructionRegister(inst, inst.dst, maxReg);
        scanInstructionRegister(inst, inst.srcA, maxReg);
        scanInstructionRegister(inst, inst.srcB, maxReg);
        scanInstructionRegister(inst, inst.srcC, maxReg);
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
    if (config.warpsPerSM != 0) {
        report.occupancyPercent =
            100.0 * static_cast<double>(report.theoreticalActiveWarpsPerSM) /
            static_cast<double>(config.warpsPerSM);
    }

    if (report.theoreticalActiveWarpsPerSM == byRegisters && byRegisters <= byShared &&
        byRegisters <= byWarpSlots) {
        report.limitingFactor = "registers";
    } else if (report.theoreticalActiveWarpsPerSM == byShared && byShared <= byWarpSlots) {
        report.limitingFactor = "shared memory";
    } else {
        report.limitingFactor = "warp slots";
    }
    return report;
}

} // namespace simt
