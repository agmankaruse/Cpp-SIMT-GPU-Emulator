#include "sm.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace simt {
namespace {

std::vector<bool> andMask(const std::vector<bool>& lhs, const std::vector<bool>& rhs) {
    std::vector<bool> out(lhs.size(), false);
    for (std::size_t i = 0; i < lhs.size() && i < rhs.size(); ++i) {
        out[i] = lhs[i] && rhs[i];
    }
    return out;
}

bool any(const std::vector<bool>& mask) {
    return std::any_of(mask.begin(), mask.end(), [](bool value) { return value; });
}

bool sameMask(const std::vector<bool>& lhs, const std::vector<bool>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

StreamingMultiprocessor::StreamingMultiprocessor(std::size_t id, const GPUConfig& config)
    : id_(id),
      config_(config),
      sharedMemory_(config.sharedMemoryBytesPerSM),
      coalescer_(config.memoryLineBytes),
      scheduler_(config.schedulerPolicy),
      intAlu_(config.intAluPipelinesPerSM, 1),
      sfu_(config.sfuPipelinesPerSM, 4),
      lsu_(config.lsuPipelinesPerSM, 1),
      l1Cache_(config.cacheSizeBytes, config.cacheLineBytes, config.cacheAssociativity,
               config.l1HitLatency, config.l1MissLatency) {
    warps_.reserve(config_.warpsPerSM);
    for (std::size_t warp = 0; warp < config_.warpsPerSM; ++warp) {
        const std::size_t globalWarpId = id_ * config_.warpsPerSM + warp;
        warps_.emplace_back(config_.lanesPerWarp, config_.registersPerLane,
                            config_.predicatesPerLane, globalWarpId, id_);
    }
}

void StreamingMultiprocessor::loadProgram(const Program* program) {
    program_ = program;
}

void StreamingMultiprocessor::setWarps(std::vector<Warp> warps) {
    warps_ = std::move(warps);
    pendingWrites_.clear();
    memoryRequests_.clear();
    barriers_.clear();
    l1Cache_.reset();
}

bool StreamingMultiprocessor::active() const {
    if (!pendingWrites_.empty() || !memoryRequests_.empty()) {
        return true;
    }
    for (const auto& warp : warps_) {
        if (!warp.done()) {
            return true;
        }
    }
    return false;
}

void StreamingMultiprocessor::tick(std::uint64_t cycle, GlobalMemory& globalMemory,
                                   Stats& stats, Trace* trace) {
    completePendingWrites(cycle, stats, trace);
    completeMemoryRequests(cycle, globalMemory, stats, trace);

    for (auto& warp : warps_) {
        if (!warp.done()) {
            warp.tryReconvergeAtCurrentPc(stats);
        }
    }

    std::vector<int> readyWarps;
    std::vector<std::uint64_t> lastIssueCycles;
    lastIssueCycles.reserve(warps_.size());

    bool hasLiveWarp = false;
    for (std::size_t i = 0; i < warps_.size(); ++i) {
        const auto& warp = warps_[i];
        lastIssueCycles.push_back(warp.lastIssueCycle());
        const ReadyState state = readiness(warp, cycle);
        if (state != ReadyState::Done && state != ReadyState::NoProgram) {
            hasLiveWarp = true;
        }
        if (state == ReadyState::Ready) {
            readyWarps.push_back(static_cast<int>(i));
        } else if (state == ReadyState::WaitingGlobalMemory) {
            ++stats.globalMemoryStallCycles;
            if (trace != nullptr) {
                trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "FETCH",
                                   warp.activeMask(), "global memory");
            }
        } else if (state == ReadyState::WaitingBarrier) {
            ++stats.barrierStallCycles;
            if (trace != nullptr) {
                trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "BARRIER_WAIT",
                                   warp.activeMask(), "barrier");
            }
        } else if (state == ReadyState::ScoreboardBlocked) {
            ++stats.scoreboardStallCycles;
            if (trace != nullptr) {
                trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "FETCH",
                                   warp.activeMask(), "scoreboard");
            }
        }
    }
    ++stats.eligibleWarpSamples;
    stats.eligibleWarpTotal += readyWarps.size();

    if (readyWarps.empty()) {
        if (hasLiveWarp || !pendingWrites_.empty() || !memoryRequests_.empty()) {
            ++stats.schedulerIdleCycles;
        }
        return;
    }

    const int selectedWarp = scheduler_.select(readyWarps, lastIssueCycles);
    issue(cycle, selectedWarp, globalMemory, stats, trace);
}

