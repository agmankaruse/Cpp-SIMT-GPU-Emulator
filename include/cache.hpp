#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

struct CacheAccessResult {
    bool hit = false;
    std::uint64_t latency = 0;
    std::uint32_t lineBase = 0;
};

class L1DataCache {
public:
    L1DataCache(std::size_t sizeBytes = 16 * 1024, std::size_t lineBytes = 128,
                std::size_t associativity = 1, std::uint64_t hitLatency = 4,
                std::uint64_t missLatency = 100);

    CacheAccessResult access(std::uint32_t address);
    void reset();

    std::size_t sizeBytes() const { return sizeBytes_; }
    std::size_t lineBytes() const { return lineBytes_; }
    std::size_t setCount() const { return sets_.empty() ? 0 : sets_.size(); }
    std::uint64_t hitLatency() const { return hitLatency_; }
    std::uint64_t missLatency() const { return missLatency_; }

private:
    struct Line {
        bool valid = false;
        std::uint32_t tag = 0;
        std::uint64_t lastUsed = 0;
    };

    std::size_t sizeBytes_;
    std::size_t lineBytes_;
    std::size_t associativity_;
    std::uint64_t hitLatency_;
    std::uint64_t missLatency_;
    std::uint64_t tick_ = 0;
    std::vector<std::vector<Line>> sets_;
};

} // namespace simt
