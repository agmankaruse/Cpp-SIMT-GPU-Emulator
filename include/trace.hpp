#pragma once

#include "instruction.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace simt {

class Trace {
public:
    explicit Trace(bool enabled = false);

    bool enabled() const { return enabled_; }
    void recordIssue(std::uint64_t cycle, std::size_t smId, std::size_t warpId,
                     std::size_t pc, const Instruction& instruction,
                     const std::vector<bool>& activeMask);
    void print(std::ostream& os) const;

private:
    bool enabled_;
    std::vector<std::string> lines_;
};

std::string maskToString(const std::vector<bool>& mask);

} // namespace simt