StreamingMultiprocessor::ReadyState
StreamingMultiprocessor::readiness(const Warp& warp, std::uint64_t cycle) const {
    if (program_ == nullptr) {
        return ReadyState::NoProgram;
    }
    if (warp.done()) {
        return ReadyState::Done;
    }
    if (warp.waitingOnGlobalMemory()) {
        return ReadyState::WaitingGlobalMemory;
    }
    if (warp.waitingOnBarrier()) {
        return ReadyState::WaitingBarrier;
    }
    if (warp.pc() >= program_->instructions.size()) {
        return ReadyState::Done;
    }

    const Instruction& inst = currentInstruction(warp);
    if (!warp.scoreboard().canRead(inst.sourceRegisters())) {
        return ReadyState::ScoreboardBlocked;
    }
    if (!functionUnitReady(inst.unitType(), cycle)) {
        return ReadyState::FunctionalUnitBusy;
    }
    if ((inst.opcode == Opcode::LdGlobal || inst.opcode == Opcode::StGlobal) &&
        memoryRequests_.size() >= config_.maxOutstandingMemoryRequestsPerSM) {
        return ReadyState::MemoryQueueFull;
    }
    return ReadyState::Ready;
}

void StreamingMultiprocessor::issue(std::uint64_t cycle, int warpIndex,
                                    GlobalMemory& globalMemory, Stats& stats,
                                    Trace* trace) {
    (void)globalMemory;
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    Instruction inst = currentInstruction(warp);
    if (trace != nullptr) {
        trace->recordIssue(cycle, id_, warp.id(), warp.pc(), inst, warp.activeMask());
    }

    ++stats.instructionsIssued;
    stats.recordSelectedWarp(warp.id());
    warp.setLastIssueCycle(cycle + 1);

    if (handleBranch(cycle, warpIndex, inst, stats, trace)) {
        ++stats.instructionsRetired;
        return;
    }

    const UnitType unit = inst.unitType();
    const std::uint64_t readyCycle = issueFunctionUnit(unit, cycle);
    if (trace != nullptr && unit != UnitType::None) {
        trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(), "EXECUTE",
                           warp.activeMask());
    }
    std::vector<std::int32_t> regValues(warp.laneCount(), 0);
    std::vector<bool> predValues(warp.laneCount(), false);

    auto activeForLane = [&](std::size_t lane) {
        return lane < warp.activeMask().size() && warp.activeMask()[lane];
    };

    switch (inst.opcode) {
    case Opcode::Nop:
        warp.advancePc();
        ++stats.instructionsRetired;
        break;
    case Opcode::Mov:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (activeForLane(lane)) {
                regValues[lane] = warp.lane(lane).reg(static_cast<std::size_t>(inst.srcA));
            }
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovI:
        std::fill(regValues.begin(), regValues.end(), inst.imm);
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovLaneId:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            regValues[lane] = static_cast<std::int32_t>(lane);
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovWarpId:
        std::fill(regValues.begin(), regValues.end(), static_cast<std::int32_t>(warp.id()));
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovCtaId:
        std::fill(regValues.begin(), regValues.end(), static_cast<std::int32_t>(warp.ctaId()));
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovTid:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            regValues[lane] = static_cast<std::int32_t>(warp.threadId(lane));
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovGtid:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            regValues[lane] = static_cast<std::int32_t>(warp.globalThreadId(lane));
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovNtid:
        std::fill(regValues.begin(), regValues.end(), static_cast<std::int32_t>(warp.blockDim()));
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::MovNcta:
        std::fill(regValues.begin(), regValues.end(), static_cast<std::int32_t>(warp.gridDim()));
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Mad:
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    case Opcode::AddI:
    case Opcode::Shl:
    case Opcode::Shr:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (!activeForLane(lane)) {
                continue;
            }
            const auto a = warp.lane(lane).reg(static_cast<std::size_t>(inst.srcA));
            const auto b = inst.srcB >= 0 ? warp.lane(lane).reg(static_cast<std::size_t>(inst.srcB)) : 0;
            const auto c = inst.srcC >= 0 ? warp.lane(lane).reg(static_cast<std::size_t>(inst.srcC)) : 0;
            switch (inst.opcode) {
            case Opcode::Add:
                regValues[lane] = a + b;
                break;
            case Opcode::Sub:
                regValues[lane] = a - b;
                break;
            case Opcode::Mul:
                regValues[lane] = a * b;
                break;
            case Opcode::Mad:
                regValues[lane] = a * b + c;
                break;
            case Opcode::And:
                regValues[lane] = a & b;
                break;
            case Opcode::Or:
                regValues[lane] = a | b;
                break;
            case Opcode::Xor:
                regValues[lane] = a ^ b;
                break;
            case Opcode::AddI:
                regValues[lane] = a + inst.imm;
                break;
            case Opcode::Shl:
                regValues[lane] = static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(a) << static_cast<std::uint32_t>(inst.imm));
                break;
            case Opcode::Shr:
                regValues[lane] = static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(a) >> static_cast<std::uint32_t>(inst.imm));
                break;
            default:
                break;
            }
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::SetpEq:
    case Opcode::SetpNe:
    case Opcode::SetpLt:
    case Opcode::SetpGe:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (!activeForLane(lane)) {
                continue;
            }
            const auto a = warp.lane(lane).reg(static_cast<std::size_t>(inst.srcA));
            const auto b = warp.lane(lane).reg(static_cast<std::size_t>(inst.srcB));
            switch (inst.opcode) {
            case Opcode::SetpEq:
                predValues[lane] = a == b;
                break;
            case Opcode::SetpNe:
                predValues[lane] = a != b;
                break;
            case Opcode::SetpLt:
                predValues[lane] = a < b;
                break;
            case Opcode::SetpGe:
                predValues[lane] = a >= b;
                break;
            default:
                break;
            }
        }
        queuePredicateWrite(readyCycle, warpIndex, inst.predDst, warp.activeMask(), predValues);
        warp.advancePc();
        break;
    case Opcode::VoteAll:
    case Opcode::VoteAny: {
        bool anyVote = false;
        bool allVote = true;
        bool hasActive = false;
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (!activeForLane(lane)) {
                continue;
            }
            hasActive = true;
            const bool vote = warp.lane(lane).pred(static_cast<std::size_t>(inst.pred));
            anyVote = anyVote || vote;
            allVote = allVote && vote;
        }
        const bool result = inst.opcode == Opcode::VoteAll ? (hasActive && allVote) : anyVote;
        std::fill(predValues.begin(), predValues.end(), result);
        queuePredicateWrite(readyCycle, warpIndex, inst.predDst, warp.activeMask(), predValues);
        warp.advancePc();
        break;
    }
    case Opcode::Ballot: {
        std::int32_t ballot = 0;
        for (std::size_t lane = 0; lane < warp.laneCount() && lane < 31; ++lane) {
            if (activeForLane(lane) &&
                warp.lane(lane).pred(static_cast<std::size_t>(inst.pred))) {
                ballot |= (1 << lane);
            }
        }
        std::fill(regValues.begin(), regValues.end(), ballot);
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    }
    case Opcode::ShflIdx:
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (!activeForLane(lane)) {
                continue;
            }
            const auto sourceLane =
                warp.lane(lane).reg(static_cast<std::size_t>(inst.srcB));
            if (sourceLane >= 0 && static_cast<std::size_t>(sourceLane) < warp.laneCount()) {
                regValues[lane] = warp.lane(static_cast<std::size_t>(sourceLane))
                                      .reg(static_cast<std::size_t>(inst.srcA));
            }
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), regValues);
        warp.advancePc();
        break;
    case Opcode::LdGlobal:
    case Opcode::StGlobal:
        issueGlobalMemory(cycle, warpIndex, inst, stats, trace);
        break;
    case Opcode::LdShared:
    case Opcode::StShared:
        issueSharedMemory(cycle, warpIndex, inst, stats);
        break;
    case Opcode::BarSync:
        issueBarrier(cycle, warpIndex, inst, stats, trace);
        break;
    case Opcode::Halt:
    case Opcode::Bra:
    case Opcode::BraPred:
        break;
    }
}

