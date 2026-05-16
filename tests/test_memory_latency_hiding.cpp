#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 2;
    cfg.lanesPerWarp = 4;
    cfg.globalMemoryLatency = 12;

    const Program program = Parser::parseText(R"(
.kernel latency_hiding
MOV.WARPID r1
MOVI r2, 0
SETP.EQ p0, r1, r2
BRA.P p0, load_path
MOVI r20, 1
ADDI r20, r20, 1
ADDI r20, r20, 1
ADDI r20, r20, 1
HALT
load_path:
MOV.LANEID r3
SHL r4, r3, 2
LD.GLOBAL r5, [r4 + 0]
ADDI r6, r5, 1
HALT
)");

    GPU gpu(cfg);
    for (std::uint32_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        gpu.globalMemory().write32(lane * 4, static_cast<int>(100 + lane));
    }
    gpu.loadProgram(program);
    const Stats stats = gpu.run();

    assert(stats.globalMemoryStallCycles > 0);
    assert(stats.schedulerIdleCycles < cfg.globalMemoryLatency);
    assert(gpu.sm(0).warp(1).lane(0).reg(20) == 4);
    assert(gpu.sm(0).warp(0).lane(3).reg(6) == 104);
}
