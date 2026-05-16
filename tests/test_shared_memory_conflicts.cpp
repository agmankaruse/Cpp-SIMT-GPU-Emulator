#include "shared_memory.hpp"

#include <cassert>

using namespace simt;

int main() {
    SharedMemory shared(4096);
    std::vector<bool> active(4, true);

    SharedMemoryAccessInfo conflict =
        shared.analyzeAccess({0, 128, 256, 384}, active, false);
    assert(conflict.conflictEvents == 1);
    assert(conflict.conflicts == 3);
    assert(conflict.maxConflictDegree == 4);

    SharedMemoryAccessInfo broadcast =
        shared.analyzeAccess({0, 0, 0, 0}, active, true);
    assert(broadcast.conflicts == 0);
    assert(broadcast.maxConflictDegree == 1);
}
