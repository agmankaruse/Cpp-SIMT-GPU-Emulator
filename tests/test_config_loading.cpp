#include "config.hpp"

#include <cassert>

using namespace simt;

int main() {
    GPUConfig cfg = loadGPUConfigFile("../configs/small_debug.gpuconf");
    assert(cfg.smCount == 1);
    assert(cfg.warpsPerSM == 2);
    assert(cfg.lanesPerWarp == 8);
    assert(cfg.sharedMemoryBytesPerSM == 8 * 1024);
    assert(cfg.cacheSizeBytes == 4 * 1024);
    assert(cfg.cacheLineBytes == 64);
    assert(cfg.l1HitLatency == 3);
    assert(cfg.l1MissLatency == 30);
    assert(cfg.gridDim == 1);
    assert(cfg.blockDim == 16);
    assert(cfg.schedulerPolicy == SchedulerPolicy::RoundRobin);
}
