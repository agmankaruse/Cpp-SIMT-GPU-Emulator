#include "shared_memory.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace simt {

SharedMemory::SharedMemory(std::size_t bytes) : ByteAddressableMemory(bytes) {}

std::size_t SharedMemory::bankForAddress(std::uint32_t address) const {
    return (address / 4u) % 32u;
}

std::uint64_t SharedMemory::bankConflicts(const std::vector<std::uint32_t>& addresses,
                                          const std::vector<bool>& activeMask) const {
    return analyzeAccess(addresses, activeMask, false).conflicts;
}

SharedMemoryAccessInfo SharedMemory::analyzeAccess(const std::vector<std::uint32_t>& addresses,
                                                   const std::vector<bool>& activeMask,
                                                   bool isLoad) const {
    std::array<std::vector<std::uint32_t>, 32> banks;
    SharedMemoryAccessInfo info;
    for (std::size_t lane = 0; lane < addresses.size() && lane < activeMask.size(); ++lane) {
        if (!activeMask[lane]) {
            continue;
        }
        ++info.activeAccesses;
        banks[bankForAddress(addresses[lane])].push_back(addresses[lane]);
    }

    std::uint64_t bankGroups = 0;
    std::uint64_t degreeTotal = 0;
    for (auto& accesses : banks) {
        if (accesses.empty()) {
            continue;
        }
        ++bankGroups;
        std::set<std::uint32_t> uniqueAddresses(accesses.begin(), accesses.end());
        std::uint64_t degree = accesses.size();
        if (isLoad && uniqueAddresses.size() == 1) {
            degree = 1;
        } else {
            degree = uniqueAddresses.size();
        }
        info.maxConflictDegree = std::max(info.maxConflictDegree, degree);
        degreeTotal += degree;
        if (degree > 1) {
            ++info.conflictEvents;
            info.conflicts += degree - 1;
        }
    }

    if (bankGroups != 0) {
        info.averageConflictDegree = static_cast<double>(degreeTotal) /
                                     static_cast<double>(bankGroups);
    }
    return info;
}

} // namespace simt
