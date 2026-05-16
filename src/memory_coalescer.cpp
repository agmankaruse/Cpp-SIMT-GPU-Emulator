#include "memory_coalescer.hpp"

#include <unordered_set>

namespace simt {

MemoryCoalescer::MemoryCoalescer(std::size_t lineBytes) : lineBytes_(lineBytes) {}

CoalescingResult MemoryCoalescer::coalesce(const std::vector<std::uint32_t>& addresses,
                                           const std::vector<bool>& activeMask) const {
    std::unordered_set<std::uint32_t> lineBases;
    CoalescingResult result;
    for (std::size_t lane = 0; lane < addresses.size() && lane < activeMask.size(); ++lane) {
        if (!activeMask[lane]) {
            continue;
        }
        ++result.activeLaneAccesses;
        const auto lineBase = static_cast<std::uint32_t>(
            (addresses[lane] / static_cast<std::uint32_t>(lineBytes_)) *
            static_cast<std::uint32_t>(lineBytes_));
        lineBases.insert(lineBase);
    }
    result.transactions = lineBases.size();
    return result;
}

} // namespace simt
