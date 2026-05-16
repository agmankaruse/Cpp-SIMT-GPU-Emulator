#include "occupancy.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 8;
    cfg.lanesPerWarp = 32;
    cfg.blockDim = 128;
    cfg.registerFileEntriesPerSM = 4096;

    const Program program = Parser::parseText(R"(
.kernel occupancy
MOVI r40, 1
ADD r41, r40, r40
HALT
)");

    OccupancyReport report = calculateOccupancy(cfg, program);
    assert(report.registersPerThread == 42);
    assert(report.warpsPerCTA == 4);
    assert(report.theoreticalActiveWarpsPerSM > 0);
    assert(report.occupancyPercent > 0.0);
    assert(!report.limitingFactor.empty());
}
