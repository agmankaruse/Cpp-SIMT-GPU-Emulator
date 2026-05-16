#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace simt {

enum class Opcode {
    Nop,
    Halt,
    Bra,
    BraPred,
    Mov,
    MovI,
    Add,
    AddI,
    Sub,
    Mul,
    Mad,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    SetpEq,
    SetpNe,
    SetpLt,
    SetpGe,
    LdGlobal,
    StGlobal,
    LdShared,
    StShared,
    MovLaneId,
    MovWarpId,
    MovCtaId,
    MovTid,
    MovGtid,
    MovNtid,
    MovNcta,
    VoteAll,
    VoteAny,
    Ballot,
    ShflIdx,
    BarSync
};

enum class UnitType {
    None,
    IntAlu,
    Sfu,
    Lsu
};

struct Instruction {
    Opcode opcode = Opcode::Nop;
    int dst = -1;
    int srcA = -1;
    int srcB = -1;
    int srcC = -1;
    int pred = -1;
    int predDst = -1;
    std::int32_t imm = 0;
    std::size_t target = 0;
    std::string label;
    std::string text;

    bool isLoad() const;
    bool isStore() const;
    bool writesRegister() const;
    bool writesPredicate() const;
    bool isBranch() const;
    UnitType unitType() const;
    std::vector<int> sourceRegisters() const;
};

std::string opcodeName(Opcode opcode);

} // namespace simt
