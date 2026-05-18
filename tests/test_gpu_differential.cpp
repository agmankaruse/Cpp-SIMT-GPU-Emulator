#include "reference_gpu.hpp"

#include <cassert>

int main() {
    simt::GPUConfig config;
    config.smCount = 1;
    config.warpsPerSM = 1;
    config.lanesPerWarp = 4;
    config.blockDim = 4;
    config.gridDim = 1;
    config.globalMemoryLatency = 4;
    config.l1MissLatency = 4;

    const auto program = simt::Parser::parseText(R"(
.kernel diff_vector
MOV.GTID r1
SHL r2, r1, 2
LD.GLOBAL r3, [r2 + 0]
ADDI r4, r3, 1
ST.GLOBAL [r2 + 512], r4
HALT
)");

    const auto result = simt::runGpuDifferentialTest(program, config);
    assert(result.passed);
}
