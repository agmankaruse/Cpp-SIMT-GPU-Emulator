#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace simt {

enum class SchedulerPolicy {
    RoundRobin,
    GreedyThenOldest,
    OldestReady,
    TwoLevel
};

struct GPUConfig {
    std::size_t smCount = 2;
    std::size_t warpsPerSM = 8;
    std::size_t lanesPerWarp = 32;
    std::size_t registersPerLane = 64;
    std::size_t predicatesPerLane = 4;
    std::size_t intAluPipelinesPerSM = 2;
    std::size_t sfuPipelinesPerSM = 1;
    std::size_t lsuPipelinesPerSM = 1;
    std::size_t sharedMemoryBytesPerSM = 32 * 1024;
    std::size_t globalMemoryBytes = 1024 * 1024;
    std::uint64_t globalMemoryLatency = 100;
    std::uint64_t sharedMemoryLatency = 4;
    std::size_t maxOutstandingMemoryRequestsPerSM = 16;
    std::size_t memoryLineBytes = 128;
    std::size_t cacheSizeBytes = 16 * 1024;
    std::size_t cacheLineBytes = 128;
    std::size_t cacheAssociativity = 1;
    std::uint64_t l1HitLatency = 4;
    std::uint64_t l1MissLatency = 100;
    std::size_t gridDim = 0;
    std::size_t blockDim = 0;
    std::size_t registerFileEntriesPerSM = 64 * 1024;
    std::size_t maxSharedMemoryPerCTA = 16 * 1024;
    SchedulerPolicy schedulerPolicy = SchedulerPolicy::RoundRobin;
};

SchedulerPolicy schedulerPolicyFromString(const std::string& text);
std::string toString(SchedulerPolicy policy);
GPUConfig loadGPUConfigFile(const std::string& path, GPUConfig base = GPUConfig{});

} // namespace simt
