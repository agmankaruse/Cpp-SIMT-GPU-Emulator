#pragma once

#include "config.hpp"

#include <cstdint>
#include <vector>

namespace simt {

class WarpScheduler {
public:
    explicit WarpScheduler(SchedulerPolicy policy = SchedulerPolicy::RoundRobin);

    int select(const std::vector<int>& readyWarps,
               const std::vector<std::uint64_t>& lastIssueCycles);

    SchedulerPolicy policy() const { return policy_; }
    void setPolicy(SchedulerPolicy policy) { policy_ = policy; }

private:
    SchedulerPolicy policy_;
    std::size_t roundRobinCursor_ = 0;
    int greedyWarp_ = -1;
};

} // namespace simt
