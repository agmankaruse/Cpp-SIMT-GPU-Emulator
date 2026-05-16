#include "divergence_stack.hpp"

#include <stdexcept>

namespace simt {

void DivergenceStack::push(const DivergenceEntry& entry) {
    entries_.push_back(entry);
}

DivergenceEntry DivergenceStack::pop() {
    if (entries_.empty()) {
        throw std::runtime_error("pop from empty divergence stack");
    }
    DivergenceEntry entry = entries_.back();
    entries_.pop_back();
    return entry;
}

DivergenceEntry& DivergenceStack::top() {
    if (entries_.empty()) {
        throw std::runtime_error("top of empty divergence stack");
    }
    return entries_.back();
}

const DivergenceEntry& DivergenceStack::top() const {
    if (entries_.empty()) {
        throw std::runtime_error("top of empty divergence stack");
    }
    return entries_.back();
}

void DivergenceStack::clear() {
    entries_.clear();
}

} // namespace simt
