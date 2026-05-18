#include "reference_gpu.hpp"

#include "gpu.hpp"
#include "memory.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>

namespace simt {
namespace {

std::size_t gridDimFor(const GPUConfig& config) {
    return config.gridDim == 0 ? config.smCount : config.gridDim;
}

std::size_t blockDimFor(const GPUConfig& config) {
    return config.blockDim == 0 ? config.warpsPerSM * config.lanesPerWarp : config.blockDim;
}

std::size_t warpsPerCTAFor(const GPUConfig& config) {
    const auto blockDim = blockDimFor(config);
    return (blockDim + config.lanesPerWarp - 1) / config.lanesPerWarp;
}

std::map<std::uint32_t, std::int32_t> memoryMap(const GlobalMemory& memory) {
    std::map<std::uint32_t, std::int32_t> result;
    for (const auto& [address, value] : memory.words()) {
        result[address] = value;
    }
    return result;
}

} // namespace

ReferenceGPU::ReferenceGPU(const GPUConfig& config)
    : config_(config), state_{GlobalMemory(config.globalMemoryBytes), {}, 0} {}

void ReferenceGPU::loadProgram(const Program& program) {
    program_ = program;
    const auto totalThreads = gridDimFor(config_) * blockDimFor(config_);
    state_.threads.assign(totalThreads, {});
    for (auto& thread : state_.threads) {
        thread.registers.assign(config_.registersPerLane, 0);
        thread.predicates.assign(config_.predicatesPerLane, false);
    }
    state_.instructionCount = 0;
}

void initializeGlobalMemory(GlobalMemory& memory) {
    for (std::uint32_t address = 0; address + 4 <= memory.size(); address += 4) {
        memory.write32(address, static_cast<std::int32_t>(address / 4));
    }
}

void ReferenceGPU::initializeGlobalMemory() {
    simt::initializeGlobalMemory(state_.globalMemory);
}

void ReferenceGPU::run(std::uint64_t maxInstructions) {
    if (program_.instructions.empty()) {
        throw std::runtime_error("reference GPU has no program loaded");
    }

    const auto gridDim = gridDimFor(config_);
    const auto blockDim = blockDimFor(config_);
    std::uint64_t executed = 0;
    for (std::size_t cta = 0; cta < gridDim; ++cta) {
        for (std::size_t localThread = 0; localThread < blockDim; ++localThread) {
            const std::size_t threadId = cta * blockDim + localThread;
            executeThread(threadId, cta, localThread);
            executed = state_.instructionCount;
            if (executed > maxInstructions) {
                throw std::runtime_error("reference GPU exceeded instruction limit");
            }
        }
    }
}

std::int32_t ReferenceGPU::threadRegister(std::size_t threadId, std::size_t reg) const {
    return state_.threads.at(threadId).registers.at(reg);
}

void ReferenceGPU::executeThread(std::size_t threadId, std::size_t ctaId,
                                 std::size_t localThreadId) {
    auto& thread = state_.threads.at(threadId);
    ByteAddressableMemory shared(config_.sharedMemoryBytesPerSM);
    std::size_t pc = 0;
    const auto blockDim = blockDimFor(config_);
    const auto warpsPerCTA = warpsPerCTAFor(config_);

    auto reg = [&](int index) -> std::int32_t {
        return index < 0 ? 0 : thread.registers.at(static_cast<std::size_t>(index));
    };
    auto setReg = [&](int index, std::int32_t value) {
        if (index >= 0) {
            thread.registers.at(static_cast<std::size_t>(index)) = value;
        }
    };
    auto pred = [&](int index) -> bool {
        return index >= 0 && thread.predicates.at(static_cast<std::size_t>(index));
    };
    auto setPred = [&](int index, bool value) {
        if (index >= 0) {
            thread.predicates.at(static_cast<std::size_t>(index)) = value;
        }
    };

    while (pc < program_.instructions.size()) {
        const Instruction& inst = program_.instructions[pc];
        ++state_.instructionCount;
        const std::size_t laneId = localThreadId % config_.lanesPerWarp;
        const std::size_t blockWarp = localThreadId / config_.lanesPerWarp;
        const std::size_t warpId = ctaId * warpsPerCTA + blockWarp;

        switch (inst.opcode) {
        case Opcode::Nop:
            ++pc;
            break;
        case Opcode::Halt:
            return;
        case Opcode::Bra:
            pc = inst.target;
            break;
        case Opcode::BraPred:
            pc = pred(inst.pred) ? inst.target : pc + 1;
            break;
        case Opcode::Mov:
            setReg(inst.dst, reg(inst.srcA));
            ++pc;
            break;
        case Opcode::MovI:
            setReg(inst.dst, inst.imm);
            ++pc;
            break;
        case Opcode::Add:
            setReg(inst.dst, reg(inst.srcA) + reg(inst.srcB));
            ++pc;
            break;
        case Opcode::AddI:
            setReg(inst.dst, reg(inst.srcA) + inst.imm);
            ++pc;
            break;
        case Opcode::Sub:
            setReg(inst.dst, reg(inst.srcA) - reg(inst.srcB));
            ++pc;
            break;
        case Opcode::Mul:
            setReg(inst.dst, reg(inst.srcA) * reg(inst.srcB));
            ++pc;
            break;
        case Opcode::Mad:
            setReg(inst.dst, reg(inst.srcA) * reg(inst.srcB) + reg(inst.srcC));
            ++pc;
            break;
        case Opcode::And:
            setReg(inst.dst, reg(inst.srcA) & reg(inst.srcB));
            ++pc;
            break;
        case Opcode::Or:
            setReg(inst.dst, reg(inst.srcA) | reg(inst.srcB));
            ++pc;
            break;
        case Opcode::Xor:
            setReg(inst.dst, reg(inst.srcA) ^ reg(inst.srcB));
            ++pc;
            break;
        case Opcode::Shl:
            setReg(inst.dst, static_cast<std::int32_t>(
                                 static_cast<std::uint32_t>(reg(inst.srcA)) << static_cast<std::uint32_t>(inst.imm)));
            ++pc;
            break;
        case Opcode::Shr:
            setReg(inst.dst, static_cast<std::int32_t>(
                                 static_cast<std::uint32_t>(reg(inst.srcA)) >> static_cast<std::uint32_t>(inst.imm)));
            ++pc;
            break;
        case Opcode::SetpEq:
            setPred(inst.predDst, reg(inst.srcA) == reg(inst.srcB));
            ++pc;
            break;
        case Opcode::SetpNe:
            setPred(inst.predDst, reg(inst.srcA) != reg(inst.srcB));
            ++pc;
            break;
        case Opcode::SetpLt:
            setPred(inst.predDst, reg(inst.srcA) < reg(inst.srcB));
            ++pc;
            break;
        case Opcode::SetpGe:
            setPred(inst.predDst, reg(inst.srcA) >= reg(inst.srcB));
            ++pc;
            break;
        case Opcode::LdGlobal:
            setReg(inst.dst, state_.globalMemory.read32(static_cast<std::uint32_t>(reg(inst.srcA) + inst.imm)));
            ++pc;
            break;
        case Opcode::StGlobal:
            state_.globalMemory.write32(static_cast<std::uint32_t>(reg(inst.srcA) + inst.imm), reg(inst.srcB));
            ++pc;
            break;
        case Opcode::LdShared:
            setReg(inst.dst, shared.read32(static_cast<std::uint32_t>(reg(inst.srcA) + inst.imm)));
            ++pc;
            break;
        case Opcode::StShared:
            shared.write32(static_cast<std::uint32_t>(reg(inst.srcA) + inst.imm), reg(inst.srcB));
            ++pc;
            break;
        case Opcode::MovLaneId:
            setReg(inst.dst, static_cast<std::int32_t>(laneId));
            ++pc;
            break;
        case Opcode::MovWarpId:
            setReg(inst.dst, static_cast<std::int32_t>(warpId));
            ++pc;
            break;
        case Opcode::MovCtaId:
            setReg(inst.dst, static_cast<std::int32_t>(ctaId));
            ++pc;
            break;
        case Opcode::MovTid:
            setReg(inst.dst, static_cast<std::int32_t>(localThreadId));
            ++pc;
            break;
        case Opcode::MovGtid:
            setReg(inst.dst, static_cast<std::int32_t>(threadId));
            ++pc;
            break;
        case Opcode::MovNtid:
            setReg(inst.dst, static_cast<std::int32_t>(blockDim));
            ++pc;
            break;
        case Opcode::MovNcta:
            setReg(inst.dst, static_cast<std::int32_t>(gridDimFor(config_)));
            ++pc;
            break;
        case Opcode::VoteAll:
        case Opcode::VoteAny:
            setPred(inst.predDst, pred(inst.pred));
            ++pc;
            break;
        case Opcode::Ballot:
            setReg(inst.dst, pred(inst.pred) ? (1 << laneId) : 0);
            ++pc;
            break;
        case Opcode::ShflIdx:
            setReg(inst.dst, reg(inst.srcA));
            ++pc;
            break;
        case Opcode::BarSync:
            ++pc;
            break;
        }
    }
}

GpuDiffResult runGpuDifferentialTest(const Program& program, const GPUConfig& config) {
    GPU gpu(config);
    initializeGlobalMemory(gpu.globalMemory());
    gpu.loadProgram(program);
    const Stats stats = gpu.run(false, 1'000'000, {});

    ReferenceGPU reference(config);
    reference.loadProgram(program);
    reference.initializeGlobalMemory();
    reference.run();

    GpuDiffResult result;
    result.kernelName = program.kernelName;
    result.cycleCount = stats.totalCycles;
    result.emulatorInstructionCount = stats.instructionsRetired;
    result.referenceInstructionCount = reference.state().instructionCount;

    const auto emulatorMemory = memoryMap(gpu.globalMemory());
    const auto referenceMemory = memoryMap(reference.globalMemory());
    for (const auto& [address, emulatorValue] : emulatorMemory) {
        const auto referenceValue = referenceMemory.at(address);
        if (emulatorValue != referenceValue) {
            result.mismatches.push_back({"global[" + std::to_string(address) + "]",
                                         emulatorValue,
                                         referenceValue,
                                         0,
                                         0,
                                         0});
            if (result.mismatches.size() >= 32) {
                break;
            }
        }
    }

    for (std::size_t smIndex = 0; smIndex < gpu.smCount() && result.mismatches.size() < 32; ++smIndex) {
        const auto& sm = gpu.sm(smIndex);
        for (std::size_t warpIndex = 0; warpIndex < sm.warpCount() && result.mismatches.size() < 32; ++warpIndex) {
            const auto& warp = sm.warp(warpIndex);
            for (std::size_t lane = 0; lane < warp.laneCount() && result.mismatches.size() < 32; ++lane) {
                const auto threadId = warp.globalThreadId(lane);
                if (threadId >= reference.state().threads.size()) {
                    continue;
                }
                for (std::size_t reg = 0; reg < std::min<std::size_t>(16, config.registersPerLane); ++reg) {
                    const auto emulatorValue = warp.lane(lane).reg(reg);
                    const auto referenceValue = reference.threadRegister(threadId, reg);
                    if (emulatorValue != referenceValue) {
                        result.mismatches.push_back({"r" + std::to_string(reg),
                                                     emulatorValue,
                                                     referenceValue,
                                                     threadId,
                                                     lane,
                                                     warp.id()});
                        break;
                    }
                }
            }
        }
    }

    result.passed = result.mismatches.empty();
    return result;
}

GpuDiffResult runGpuDifferentialTestFromFile(const std::string& path, const GPUConfig& config) {
    return runGpuDifferentialTest(Parser::parseFile(path), config);
}

std::string formatGpuDiffResult(const GpuDiffResult& result) {
    std::ostringstream out;
    out << (result.passed ? "PASS" : "FAIL")
        << " diff kernel=" << result.kernelName
        << " cycles=" << result.cycleCount
        << " emulator_instructions=" << result.emulatorInstructionCount
        << " reference_instructions=" << result.referenceInstructionCount << '\n';
    for (const auto& mismatch : result.mismatches) {
        out << "mismatch thread=" << mismatch.threadId
            << " lane=" << mismatch.laneId
            << " warp=" << mismatch.warpId
            << " location=" << mismatch.location
            << " emulator=" << mismatch.emulatorValue
            << " reference=" << mismatch.referenceValue << '\n';
    }
    return out.str();
}

} // namespace simt
