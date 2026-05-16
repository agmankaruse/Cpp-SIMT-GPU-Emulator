#include "functional_unit.hpp"

#include <algorithm>

namespace simt {

FunctionalUnit::FunctionalUnit(std::size_t pipelines, std::uint64_t latency)
    : latency_(latency), busyUntil_(pipelines, 0) {}

bool FunctionalUnit::canIssue(std::uint64_t cycle) const {
    return std::any_of(busyUntil_.begin(), busyUntil_.end(), [cycle](std::uint64_t busyUntil) {
        return busyUntil <= cycle;
    });
}

std::uint64_t FunctionalUnit::issue(std::uint64_t cycle) {
    auto it = std::find_if(busyUntil_.begin(), busyUntil_.end(), [cycle](std::uint64_t busyUntil) {
        return busyUntil <= cycle;
    });
    if (it == busyUntil_.end()) {
        return cycle;
    }
    *it = cycle + 1;
    return cycle + latency_;
}

} // namespace simt
