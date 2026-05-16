#pragma once

#include "memory.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

class SharedMemory : public ByteAddressableMemory {
public:
    explicit SharedMemory(std::size_t bytes = 32 * 1024);

    std::size_t bankForAddress(std::uint32_t address) const;
    std::uint64_t bankConflicts(const std::vector<std::uint32_t>& addresses,
                                const std::vector<bool>& activeMask) const;
};

} // namespace simt
