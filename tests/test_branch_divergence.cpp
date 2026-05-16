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
.kernel branch_divergence
MOV.LANEID r1
MOVI r2, 2
SETP.LT p0, r1, r2
BRA.P p0, taken
MOVI r3, 20
HALT
taken:
MOVI r3, 10
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    const Stats stats = gpu.run();

    const Warp& warp = gpu.sm(0).warp(0);
    assert(warp.lane(0).reg(3) == 10);
    assert(warp.lane(1).reg(3) == 10);
    assert(warp.lane(2).reg(3) == 20);
    assert(warp.lane(3).reg(3) == 20);
    assert(stats.branchDivergenceEvents == 1);
    assert(stats.branchReconvergenceEvents == 1);
}
