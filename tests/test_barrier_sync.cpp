#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 2;
    cfg.lanesPerWarp = 4;
    cfg.gridDim = 1;
    cfg.blockDim = 8;

    const Program program = Parser::parseText(R"(
.kernel barrier
MOV.WARPID r1
MOVI r2, 0
SETP.EQ p0, r1, r2
BRA.P p0, producer
BAR.SYNC
LD.SHARED r4, [r0 + 0]
HALT
producer:
MOVI r3, 42
ST.SHARED [r0 + 0], r3
BAR.SYNC
LD.SHARED r4, [r0 + 0]
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    Stats stats = gpu.run();

    assert(stats.barrierCount == 2);
    assert(stats.barrierStallCycles > 0);
    assert(gpu.sm(0).warp(0).lane(0).reg(4) == 42);
    assert(gpu.sm(0).warp(1).lane(0).reg(4) == 42);
}
