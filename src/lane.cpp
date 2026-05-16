#include "lane.hpp"

#include <stdexcept>

namespace simt {

Lane::Lane(std::size_t registerCount, std::size_t predicateCount)
    : registers_(registerCount, 0), predicates_(predicateCount, false) {}

std::int32_t Lane::reg(std::size_t index) const {
    if (index >= registers_.size()) {
        throw std::out_of_range("register index out of range");
    }
    return registers_[index];
}

void Lane::setReg(std::size_t index, std::int32_t value) {
    if (index >= registers_.size()) {
        throw std::out_of_range("register index out of range");
    }
    registers_[index] = value;
}

bool Lane::pred(std::size_t index) const {
    if (index >= predicates_.size()) {
        throw std::out_of_range("predicate index out of range");
    }
    return predicates_[index];
}

void Lane::setPred(std::size_t index, bool value) {
    if (index >= predicates_.size()) {
        throw std::out_of_range("predicate index out of range");
    }
    predicates_[index] = value;
}

} // namespace simt
