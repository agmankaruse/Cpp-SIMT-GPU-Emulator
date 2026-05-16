#pragma once

#include "memory.hpp"

namespace simt {

class GlobalMemory : public ByteAddressableMemory {
public:
    explicit GlobalMemory(std::size_t bytes = 1024 * 1024);
};

} // namespace simt
