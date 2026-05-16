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

    CoalescingResult good = coalescer.coalesce(contiguous, active);
    CoalescingResult bad = coalescer.coalesce(strided, active);

    assert(good.transactions == 1);
    assert(good.requestedBytes == 128);
    assert(good.transferredBytes == 128);
    assert(good.wastedBytes == 0);

    assert(bad.transactions == 32);
    assert(bad.requestedBytes == 128);
    assert(bad.transferredBytes == 4096);
    assert(bad.wastedBytes == 3968);
}
