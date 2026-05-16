#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 2;
    cfg.lanesPerWarp = 4;
    cfg.gridDim = 2;
    cfg.blockDim = 8;

    const Program program = Parser::parseText(R"(
.kernel launch
MOV.TID r1
MOV.CTAID r2
MOV.GTID r3
MOV.NTID r4
MOV.NCTA r5
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    gpu.run();

    assert(gpu.sm(0).warpCount() == 4);
    assert(gpu.sm(0).warp(0).lane(3).reg(1) == 3);
    assert(gpu.sm(0).warp(1).lane(0).reg(1) == 4);
    assert(gpu.sm(0).warp(2).lane(0).reg(2) == 1);
    assert(gpu.sm(0).warp(2).lane(0).reg(3) == 8);
    assert(gpu.sm(0).warp(0).lane(0).reg(4) == 8);
    assert(gpu.sm(0).warp(0).lane(0).reg(5) == 2);
}
