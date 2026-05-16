#include "trace.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace simt {

Trace::Trace(bool enabled) : enabled_(enabled) {}

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
    if (!enabled_) {
        return;
    }

    std::ostringstream line;
    line << "[cycle " << std::setw(5) << cycle << "] "
         << "SM" << smId << " W" << warpId << " PC=" << pc
         << " mask=" << maskToString(activeMask) << " :: " << instruction.text;
    lines_.push_back(line.str());
}

void Trace::print(std::ostream& os) const {
    for (const auto& line : lines_) {
        os << line << '\n';
    }
}

} // namespace simt
