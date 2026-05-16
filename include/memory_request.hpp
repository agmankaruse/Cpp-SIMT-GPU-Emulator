#pragma once

#include <cstdint>
#include <vector>

namespace simt {

enum class MemorySpace {
    Global,
    Shared
};

struct MemoryRequest {
    MemorySpace space = MemorySpace::Global;
    bool isLoad = true;
    int warpIndex = -1;
    int dstReg = -1;
    std::uint64_t returnCycle = 0;
    std::size_t transactionCount = 0;
    std::vector<bool> activeMask;
    std::vector<std::uint32_t> addresses;
    std::vector<std::int32_t> storeValues;
};

} // namespace simt