void StreamingMultiprocessor::completePendingWrites(std::uint64_t cycle, Stats& stats,
                                                    Trace* trace) {
    auto it = pendingWrites_.begin();
    while (it != pendingWrites_.end()) {
        if (it->readyCycle > cycle) {
            ++it;
            continue;
        }

        Warp& warp = warps_.at(static_cast<std::size_t>(it->warpIndex));
        if (it->kind == PendingWrite::Kind::Register) {
            for (std::size_t lane = 0; lane < warp.laneCount() && lane < it->activeMask.size();
                 ++lane) {
                if (it->activeMask[lane]) {
                    warp.lane(lane).setReg(static_cast<std::size_t>(it->dst), it->regValues[lane]);
                }
            }
            warp.scoreboard().release(it->dst);
        } else {
            for (std::size_t lane = 0; lane < warp.laneCount() && lane < it->activeMask.size();
                 ++lane) {
                if (it->activeMask[lane]) {
                    warp.lane(lane).setPred(static_cast<std::size_t>(it->dst), it->predValues[lane]);
                }
            }
        }

        if (trace != nullptr) {
            trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "WRITEBACK",
                               it->activeMask);
        }
        ++stats.instructionsRetired;
        it = pendingWrites_.erase(it);
    }
}

void StreamingMultiprocessor::completeMemoryRequests(std::uint64_t cycle,
                                                     GlobalMemory& globalMemory,
                                                     Stats& stats, Trace* trace) {
    auto it = memoryRequests_.begin();
    while (it != memoryRequests_.end()) {
        if (it->returnCycle > cycle) {
            ++it;
            continue;
        }

        Warp& warp = warps_.at(static_cast<std::size_t>(it->warpIndex));
        if (it->isLoad) {
            for (std::size_t lane = 0; lane < warp.laneCount() && lane < it->activeMask.size();
                 ++lane) {
                if (it->activeMask[lane]) {
                    const auto value = globalMemory.read32(it->addresses[lane]);
                    warp.lane(lane).setReg(static_cast<std::size_t>(it->dstReg), value);
                }
            }
            warp.scoreboard().release(it->dstReg);
            warp.setWaitingOnGlobalMemory(false);
        } else {
            for (std::size_t lane = 0; lane < warp.laneCount() && lane < it->activeMask.size();
                 ++lane) {
                if (it->activeMask[lane]) {
                    globalMemory.write32(it->addresses[lane], it->storeValues[lane]);
                }
            }
        }

        if (trace != nullptr) {
            trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "MEMORY_RETURN",
                               it->activeMask);
        }
        ++stats.instructionsRetired;
        it = memoryRequests_.erase(it);
    }
}

