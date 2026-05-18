#include "reference_gpu.hpp"

#include <cassert>

int main() {
    simt::GPUConfig config;
    config.smCount = 1;
    config.warpsPerSM = 1;
    config.lanesPerWarp = 4;
    config.blockDim = 4;
    config.gridDim = 1;

    const auto program = simt::Parser::parseText(R"(
.kernel divergence
MOV.LANEID r1
MOVI r2, 2
SETP.LT p0, r1, r2
BRA.P p0, low
MOVI r3, 20
SHL r4, r1, 2
ST.GLOBAL [r4 + 2048], r3
HALT
low:
MOVI r3, 10
SHL r4, r1, 2
ST.GLOBAL [r4 + 2048], r3
HALT
)");
    assert(simt::runGpuDifferentialTest(program, config).passed);
}
