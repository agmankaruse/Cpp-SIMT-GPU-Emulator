#pragma once

#include "config.hpp"
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
#include <vector>

namespace simt {

class StreamingMultiprocessor {
public:
    StreamingMultiprocessor(std::size_t id, const GPUConfig& config);

    void loadProgram(const Program* program);
    void tick(std::uint64_t cycle, GlobalMemory& globalMemory, Stats& stats, Trace* trace);

    bool active() const;
    std::size_t id() const { return id_; }
    Warp& warp(std::size_t index) { return warps_.at(index); }
    const Warp& warp(std::size_t index) const { return warps_.at(index); }
    std::size_t warpCount() const { return warps_.size(); }
    SharedMemory& sharedMemory() { return sharedMemory_; }

private:
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
        ScoreboardBlocked,
        FunctionalUnitBusy,
        MemoryQueueFull,
        NoProgram
    };

    ReadyState readiness(const Warp& warp, std::uint64_t cycle) const;
    void issue(std::uint64_t cycle, int warpIndex, GlobalMemory& globalMemory,
               Stats& stats, Trace* trace);
    void completePendingWrites(std::uint64_t cycle, Stats& stats);
    void completeMemoryRequests(std::uint64_t cycle, GlobalMemory& globalMemory, Stats& stats);
    void queueRegisterWrite(std::uint64_t readyCycle, int warpIndex, int dst,
                            const std::vector<bool>& mask,
                            const std::vector<std::int32_t>& values);
    void queuePredicateWrite(std::uint64_t readyCycle, int warpIndex, int dst,
                             const std::vector<bool>& mask,
                             const std::vector<bool>& values);
    std::vector<std::uint32_t> laneAddresses(const Warp& warp, int baseReg, std::int32_t imm) const;
    std::vector<std::int32_t> laneRegisterValues(const Warp& warp, int reg) const;
    void issueGlobalMemory(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                           Stats& stats);
    void issueSharedMemory(std::uint64_t cycle, int warpIndex, const Instruction& inst,
                           Stats& stats);
    bool handleBranch(std::uint64_t cycle, int warpIndex, const Instruction& inst, Stats& stats);

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
    std::vector<PendingWrite> pendingWrites_;
    std::vector<MemoryRequest> memoryRequests_;
};

} // namespace simt
