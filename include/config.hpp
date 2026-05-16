#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace simt {

enum class SchedulerPolicy {
    RoundRobin,
    OldestReady
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
    SchedulerPolicy schedulerPolicy = SchedulerPolicy::RoundRobin;
};

SchedulerPolicy schedulerPolicyFromString(const std::string& text);
std::string toString(SchedulerPolicy policy);

} // namespace simt
