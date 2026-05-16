#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

struct CoalescingResult {
    std::size_t activeLaneAccesses = 0;
    std::size_t transactions = 0;
};

class MemoryCoalescer {
public:
    explicit MemoryCoalescer(std::size_t lineBytes = 128);

    CoalescingResult coalesce(const std::vector<std::uint32_t>& addresses,
                              const std::vector<bool>& activeMask) const;

    std::size_t lineBytes() const { return lineBytes_; }
    std::size_t wordsPerTransaction() const { return lineBytes_ / 4; }

private:
    std::size_t lineBytes_;
};

} // namespace simt
