#pragma once

#include <cstdint>
#include <vector>

namespace simt {

class Lane {
public:
    Lane(std::size_t registerCount = 64, std::size_t predicateCount = 4);

    std::int32_t reg(std::size_t index) const;
    void setReg(std::size_t index, std::int32_t value);

    bool pred(std::size_t index) const;
    void setPred(std::size_t index, bool value);

    std::size_t registerCount() const { return registers_.size(); }
    std::size_t predicateCount() const { return predicates_.size(); }

private:
    std::vector<std::int32_t> registers_;
    std::vector<bool> predicates_;
};

} // namespace simt
