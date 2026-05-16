#include "scoreboard.hpp"

namespace simt {

Scoreboard::Scoreboard(std::size_t registerCount) : pendingWrites_(registerCount, 0) {}

bool Scoreboard::canRead(const std::vector<int>& registers) const {
    for (int reg : registers) {
        if (isPending(reg)) {
            return false;
        }
    }
    return true;
}

bool Scoreboard::isPending(int reg) const {
    return reg >= 0 && static_cast<std::size_t>(reg) < pendingWrites_.size() &&
           pendingWrites_[reg] > 0;
}

void Scoreboard::reserve(int reg) {
    if (reg >= 0 && static_cast<std::size_t>(reg) < pendingWrites_.size()) {
        ++pendingWrites_[reg];
    }
}

void Scoreboard::release(int reg) {
    if (reg >= 0 && static_cast<std::size_t>(reg) < pendingWrites_.size() &&
        pendingWrites_[reg] > 0) {
        --pendingWrites_[reg];
    }
}

void Scoreboard::clear() {
    for (int& pending : pendingWrites_) {
        pending = 0;
    }
}

} // namespace simt
