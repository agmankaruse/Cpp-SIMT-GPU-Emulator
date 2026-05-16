#include "sm.hpp"

#include <algorithm>
#include <stdexcept>

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
      lsu_(config.lsuPipelinesPerSM, 1) {
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
    completePendingWrites(cycle, stats);
    completeMemoryRequests(cycle, globalMemory, stats);

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
        } else if (state == ReadyState::ScoreboardBlocked) {
            ++stats.scoreboardStallCycles;
        }
    }

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
    warp.setLastIssueCycle(cycle + 1);

    if (handleBranch(cycle, warpIndex, inst, stats)) {
        ++stats.instructionsRetired;
        return;
    }

    const UnitType unit = inst.unitType();
    const std::uint64_t readyCycle = issueFunctionUnit(unit, cycle);
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
    case Opcode::MovNtid:
        std::fill(regValues.begin(), regValues.end(), static_cast<std::int32_t>(warp.laneCount()));
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
    case Opcode::LdGlobal:
    case Opcode::StGlobal:
        issueGlobalMemory(cycle, warpIndex, inst, stats);
        break;
    case Opcode::LdShared:
    case Opcode::StShared:
        issueSharedMemory(cycle, warpIndex, inst, stats);
        break;
    case Opcode::Halt:
    case Opcode::Bra:
    case Opcode::BraPred:
        break;
    }
}

void StreamingMultiprocessor::completePendingWrites(std::uint64_t cycle, Stats& stats) {
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

        ++stats.instructionsRetired;
        it = pendingWrites_.erase(it);
    }
}

void StreamingMultiprocessor::completeMemoryRequests(std::uint64_t cycle,
                                                     GlobalMemory& globalMemory,
                                                     Stats& stats) {
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
                                                const Instruction& inst, Stats& stats) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    const auto addresses = laneAddresses(warp, inst.srcA, inst.imm);
    const auto coalesced = coalescer_.coalesce(addresses, warp.activeMask());

    stats.memoryRequests += 1;
    stats.coalescedTransactions += coalesced.transactions;
    stats.memoryLaneAccesses += coalesced.activeLaneAccesses;

    MemoryRequest request;
    request.space = MemorySpace::Global;
    request.isLoad = inst.opcode == Opcode::LdGlobal;
    request.warpIndex = warpIndex;
    request.dstReg = inst.dst;
    request.returnCycle = cycle + config_.globalMemoryLatency;
    request.transactionCount = coalesced.transactions;
    request.activeMask = warp.activeMask();
    request.addresses = addresses;

    if (request.isLoad) {
        warp.scoreboard().reserve(inst.dst);
        warp.setWaitingOnGlobalMemory(true);
    } else {
        request.storeValues = laneRegisterValues(warp, inst.srcB);
    }

    memoryRequests_.push_back(request);
    warp.advancePc();
}

void StreamingMultiprocessor::issueSharedMemory(std::uint64_t cycle, int warpIndex,
                                                const Instruction& inst, Stats& stats) {
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));
    const auto addresses = laneAddresses(warp, inst.srcA, inst.imm);
    const auto conflicts = sharedMemory_.bankConflicts(addresses, warp.activeMask());
    stats.sharedMemoryBankConflicts += conflicts;

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

bool StreamingMultiprocessor::handleBranch(std::uint64_t cycle, int warpIndex,
                                           const Instruction& inst, Stats& stats) {
    (void)cycle;
    Warp& warp = warps_.at(static_cast<std::size_t>(warpIndex));

    if (inst.opcode == Opcode::Halt) {
        warp.haltOrDeferToDivergenceStack(stats);
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
