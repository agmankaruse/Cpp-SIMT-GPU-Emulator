#include "cache.hpp"

#include <cassert>

using namespace simt;

int main() {
    L1DataCache cache(128, 64, 1, 3, 40);
    CacheAccessResult first = cache.access(0);
    CacheAccessResult second = cache.access(4);
    CacheAccessResult third = cache.access(128);
    CacheAccessResult fourth = cache.access(0);

    assert(!first.hit);
    assert(first.latency == 40);
    assert(second.hit);
    assert(second.latency == 3);
    assert(!third.hit);
    assert(!fourth.hit);
}
