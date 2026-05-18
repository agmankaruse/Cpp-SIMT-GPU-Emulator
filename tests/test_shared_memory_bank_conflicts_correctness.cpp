#include "shared_memory.hpp"

#include <cassert>

int main() {
    simt::SharedMemory shared(1024);
    std::vector<std::uint32_t> addresses = {0, 128, 256, 384};
    std::vector<bool> mask = {true, true, true, true};
    const auto info = shared.analyzeAccess(addresses, mask, true);
    assert(info.activeAccesses == 4);
    assert(info.conflicts > 0);
    shared.write32(0, 42);
    assert(shared.read32(0) == 42);
}
