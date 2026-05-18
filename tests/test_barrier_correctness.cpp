#include "gpu.hpp"
#include "reference_gpu.hpp"

#include <cassert>

int main() {
    simt::GPUConfig config;
    config.smCount = 1;
    config.warpsPerSM = 2;
    config.lanesPerWarp = 4;
    config.blockDim = 8;
    config.gridDim = 1;

    const auto program = simt::Parser::parseText(R"(
.kernel barrier
MOV.WARPID r1
MOVI r2, 0
SETP.EQ p0, r1, r2
BRA.P p0, producer
BAR.SYNC
LD.SHARED r5, [r0 + 0]
HALT
producer:
MOVI r3, 1234
ST.SHARED [r0 + 0], r3
BAR.SYNC
LD.SHARED r5, [r0 + 0]
HALT
)");

    simt::GPU gpu(config);
    simt::initializeGlobalMemory(gpu.globalMemory());
    gpu.loadProgram(program);
    gpu.run(false, 100000, {});
    for (std::size_t warp = 0; warp < gpu.sm(0).warpCount(); ++warp) {
        for (std::size_t lane = 0; lane < gpu.sm(0).warp(warp).laneCount(); ++lane) {
            assert(gpu.sm(0).warp(warp).lane(lane).reg(5) == 1234);
        }
    }
}
