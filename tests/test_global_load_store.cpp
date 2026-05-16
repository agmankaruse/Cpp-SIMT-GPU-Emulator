#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 1;
    cfg.lanesPerWarp = 4;
    cfg.globalMemoryLatency = 5;

    const Program program = Parser::parseText(R"(
.kernel global_memory
MOV.LANEID r1
SHL r2, r1, 2
LD.GLOBAL r3, [r2 + 0]
ADDI r4, r3, 7
ST.GLOBAL [r2 + 128], r4
HALT
)");

    GPU gpu(cfg);
    for (std::uint32_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        gpu.globalMemory().write32(lane * 4, static_cast<int>(20 + lane));
    }
    gpu.loadProgram(program);
    const Stats stats = gpu.run();

    assert(stats.memoryRequests == 2);
    for (std::uint32_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(gpu.globalMemory().read32(128 + lane * 4) == static_cast<int>(27 + lane));
    }
}
