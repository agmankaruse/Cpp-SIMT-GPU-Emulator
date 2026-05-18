#include "gpu.hpp"
#include "invariant_checker.hpp"
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
.kernel invariant
MOVI r1, 7
ADDI r2, r1, 3
HALT
)");

    simt::GPU gpu(config);
    simt::initializeGlobalMemory(gpu.globalMemory());
    gpu.loadProgram(program);
    gpu.run(false, 100000, {});
    assert(simt::InvariantChecker::check(gpu).passed());
}
