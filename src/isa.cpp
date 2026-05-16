#include "isa.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace simt {

std::string normalizeMnemonic(const std::string& mnemonic) {
    std::string out = mnemonic;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return out;
}

Opcode opcodeFromMnemonic(const std::string& mnemonic) {
    static const std::unordered_map<std::string, Opcode> opcodes = {
        {"NOP", Opcode::Nop},
        {"HALT", Opcode::Halt},
        {"BRA", Opcode::Bra},
        {"BRA.P", Opcode::BraPred},
        {"MOV", Opcode::Mov},
        {"MOVI", Opcode::MovI},
        {"ADD", Opcode::Add},
        {"ADDI", Opcode::AddI},
        {"SUB", Opcode::Sub},
        {"MUL", Opcode::Mul},
        {"MAD", Opcode::Mad},
        {"AND", Opcode::And},
        {"OR", Opcode::Or},
        {"XOR", Opcode::Xor},
        {"SHL", Opcode::Shl},
        {"SHR", Opcode::Shr},
        {"SETP.EQ", Opcode::SetpEq},
        {"SETP.NE", Opcode::SetpNe},
        {"SETP.LT", Opcode::SetpLt},
        {"SETP.GE", Opcode::SetpGe},
        {"LD.GLOBAL", Opcode::LdGlobal},
        {"ST.GLOBAL", Opcode::StGlobal},
        {"LD.SHARED", Opcode::LdShared},
        {"ST.SHARED", Opcode::StShared},
        {"MOV.LANEID", Opcode::MovLaneId},
        {"MOV.WARPID", Opcode::MovWarpId},
        {"MOV.CTAID", Opcode::MovCtaId},
        {"MOV.NTID", Opcode::MovNtid},
    };

    const auto normalized = normalizeMnemonic(mnemonic);
    const auto it = opcodes.find(normalized);
    if (it == opcodes.end()) {
        throw std::runtime_error("unknown opcode mnemonic: " + mnemonic);
    }
    return it->second;
}

} // namespace simt
