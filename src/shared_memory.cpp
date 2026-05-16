#include "shared_memory.hpp"

#include <array>

namespace simt {

SharedMemory::SharedMemory(std::size_t bytes) : ByteAddressableMemory(bytes) {}

std::size_t SharedMemory::bankForAddress(std::uint32_t address) const {
    return (address / 4u) % 32u;
}

std::uint64_t SharedMemory::bankConflicts(const std::vector<std::uint32_t>& addresses,
                                          const std::vector<bool>& activeMask) const {
    std::array<std::uint64_t, 32> banks{};
    for (std::size_t lane = 0; lane < addresses.size() && lane < activeMask.size(); ++lane) {
        if (activeMask[lane]) {
            ++banks[bankForAddress(addresses[lane])];
        }
    }

    std::uint64_t conflicts = 0;
    for (std::uint64_t count : banks) {
        if (count > 1) {
            conflicts += count - 1;
        }
    }
    return conflicts;
}

} // namespace simt
