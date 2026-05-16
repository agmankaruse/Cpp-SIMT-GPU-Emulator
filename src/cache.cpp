#include "cache.hpp"

#include <algorithm>
#include <stdexcept>

namespace simt {

L1DataCache::L1DataCache(std::size_t sizeBytes, std::size_t lineBytes,
                         std::size_t associativity, std::uint64_t hitLatency,
                         std::uint64_t missLatency)
    : sizeBytes_(sizeBytes),
      lineBytes_(lineBytes),
      associativity_(associativity == 0 ? 1 : associativity),
      hitLatency_(hitLatency),
      missLatency_(missLatency) {
    if (lineBytes_ == 0 || sizeBytes_ < lineBytes_) {
        throw std::runtime_error("invalid L1 cache geometry");
    }
    const std::size_t lineCount = std::max<std::size_t>(1, sizeBytes_ / lineBytes_);
    const std::size_t setCount = std::max<std::size_t>(1, lineCount / associativity_);
    sets_.assign(setCount, std::vector<Line>(associativity_));
}

CacheAccessResult L1DataCache::access(std::uint32_t address) {
    const auto lineBase = static_cast<std::uint32_t>(
        (address / static_cast<std::uint32_t>(lineBytes_)) *
        static_cast<std::uint32_t>(lineBytes_));
    const std::uint32_t lineNumber = lineBase / static_cast<std::uint32_t>(lineBytes_);
    const std::size_t setIndex = lineNumber % sets_.size();
    const std::uint32_t tag = lineNumber / static_cast<std::uint32_t>(sets_.size());
    ++tick_;

    for (Line& line : sets_[setIndex]) {
        if (line.valid && line.tag == tag) {
            line.lastUsed = tick_;
            return {true, hitLatency_, lineBase};
        }
    }

    auto victim = std::min_element(sets_[setIndex].begin(), sets_[setIndex].end(),
                                   [](const Line& a, const Line& b) {
                                       if (a.valid != b.valid) {
                                           return !a.valid;
                                       }
                                       return a.lastUsed < b.lastUsed;
                                   });
    victim->valid = true;
    victim->tag = tag;
    victim->lastUsed = tick_;
    return {false, missLatency_, lineBase};
}

void L1DataCache::reset() {
    tick_ = 0;
    for (auto& set : sets_) {
        for (Line& line : set) {
            line = Line{};
        }
    }
}

} // namespace simt
