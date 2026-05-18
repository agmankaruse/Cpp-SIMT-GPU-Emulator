#pragma once

#include "config.hpp"
#include "cache.hpp"
#include "functional_unit.hpp"
#include "global_memory.hpp"
#include "memory_coalescer.hpp"
#include "memory_request.hpp"
#include "parser.hpp"
#include "shared_memory.hpp"
#include "stats.hpp"
#include "trace.hpp"
#include "warp.hpp"
#include "warp_scheduler.hpp"

#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

namespace simt {

class InvariantChecker;

class StreamingMultiprocessor {
public:
    StreamingMultiprocessor(std::size_t id, const GPUConfig& config);

    void loadProgram(const Program* program);
    void setWarps(std::vector<Warp> warps);
    void tick(std::uint64_t cycle, GlobalMemory& globalMemory, Stats& stats, Trace* trace);

    bool active() const;
    std::size_t id() const { return id_; }
    Warp& warp(std::size_t index) { return warps_.at(index); }
    const Warp& warp(std::size_t index) const { return warps_.at(index); }
    std::size_t warpCount() const { return warps_.size(); }
    SharedMemory& sharedMemory() { return sharedMemory_; }
    const SharedMemory& sharedMemory() const { return sharedMemory_; }

private:
    friend class GPU;
    friend class InvariantChecker;

    struct PendingWrite {
        enum class Kind {
            Register,
            Predicate
        };

        Kind kind = Kind::Register;
        int warpIndex = -1;
        int dst = -1;
        std::uint64_t readyCycle = 0;
        std::vector<bool> activeMask;
        std::vector<std::int32_t> regValues;
        std::vector<bool> predValues;
    };

    enum class ReadyState {
        Ready,
        Done,
        WaitingGlobalMemory,
        WaitingBarrier,
        ScoreboardBlocked,
        FunctionalUnitBusy,
        MemoryQueueFull,
        NoProgram
    };

    struct BarrierState {
        std::set<int> waitingWarps;
        std::size_t expectedWarps = 0;
    };

    ReadyState readiness(const Warp& warp, std::uint64_t cycle) const;
    void issue(std::uint64_t cycle, int warpIndex, GlobalMemory& globalMemory,
               Stats& stats, Trace* trace);
    void completePendingWrites(std::uint64_t cycle, Stats& stats, Trace* trace);
    void completeMemoryRequests(std::uint64_t cycle, GlobalMemory& globalMemory, Stats& stats,
                                Trace* trace);
    void queueRegisterWrite(std::uint64_t readyCycle, int warpIndex, int dst,
                            const std::vector<bool>& mask,
                            const std::vector<std::int32_t>& values);
    void queuePredicateWrite(std::uint64_t readyCycle, int warpIndex, int dst,
                             const std::vector<bool>& mask,
                             const std::vector<bool>& values);
    std::vector<std::uint32_t> laneAddresses(const Warp& warp, int baseReg, std::int32_t imm) const;
    std::vector<std::int32_t> laneRegisterValues(const Warp& warp, int reg) const;
    void issueGlobalMemory(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                           Stats& stats, Trace* trace);
    void issueSharedMemory(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                           Stats& stats);
    void issueBarrier(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                      Stats& stats, Trace* trace);
    void releaseBarrierIfReady(std::uint64_t cycle, std::size_t ctaId, Stats& stats,
                               Trace* trace);
    std::size_t activeWarpCountForCTA(std::size_t ctaId) const;
    bool handleBranch(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                      Stats& stats, Trace* trace);

    const Instruction& currentInstruction(const Warp& warp) const;
    bool functionUnitReady(UnitType unit, std::uint64_t cycle) const;
    std::uint64_t issueFunctionUnit(UnitType unit, std::uint64_t cycle);

    std::size_t id_;
    GPUConfig config_;
    const Program* program_ = nullptr;
    std::vector<Warp> warps_;
    SharedMemory sharedMemory_;
    MemoryCoalescer coalescer_;
    WarpScheduler scheduler_;
    FunctionalUnit intAlu_;
    FunctionalUnit sfu_;
    FunctionalUnit lsu_;
    L1DataCache l1Cache_;
    std::vector<PendingWrite> pendingWrites_;
    std::vector<MemoryRequest> memoryRequests_;
    std::unordered_map<std::size_t, BarrierState> barriers_;
};

} // namespace simt
