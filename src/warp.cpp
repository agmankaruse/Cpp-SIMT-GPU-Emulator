#include "warp.hpp"

#include <algorithm>

namespace simt {

Warp::Warp(std::size_t laneCount, std::size_t registerCount,
           std::size_t predicateCount, std::size_t globalWarpId, std::size_t ctaId,
           std::size_t blockWarpId, std::size_t warpStartThread,
           std::size_t blockDim, std::size_t gridDim)
    : globalWarpId_(globalWarpId),
      ctaId_(ctaId),
      blockWarpId_(blockWarpId),
      warpStartThread_(warpStartThread),
      blockDim_(blockDim),
      gridDim_(gridDim),
      lanes_(laneCount, Lane(registerCount, predicateCount)),
      activeMask_(laneCount, true),
      scoreboard_(registerCount) {}

bool Warp::anyActive() const {
    return std::any_of(activeMask_.begin(), activeMask_.end(), [](bool active) {
        return active;
    });
}

bool Warp::tryReconvergeAtCurrentPc(Stats& stats) {
    if (divergenceStack_.empty()) {
        return false;
    }

    DivergenceEntry& entry = divergenceStack_.top();
    if (!entry.hasReconvergePc || entry.reconvergePc != pc_) {
        return false;
    }

    for (std::size_t lane = 0; lane < activeMask_.size() && lane < entry.completedMask.size();
         ++lane) {
        activeMask_[lane] = activeMask_[lane] || entry.completedMask[lane];
    }
    divergenceStack_.pop();
    ++stats.branchReconvergenceEvents;
    return true;
}

void Warp::haltOrDeferToDivergenceStack(Stats& stats) {
    if (divergenceStack_.empty()) {
        halted_ = true;
        std::fill(activeMask_.begin(), activeMask_.end(), false);
        return;
    }

    DivergenceEntry entry = divergenceStack_.pop();
    pc_ = entry.deferredPc;
    activeMask_ = entry.deferredMask;
    ++stats.branchReconvergenceEvents;
}

} // namespace simt
