#include "memory_coalescer.hpp"

#include <cassert>

int main() {
    simt::MemoryCoalescer coalescer(128);
    std::vector<std::uint32_t> addresses = {0, 4, 8, 12};
    std::vector<bool> mask = {true, true, true, true};
    const auto result = coalescer.coalesce(addresses, mask);
    assert(result.transactions == 1);
    assert(result.requestedBytes == 16);
    assert(result.transferredBytes == 128);
}
