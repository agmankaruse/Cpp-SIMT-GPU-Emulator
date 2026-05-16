#include "warp_scheduler.hpp"

#include <limits>

namespace simt {

WarpScheduler::WarpScheduler(SchedulerPolicy policy) : policy_(policy) {}

int WarpScheduler::select(const std::vector<int>& readyWarps,
                          const std::vector<std::uint64_t>& lastIssueCycles) {
    if (readyWarps.empty()) {
        return -1;
    }

    if (policy_ == SchedulerPolicy::GreedyThenOldest && greedyWarp_ >= 0) {
        for (int ready : readyWarps) {
            if (ready == greedyWarp_) {
                return greedyWarp_;
            }
        }
    }

    if (policy_ == SchedulerPolicy::OldestReady ||
        policy_ == SchedulerPolicy::GreedyThenOldest ||
        policy_ == SchedulerPolicy::TwoLevel) {
        int selected = readyWarps.front();
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (int warp : readyWarps) {
            const auto age = lastIssueCycles.at(static_cast<std::size_t>(warp));
            if (age < oldest || (age == oldest && warp < selected)) {
                oldest = age;
                selected = warp;
            }
        }
        greedyWarp_ = selected;
        return selected;
    }

    for (std::size_t probe = 0; probe < lastIssueCycles.size(); ++probe) {
        const int candidate = static_cast<int>((roundRobinCursor_ + probe) % lastIssueCycles.size());
        for (int ready : readyWarps) {
            if (ready == candidate) {
                roundRobinCursor_ = (static_cast<std::size_t>(candidate) + 1) %
                                    lastIssueCycles.size();
                greedyWarp_ = candidate;
                return candidate;
            }
        }
    }
    return readyWarps.front();
}

} // namespace simt