void StreamingMultiprocessor::queueRegisterWrite(std::uint64_t readyCycle, int warpIndex,
                                                 int dst, const std::vector<bool>& mask,
                                                 const std::vector<std::int32_t>& values) {
    PendingWrite write;
    write.kind = PendingWrite::Kind::Register;
    write.warpIndex = warpIndex;
    write.dst = dst;
    write.readyCycle = readyCycle;
    write.activeMask = mask;
    write.regValues = values;
    pendingWrites_.push_back(write);
}

void StreamingMultiprocessor::queuePredicateWrite(std::uint64_t readyCycle, int warpIndex,
                                                  int dst, const std::vector<bool>& mask,
                                                  const std::vector<bool>& values) {
    PendingWrite write;
    write.kind = PendingWrite::Kind::Predicate;
    write.warpIndex = warpIndex;
    write.dst = dst;
    write.readyCycle = readyCycle;
    write.activeMask = mask;
    write.predValues = values;
    pendingWrites_.push_back(write);
}

std::vector<std::uint32_t> StreamingMultiprocessor::laneAddresses(
    const Warp& warp, int baseReg, std::int32_t imm) const {
    std::vector<std::uint32_t> addresses(warp.laneCount(), 0);
    for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
        addresses[lane] = static_cast<std::uint32_t>(
            warp.lane(lane).reg(static_cast<std::size_t>(baseReg)) + imm);
    }
    return addresses;
}

std::vector<std::int32_t> StreamingMultiprocessor::laneRegisterValues(const Warp& warp,
                                                                      int reg) const {
    std::vector<std::int32_t> values(warp.laneCount(), 0);
    for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
        values[lane] = warp.lane(lane).reg(static_cast<std::size_t>(reg));
    }
    return values;
}

