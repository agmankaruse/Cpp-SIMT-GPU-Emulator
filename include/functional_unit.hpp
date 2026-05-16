#pragma once

#include <cstdint>
#include <vector>

namespace simt {

class FunctionalUnit {
public:
    FunctionalUnit(std::size_t pipelines = 1, std::uint64_t latency = 1);

    bool canIssue(std::uint64_t cycle) const;
    std::uint64_t issue(std::uint64_t cycle);
    std::uint64_t latency() const { return latency_; }

private:
    std::uint64_t latency_;
    std::vector<std::uint64_t> busyUntil_;
};

} // namespace simt
