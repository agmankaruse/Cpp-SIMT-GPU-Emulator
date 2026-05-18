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
.kernel random_like
MOVI r1, 1
MOVI r2, 2
ADD r3, r1, r2
MUL r4, r3, r2
ST.GLOBAL [r0 + 1024], r4
HALT
)");
    assert(simt::runGpuDifferentialTest(program, config).passed);
}
