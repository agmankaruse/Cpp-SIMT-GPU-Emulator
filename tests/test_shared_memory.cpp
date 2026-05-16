#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 1;
    cfg.lanesPerWarp = 4;
    cfg.sharedMemoryLatency = 2;

    const Program program = Parser::parseText(R"(
.kernel shared_memory
MOV.LANEID r1
SHL r2, r1, 2
ADDI r3, r1, 100
ST.SHARED [r2 + 0], r3
LD.SHARED r4, [r2 + 0]
SHL r5, r1, 7
ST.SHARED [r5 + 0], r3
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    const Stats stats = gpu.run();

    const Warp& warp = gpu.sm(0).warp(0);
    for (std::size_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(warp.lane(lane).reg(4) == static_cast<int>(100 + lane));
    }
    assert(stats.sharedMemoryBankConflicts > 0);
}
