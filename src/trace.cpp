#include "trace.hpp"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace simt {

namespace {

std::string csvEscape(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

} // namespace

Trace::Trace(bool enabled, const std::string& timelinePath)
    : enabled_(enabled), timelinePath_(timelinePath) {}

std::string maskToString(const std::vector<bool>& mask) {
    std::string text;
    text.reserve(mask.size());
    for (bool active : mask) {
        text.push_back(active ? '1' : '0');
    }
    return text;
}

void Trace::recordIssue(std::uint64_t cycle, std::size_t smId, std::size_t warpId,
                        std::size_t pc, const Instruction& instruction,
                        const std::vector<bool>& activeMask) {
    if (!enabled_ && !timelineEnabled()) {
        return;
    }

    if (enabled_) {
        std::ostringstream line;
        line << "[cycle " << std::setw(5) << cycle << "] "
             << "SM" << smId << " W" << warpId << " PC=" << pc
             << " mask=" << maskToString(activeMask) << " :: " << instruction.text;
        lines_.push_back(line.str());
    }
    recordEvent(cycle, smId, warpId, &instruction, pc, "ISSUE", activeMask);
}

void Trace::recordEvent(std::uint64_t cycle, std::size_t smId, std::size_t warpId,
                        const Instruction* instruction, std::size_t pc,
                        const std::string& event,
                        const std::vector<bool>& activeMask,
                        const std::string& stallReason) {
    if (!timelineEnabled()) {
        return;
    }

    std::ostringstream row;
    const bool selected = event == "ISSUE";
    const bool eligible = stallReason.empty();
    row << cycle << ',' << smId << ',' << 0 << ',' << warpId << ','
        << pc << ','
        << csvEscape(instruction == nullptr ? "" : instruction->text) << ','
        << event << ',' << maskToString(activeMask) << ','
        << (eligible ? "1" : "0") << ','
        << (selected ? "1" : "0") << ','
        << csvEscape(stallReason) << ','
        << "" << ','
        << "" << ','
        << "";
    timelineRows_.push_back(row.str());
}

void Trace::print(std::ostream& os) const {
    for (const auto& line : lines_) {
        os << line << '\n';
    }
}

void Trace::writeTimelineCsv() const {
    if (!timelineEnabled()) {
        return;
    }
    std::ofstream file(timelinePath_);
    file << "cycle,sm,cta,warp,pc,instruction,event,active_mask,eligible,selected,stall_reason,"
            "memory_transaction_count,cache_result,divergence_depth\n";
    for (const auto& row : timelineRows_) {
        file << row << '\n';
    }
}

} // namespace simt