void StreamingMultiprocessor::issueGlobalMemory(std::uint64_t cycle, int warpIndex,
                                                const Instruction& inst, Stats& stats,
                                                Trace* trace) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    const auto addresses = laneAddresses(warp, inst.srcA, inst.imm);
    const auto coalesced = coalescer_.coalesce(addresses, warp.activeMask());

    stats.memoryRequests += 1;
    stats.warpMemoryInstructions += 1;
    stats.coalescedTransactions += coalesced.transactions;
    stats.memoryLaneAccesses += coalesced.activeLaneAccesses;
    stats.requestedBytes += coalesced.requestedBytes;
    stats.transferredBytes += coalesced.transferredBytes;
    stats.wastedBytes += coalesced.wastedBytes;

    std::uint64_t memoryLatency = 0;
    for (std::uint32_t lineBase : coalesced.lineBases) {
        const CacheAccessResult access = l1Cache_.access(lineBase);
        memoryLatency = std::max(memoryLatency, access.latency);
        if (access.hit) {
            ++stats.l1Hits;
        } else {
            ++stats.l1Misses;
            if (access.latency > config_.l1HitLatency) {
                stats.cacheMissStallCycles += access.latency - config_.l1HitLatency;
            }
        }
    }
    if (memoryLatency == 0) {
        memoryLatency = config_.l1HitLatency;
    }

    MemoryRequest request;
    request.space = MemorySpace::Global;
    request.isLoad = inst.opcode == Opcode::LdGlobal;
    request.warpIndex = warpIndex;
    request.dstReg = inst.dst;
    request.returnCycle = cycle + memoryLatency;
    request.transactionCount = coalesced.transactions;
    request.requestedBytes = coalesced.requestedBytes;
    request.transferredBytes = coalesced.transferredBytes;
    request.activeMask = warp.activeMask();
    request.addresses = addresses;

    if (request.isLoad) {
        warp.scoreboard().reserve(inst.dst);
        warp.setWaitingOnGlobalMemory(true);
    } else {
        request.storeValues = laneRegisterValues(warp, inst.srcB);
    }

    memoryRequests_.push_back(request);
    if (trace != nullptr) {
        trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(), "MEMORY_REQUEST",
                           warp.activeMask());
    }
    warp.advancePc();
}

void StreamingMultiprocessor::issueSharedMemory(std::uint64_t cycle, int warpIndex,
                                                const Instruction& inst, Stats& stats) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    const auto addresses = laneAddresses(warp, inst.srcA, inst.imm);
    const auto accessInfo =
        sharedMemory_.analyzeAccess(addresses, warp.activeMask(), inst.opcode == Opcode::LdShared);
    const auto conflicts = accessInfo.conflicts;
    stats.sharedMemoryAccesses += accessInfo.activeAccesses;
    stats.sharedMemoryBankConflicts += conflicts;
    stats.sharedMemoryBankConflictEvents += accessInfo.conflictEvents;
    stats.sharedMemoryMaxConflictDegree =
        std::max(stats.sharedMemoryMaxConflictDegree, accessInfo.maxConflictDegree);
    stats.sharedMemoryConflictDegreeTotal +=
        static_cast<std::uint64_t>(accessInfo.averageConflictDegree * accessInfo.activeAccesses);

    const std::uint64_t readyCycle = cycle + config_.sharedMemoryLatency + conflicts;

    if (inst.opcode == Opcode::LdShared) {
        std::vector<std::int32_t> values(warp.laneCount(), 0);
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (warp.activeMask()[lane]) {
                values[lane] = sharedMemory_.read32(addresses[lane]);
            }
        }
        warp.scoreboard().reserve(inst.dst);
        queueRegisterWrite(readyCycle, warpIndex, inst.dst, warp.activeMask(), values);
    } else {
        const auto values = laneRegisterValues(warp, inst.srcB);
        for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
            if (warp.activeMask()[lane]) {
                sharedMemory_.write32(addresses[lane], values[lane]);
            }
        }
        ++stats.instructionsRetired;
    }
    warp.advancePc();
}

void StreamingMultiprocessor::issueBarrier(std::uint64_t cycle, int warpIndex,
                                           const Instruction& inst, Stats& stats,
                                           Trace* trace) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    BarrierState& barrier = barriers_[warp.ctaId()];
    if (barrier.expectedWarps == 0) {
        barrier.expectedWarps = activeWarpCountForCTA(warp.ctaId());
    }
    barrier.waitingWarps.insert(warpIndex);
    warp.setWaitingOnBarrier(true);
    warp.advancePc();
    ++stats.barrierCount;
    if (trace != nullptr) {
        trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(), "BARRIER_WAIT",
                           warp.activeMask());
    }
    releaseBarrierIfReady(cycle, warp.ctaId(), stats, trace);
}

