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
.kernel ref_smoke
MOV.GTID r1
SHL r2, r1, 2
LD.GLOBAL r3, [r2 + 0]
ADDI r4, r3, 10
ST.GLOBAL [r2 + 256], r4
HALT
)");

    simt::ReferenceGPU reference(config);
    reference.loadProgram(program);
    reference.initializeGlobalMemory();
    reference.run();

    assert(reference.globalMemory().read32(256) == 10);
    assert(reference.globalMemory().read32(260) == 11);
    assert(reference.globalMemory().read32(264) == 12);
    assert(reference.globalMemory().read32(268) == 13);
}
