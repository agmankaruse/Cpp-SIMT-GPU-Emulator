#include "instruction.hpp"

#include <stdexcept>

namespace simt {

bool Instruction::isLoad() const {
    return opcode == Opcode::LdGlobal || opcode == Opcode::LdShared;
}

bool Instruction::isStore() const {
    return opcode == Opcode::StGlobal || opcode == Opcode::StShared;
}

bool Instruction::writesRegister() const {
    switch (opcode) {
    case Opcode::Mov:
    case Opcode::MovI:
    case Opcode::Add:
    case Opcode::AddI:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Mad:
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    case Opcode::Shl:
    case Opcode::Shr:
    case Opcode::LdGlobal:
    case Opcode::LdShared:
    case Opcode::MovLaneId:
    case Opcode::MovWarpId:
    case Opcode::MovCtaId:
    case Opcode::MovTid:
    case Opcode::MovGtid:
    case Opcode::MovNtid:
    case Opcode::MovNcta:
    case Opcode::Ballot:
    case Opcode::ShflIdx:
        return dst >= 0;
    case Opcode::BarSync:
        return false;
    default:
        return false;
    }
}

bool Instruction::writesPredicate() const {
    switch (opcode) {
    case Opcode::SetpEq:
    case Opcode::SetpNe:
    case Opcode::SetpLt:
    case Opcode::SetpGe:
    case Opcode::VoteAll:
    case Opcode::VoteAny:
        return predDst >= 0;
    default:
        return false;
    }
}

bool Instruction::isBranch() const {
    return opcode == Opcode::Bra || opcode == Opcode::BraPred;
}

UnitType Instruction::unitType() const {
    switch (opcode) {
    case Opcode::Mul:
    case Opcode::Mad:
        return UnitType::Sfu;
    case Opcode::LdGlobal:
    case Opcode::StGlobal:
    case Opcode::LdShared:
    case Opcode::StShared:
        return UnitType::Lsu;
    case Opcode::Nop:
    case Opcode::Halt:
    case Opcode::Bra:
    case Opcode::BraPred:
    case Opcode::BarSync:
        return UnitType::None;
    default:
        return UnitType::IntAlu;
    }
}

std::vector<int> Instruction::sourceRegisters() const {
    switch (opcode) {
    case Opcode::Mov:
        return {srcA};
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    case Opcode::SetpEq:
    case Opcode::SetpNe:
    case Opcode::SetpLt:
    case Opcode::SetpGe:
        return {srcA, srcB};
    case Opcode::AddI:
    case Opcode::Shl:
    case Opcode::Shr:
    case Opcode::LdGlobal:
    case Opcode::LdShared:
        return {srcA};
    case Opcode::Mad:
        return {srcA, srcB, srcC};
    case Opcode::StGlobal:
    case Opcode::StShared:
        return {srcA, srcB};
    case Opcode::ShflIdx:
        return {srcA, srcB};
    default:
        return {};
    }
}

std::string opcodeName(Opcode opcode) {
    switch (opcode) {
    case Opcode::Nop:
        return "NOP";
    case Opcode::Halt:
        return "HALT";
    case Opcode::Bra:
        return "BRA";
    case Opcode::BraPred:
        return "BRA.P";
    case Opcode::Mov:
        return "MOV";
    case Opcode::MovI:
        return "MOVI";
    case Opcode::Add:
        return "ADD";
    case Opcode::AddI:
        return "ADDI";
    case Opcode::Sub:
        return "SUB";
    case Opcode::Mul:
        return "MUL";
    case Opcode::Mad:
        return "MAD";
    case Opcode::And:
        return "AND";
    case Opcode::Or:
        return "OR";
    case Opcode::Xor:
        return "XOR";
    case Opcode::Shl:
        return "SHL";
    case Opcode::Shr:
        return "SHR";
    case Opcode::SetpEq:
        return "SETP.EQ";
    case Opcode::SetpNe:
        return "SETP.NE";
    case Opcode::SetpLt:
        return "SETP.LT";
    case Opcode::SetpGe:
        return "SETP.GE";
    case Opcode::LdGlobal:
        return "LD.GLOBAL";
    case Opcode::StGlobal:
        return "ST.GLOBAL";
    case Opcode::LdShared:
        return "LD.SHARED";
    case Opcode::StShared:
        return "ST.SHARED";
    case Opcode::MovLaneId:
        return "MOV.LANEID";
    case Opcode::MovWarpId:
        return "MOV.WARPID";
    case Opcode::MovCtaId:
        return "MOV.CTAID";
    case Opcode::MovTid:
        return "MOV.TID";
    case Opcode::MovGtid:
        return "MOV.GTID";
    case Opcode::MovNtid:
        return "MOV.NTID";
    case Opcode::MovNcta:
        return "MOV.NCTA";
    case Opcode::VoteAll:
        return "VOTE.ALL";
    case Opcode::VoteAny:
        return "VOTE.ANY";
    case Opcode::Ballot:
        return "BALLOT";
    case Opcode::ShflIdx:
        return "SHFL.IDX";
    case Opcode::BarSync:
        return "BAR.SYNC";
    }
    throw std::runtime_error("unknown opcode");
}

} // namespace simt
