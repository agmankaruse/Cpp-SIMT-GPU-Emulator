#pragma once

#include "memory.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

struct SharedMemoryAccessInfo {
    std::uint64_t activeAccesses = 0;
    std::uint64_t conflictEvents = 0;
    std::uint64_t conflicts = 0;
    std::uint64_t maxConflictDegree = 0;
    double averageConflictDegree = 1.0;
};

class SharedMemory : public ByteAddressableMemory {
public:
    explicit SharedMemory(std::size_t bytes = 32 * 1024);

    std::size_t bankForAddress(std::uint32_t address) const;
    std::uint64_t bankConflicts(const std::vector<std::uint32_t>& addresses,
                                const std::vector<bool>& activeMask) const;
    SharedMemoryAccessInfo analyzeAccess(const std::vector<std::uint32_t>& addresses,
                                         const std::vector<bool>& activeMask,
                                         bool isLoad) const;
};

} // namespace simt
