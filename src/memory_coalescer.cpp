#include "memory_coalescer.hpp"

#include <algorithm>

namespace simt {

MemoryCoalescer::MemoryCoalescer(std::size_t lineBytes) : lineBytes_(lineBytes) {}

CoalescingResult MemoryCoalescer::coalesce(const std::vector<std::uint32_t>& addresses,
                                           const std::vector<bool>& activeMask) const {
    CoalescingResult result;
    for (std::size_t lane = 0; lane < addresses.size() && lane < activeMask.size(); ++lane) {
        if (!activeMask[lane]) {
            continue;
        }
        ++result.activeLaneAccesses;
        const auto lineBase = static_cast<std::uint32_t>(
            (addresses[lane] / static_cast<std::uint32_t>(lineBytes_)) *
            static_cast<std::uint32_t>(lineBytes_));
        if (std::find(result.lineBases.begin(), result.lineBases.end(), lineBase) ==
            result.lineBases.end()) {
            result.lineBases.push_back(lineBase);
        }
    }
    std::sort(result.lineBases.begin(), result.lineBases.end());
    result.transactions = result.lineBases.size();
    result.requestedBytes = result.activeLaneAccesses * 4;
    result.transferredBytes = result.transactions * lineBytes_;
    result.wastedBytes = result.transferredBytes > result.requestedBytes
                             ? result.transferredBytes - result.requestedBytes
                             : 0;
    return result;
}

} // namespace simt
