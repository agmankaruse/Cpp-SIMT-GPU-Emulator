#pragma once

#include <vector>

namespace simt {

class Scoreboard {
public:
    explicit Scoreboard(std::size_t registerCount = 64);

    bool canRead(const std::vector<int>& registers) const;
    bool isPending(int reg) const;
    void reserve(int reg);
    void release(int reg);
    void clear();
    const std::vector<int>& pendingRegisters() const { return pendingWrites_; }

private:
    std::vector<int> pendingWrites_;
};

} // namespace simt