void StreamingMultiprocessor::releaseBarrierIfReady(std::uint64_t cycle, std::size_t ctaId,
                                                    Stats& stats, Trace* trace) {
    auto it = barriers_.find(ctaId);
    if (it == barriers_.end()) {
        return;
    }
    BarrierState& barrier = it->second;
    if (barrier.expectedWarps == 0 || barrier.waitingWarps.size() < barrier.expectedWarps) {
        return;
    }

    const std::size_t releasedWarps = barrier.waitingWarps.size();
    for (int warpIndex : barrier.waitingWarps) {
        Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
        warp.setWaitingOnBarrier(false);
        if (trace != nullptr) {
            trace->recordEvent(cycle, id_, warp.id(), nullptr, warp.pc(), "BARRIER_RELEASE",
                               warp.activeMask());
        }
    }
    stats.instructionsRetired += releasedWarps;
    barriers_.erase(it);
}

std::size_t StreamingMultiprocessor::activeWarpCountForCTA(std::size_t ctaId) const {
    std::size_t count = 0;
    for (const Warp& warp : warps_) {
        if (warp.ctaId() == ctaId && !warp.done()) {
            ++count;
        }
    }
    return count == 0 ? 1 : count;
}

bool StreamingMultiprocessor::handleBranch(std::uint64_t cycle, int warpIndex,
                                           const Instruction& inst, Stats& stats,
                                           Trace* trace) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));

    if (inst.opcode == Opcode::Halt) {
        const bool hadDeferredPath = !warp.divergenceStack().empty();
        warp.haltOrDeferToDivergenceStack(stats);
        if (trace != nullptr) {
            trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(),
                               hadDeferredPath ? "RECONVERGE" : "HALT", warp.activeMask());
        }
        return true;
    }

    if (inst.opcode == Opcode::Bra) {
        if (!warp.divergenceStack().empty()) {
            DivergenceEntry& entry = warp.divergenceStack().top();
            if (!entry.hasReconvergePc) {
                entry.hasReconvergePc = true;
                entry.reconvergePc = inst.target;
                entry.completedMask = warp.activeMask();
                warp.setActiveMask(entry.deferredMask);
                warp.setPc(entry.deferredPc);
                if (trace != nullptr) {
                    trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(), "RECONVERGE",
                                       warp.activeMask());
                }
                return true;
            }
        }
        warp.setPc(inst.target);
        return true;
    }

    if (inst.opcode != Opcode::BraPred) {
        return false;
    }

    std::vector<bool> takeMask(warp.laneCount(), false);
    std::vector<bool> fallMask(warp.laneCount(), false);

    for (std::size_t lane = 0; lane < warp.laneCount(); ++lane) {
        if (!warp.activeMask()[lane]) {
            continue;
        }
        if (warp.lane(lane).pred(static_cast<std::size_t>(inst.pred))) {
            takeMask[lane] = true;
        } else {
            fallMask[lane] = true;
        }
    }

    const bool anyTake = any(takeMask);
    const bool anyFall = any(fallMask);

    if (anyTake && anyFall) {
        DivergenceEntry entry;
        entry.deferredPc = warp.pc() + 1;
        entry.deferredMask = fallMask;
        warp.divergenceStack().push(entry);
        warp.setActiveMask(takeMask);
        warp.setPc(inst.target);
        ++stats.branchDivergenceEvents;
        if (trace != nullptr) {
            trace->recordEvent(cycle, id_, warp.id(), &inst, warp.pc(), "DIVERGE",
                               warp.activeMask());
        }
    } else if (anyTake) {
        warp.setActiveMask(takeMask);
        warp.setPc(inst.target);
    } else {
        warp.setActiveMask(fallMask);
        warp.advancePc();
    }
    return true;
}

const Instruction& StreamingMultiprocessor::currentInstruction(const Warp& warp) const {
    if (program_ == nullptr || warp.pc() >= program_->instructions.size()) {
        throw std::runtime_error("warp PC is outside program");
    }
    return program_->instructions[warp.pc()];
}

bool StreamingMultiprocessor::functionUnitReady(UnitType unit, std::uint64_t cycle) const {
    switch (unit) {
    case UnitType::None:
        return true;
    case UnitType::IntAlu:
        return intAlu_.canIssue(cycle);
    case UnitType::Sfu:
        return sfu_.canIssue(cycle);
    case UnitType::Lsu:
        return lsu_.canIssue(cycle);
    }
    return false;
}

std::uint64_t StreamingMultiprocessor::issueFunctionUnit(UnitType unit, std::uint64_t cycle) {
    switch (unit) {
    case UnitType::None:
        return cycle;
    case UnitType::IntAlu:
        return intAlu_.issue(cycle);
    case UnitType::Sfu:
        return sfu_.issue(cycle);
    case UnitType::Lsu:
        return lsu_.issue(cycle);
    }
    return cycle;
}

} // namespace simt
