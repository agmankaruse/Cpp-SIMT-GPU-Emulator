#pragma once

#include <cstddef>
#include <vector>

namespace simt {

struct DivergenceEntry {
    std::size_t deferredPc = 0;
    std::vector<bool> deferredMask;

    bool hasReconvergePc = false;
    std::size_t reconvergePc = 0;
    std::vector<bool> completedMask;
};

class DivergenceStack {
public:
    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

    void push(const DivergenceEntry& entry);
    DivergenceEntry pop();
    DivergenceEntry& top();
    const DivergenceEntry& top() const;
    void clear();

private:
    std::vector<DivergenceEntry> entries_;
};

} // namespace simt
