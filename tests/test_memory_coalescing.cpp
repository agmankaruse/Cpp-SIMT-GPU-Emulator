#include "memory_coalescer.hpp"

#include <cassert>

using namespace simt;

int main() {
    MemoryCoalescer coalescer(128);
    std::vector<bool> active(32, true);

    std::vector<std::uint32_t> contiguous(32);
    std::vector<std::uint32_t> strided(32);
    for (std::uint32_t lane = 0; lane < 32; ++lane) {
        contiguous[lane] = lane * 4;
        strided[lane] = lane * 128;
    }

    const CoalescingResult coalesced = coalescer.coalesce(contiguous, active);
    const CoalescingResult scattered = coalescer.coalesce(strided, active);

    assert(coalesced.activeLaneAccesses == 32);
    assert(coalesced.transactions == 1);
    assert(scattered.transactions == 32);
}
