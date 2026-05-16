#pragma once

#include "instruction.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace simt {

class Trace {
public:
    explicit Trace(bool enabled = false, const std::string& timelinePath = {});

    bool enabled() const { return enabled_; }
    bool timelineEnabled() const { return !timelinePath_.empty(); }
    void recordIssue(std::uint64_t cycle, std::size_t smId, std::size_t warpId,
                     std::size_t pc, const Instruction& instruction,
                     const std::vector<bool>& activeMask);
    void recordEvent(std::uint64_t cycle, std::size_t smId, std::size_t warpId,
                     const Instruction* instruction, std::size_t pc,
                     const std::string& event, const std::vector<bool>& activeMask,
                     const std::string& stallReason = {});
    void print(std::ostream& os) const;
    void writeTimelineCsv() const;

private:
    bool enabled_;
    std::string timelinePath_;
    std::vector<std::string> lines_;
    std::vector<std::string> timelineRows_;
};

std::string maskToString(const std::vector<bool>& mask);

} // namespace simt
