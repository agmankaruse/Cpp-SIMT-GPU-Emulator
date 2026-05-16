#include "warp_scheduler.hpp"

#include <cassert>

using namespace simt;

int main() {
    WarpScheduler rr(SchedulerPolicy::RoundRobin);
    std::vector<std::uint64_t> ages = {0, 0, 0};

    assert(rr.select({0, 1, 2}, ages) == 0);
    assert(rr.select({1, 2}, ages) == 1);
    assert(rr.select({2}, ages) == 2);
    assert(rr.select({0, 2}, ages) == 0);

    WarpScheduler oldest(SchedulerPolicy::OldestReady);
    ages = {9, 3, 5};
    assert(oldest.select({0, 1, 2}, ages) == 1);
    assert(oldest.select({0, 2}, ages) == 2);

    WarpScheduler greedy(SchedulerPolicy::GreedyThenOldest);
    ages = {10, 1, 2};
    assert(greedy.select({0, 1, 2}, ages) == 1);
    assert(greedy.select({1, 2}, ages) == 1);
}
