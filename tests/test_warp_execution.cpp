#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 1;
    cfg.lanesPerWarp = 4;

    const Program program = Parser::parseText(R"(
.kernel warp_execution
MOV.LANEID r1
MOVI r2, 10
ADD r3, r1, r2
MOVI r4, 2
SETP.LT p0, r1, r4
BRA.P p0, low_lanes
MOVI r5, 99
HALT
low_lanes:
MOVI r5, 55
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    const Stats stats = gpu.run();

    const Warp& warp = gpu.sm(0).warp(0);
    for (std::size_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(warp.lane(lane).reg(1) == static_cast<int>(lane));
        assert(warp.lane(lane).reg(3) == static_cast<int>(10 + lane));
        assert(warp.lane(lane).reg(5) == (lane < 2 ? 55 : 99));
    }
    assert(stats.branchDivergenceEvents == 1);
    assert(stats.branchReconvergenceEvents == 1);
}
