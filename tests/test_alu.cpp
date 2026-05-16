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
.kernel alu
MOVI r1, 6
MOVI r2, 7
ADD r3, r1, r2
SUB r4, r2, r1
MUL r5, r1, r2
MAD r6, r1, r2, r3
AND r7, r1, r2
OR r8, r1, r2
XOR r9, r1, r2
SHL r10, r1, 2
SHR r11, r10, 1
SETP.EQ p0, r3, r3
BRA.P p0, done
MOVI r12, 0
HALT
done:
MOVI r12, 1
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    gpu.run();

    const Warp& warp = gpu.sm(0).warp(0);
    for (std::size_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(warp.lane(lane).reg(3) == 13);
        assert(warp.lane(lane).reg(4) == 1);
        assert(warp.lane(lane).reg(5) == 42);
        assert(warp.lane(lane).reg(6) == 55);
        assert(warp.lane(lane).reg(7) == (6 & 7));
        assert(warp.lane(lane).reg(8) == (6 | 7));
        assert(warp.lane(lane).reg(9) == (6 ^ 7));
        assert(warp.lane(lane).reg(10) == 24);
        assert(warp.lane(lane).reg(11) == 12);
        assert(warp.lane(lane).pred(0));
        assert(warp.lane(lane).reg(12) == 1);
    }
}
