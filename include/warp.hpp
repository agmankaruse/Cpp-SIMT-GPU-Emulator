#pragma once

#include "divergence_stack.hpp"
#include "lane.hpp"
#include "scoreboard.hpp"
#include "stats.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

class Warp {
public:
    Warp(std::size_t laneCount = 32, std::size_t registerCount = 64,
         std::size_t predicateCount = 4, std::size_t globalWarpId = 0,
         std::size_t ctaId = 0, std::size_t blockWarpId = 0,
         std::size_t warpStartThread = 0, std::size_t blockDim = 32,
         std::size_t gridDim = 1);

    std::size_t id() const { return globalWarpId_; }
    std::size_t ctaId() const { return ctaId_; }
    std::size_t blockWarpId() const { return blockWarpId_; }
    std::size_t warpStartThread() const { return warpStartThread_; }
    std::size_t blockDim() const { return blockDim_; }
    std::size_t gridDim() const { return gridDim_; }
    std::size_t threadId(std::size_t lane) const { return warpStartThread_ + lane; }
    std::size_t globalThreadId(std::size_t lane) const {
        return ctaId_ * blockDim_ + threadId(lane);
    }
    std::size_t laneCount() const { return lanes_.size(); }

    Lane& lane(std::size_t index) { return lanes_.at(index); }
    const Lane& lane(std::size_t index) const { return lanes_.at(index); }
    const std::vector<Lane>& lanes() const { return lanes_; }

    std::size_t pc() const { return pc_; }
    void setPc(std::size_t pc) { pc_ = pc; }
    void advancePc() { ++pc_; }

    bool halted() const { return halted_; }
    void setHalted(bool value) { halted_ = value; }
    bool done() const { return halted_ && divergenceStack_.empty(); }

    bool waitingOnGlobalMemory() const { return waitingOnGlobalMemory_; }
    void setWaitingOnGlobalMemory(bool value) { waitingOnGlobalMemory_ = value; }
    bool waitingOnBarrier() const { return waitingOnBarrier_; }
    void setWaitingOnBarrier(bool value) { waitingOnBarrier_ = value; }

    const std::vector<bool>& activeMask() const { return activeMask_; }
    std::vector<bool>& activeMask() { return activeMask_; }
    void setActiveMask(const std::vector<bool>& mask) { activeMask_ = mask; }
    bool anyActive() const;

    Scoreboard& scoreboard() { return scoreboard_; }
    const Scoreboard& scoreboard() const { return scoreboard_; }

    DivergenceStack& divergenceStack() { return divergenceStack_; }
    const DivergenceStack& divergenceStack() const { return divergenceStack_; }

    std::uint64_t lastIssueCycle() const { return lastIssueCycle_; }
    void setLastIssueCycle(std::uint64_t cycle) { lastIssueCycle_ = cycle; }

    bool tryReconvergeAtCurrentPc(Stats& stats);
    void haltOrDeferToDivergenceStack(Stats& stats);

private:
    std::size_t globalWarpId_;
    std::size_t ctaId_;
    std::size_t blockWarpId_;
    std::size_t warpStartThread_;
    std::size_t blockDim_;
    std::size_t gridDim_;
    std::vector<Lane> lanes_;
    std::vector<bool> activeMask_;
    std::size_t pc_ = 0;
    bool halted_ = false;
    bool waitingOnGlobalMemory_ = false;
    bool waitingOnBarrier_ = false;
    Scoreboard scoreboard_;
    DivergenceStack divergenceStack_;
    std::uint64_t lastIssueCycle_ = 0;
};

} // namespace simt
