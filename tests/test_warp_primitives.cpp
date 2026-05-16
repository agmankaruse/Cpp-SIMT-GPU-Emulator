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
.kernel warp_primitives
MOV.LANEID r1
MOVI r2, 2
SETP.LT p0, r1, r2
VOTE.ANY p1, p0
VOTE.ALL p2, p0
BALLOT r3, p0
MOVI r5, 1
SHFL.IDX r4, r1, r5
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    gpu.run();

    const Warp& warp = gpu.sm(0).warp(0);
    for (std::size_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(warp.lane(lane).pred(1));
        assert(!warp.lane(lane).pred(2));
        assert(warp.lane(lane).reg(3) == 3);
        assert(warp.lane(lane).reg(4) == 1);
    }
}
