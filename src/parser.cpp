#include "parser.hpp"

#include "isa.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace simt {
namespace {

std::string trim(const std::string& text) {
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string stripComment(const std::string& line) {
    std::size_t cut = std::string::npos;
    for (const std::string marker : {"//", "#", ";"}) {
        const auto pos = line.find(marker);
        if (pos != std::string::npos) {
            cut = std::min(cut, pos);
        }
    }
    return trim(line.substr(0, cut));
}

std::vector<std::string> splitOperands(const std::string& text) {
    std::vector<std::string> operands;
    std::string current;
    int bracketDepth = 0;
    for (char c : text) {
        if (c == '[') {
            ++bracketDepth;
        } else if (c == ']') {
            --bracketDepth;
        }

        if (c == ',' && bracketDepth == 0) {
            operands.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!trim(current).empty()) {
        operands.push_back(trim(current));
    }
    return operands;
}

int parseRegister(const std::string& text) {
    if (text.size() < 2 || (text[0] != 'r' && text[0] != 'R')) {
        throw std::runtime_error("expected register, got: " + text);
    }
    return std::stoi(text.substr(1));
}

int parsePredicate(const std::string& text) {
    if (text.size() < 2 || (text[0] != 'p' && text[0] != 'P')) {
        throw std::runtime_error("expected predicate, got: " + text);
    }
    return std::stoi(text.substr(1));
}

std::int32_t parseImmediate(const std::string& text) {
    return static_cast<std::int32_t>(std::stol(text, nullptr, 0));
}

void parseMemoryOperand(const std::string& text, int& baseReg, std::int32_t& imm) {
    const auto open = text.find('[');
    const auto close = text.find(']');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        throw std::runtime_error("expected memory operand [rA + imm], got: " + text);
    }
    std::string inner = trim(text.substr(open + 1, close - open - 1));
    for (char& c : inner) {
        if (c == '+') {
            c = ' ';
        }
    }
    std::istringstream parts(inner);
    std::string regToken;
    parts >> regToken;
    baseReg = parseRegister(regToken);
    std::string immToken;
    if (parts >> immToken) {
        imm = parseImmediate(immToken);
    } else {
        imm = 0;
    }
}

} // namespace

Program Parser::parseText(const std::string& text) {
    Program program;
    std::istringstream input(text);
    std::string rawLine;
    std::size_t lineNumber = 0;

    struct UnresolvedBranch {
        std::size_t instructionIndex = 0;
        std::string label;
    };
    std::vector<UnresolvedBranch> unresolved;

    while (std::getline(input, rawLine)) {
        ++lineNumber;
        std::string line = stripComment(rawLine);
        if (line.empty()) {
            continue;
        }

        if (line.rfind(".kernel", 0) == 0) {
            std::istringstream kernelLine(line);
            std::string directive;
            kernelLine >> directive >> program.kernelName;
            if (program.kernelName.empty()) {
                throw std::runtime_error(".kernel requires a name");
            }
            continue;
        }

        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string label = trim(line.substr(0, colon));
            if (label.empty()) {
                throw std::runtime_error("empty label on line " + std::to_string(lineNumber));
            }
            program.labels[label] = program.instructions.size();
            line = trim(line.substr(colon + 1));
            if (line.empty()) {
                continue;
            }
        }

        Instruction instruction = parseInstruction(line);
        if (!instruction.label.empty()) {
            unresolved.push_back({program.instructions.size(), instruction.label});
        }
        program.instructions.push_back(instruction);
    }

    for (const auto& branch : unresolved) {
        const auto it = program.labels.find(branch.label);
        if (it == program.labels.end()) {
            throw std::runtime_error("unknown branch label: " + branch.label);
        }
        program.instructions[branch.instructionIndex].target = it->second;
    }

    if (program.instructions.empty()) {
        throw std::runtime_error("program has no instructions");
    }
    return program;
}

Program Parser::parseFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("unable to open program: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseText(buffer.str());
}

Instruction Parser::parseInstruction(const std::string& line) {
    std::istringstream head(line);
    std::string mnemonic;
    head >> mnemonic;
    const auto opcode = opcodeFromMnemonic(mnemonic);
    std::string rest;
    std::getline(head, rest);

    Instruction inst;
    inst.opcode = opcode;
    inst.text = line;
    const auto operands = splitOperands(rest);

    auto requireCount = [&](std::size_t count) {
        if (operands.size() != count) {
            throw std::runtime_error("wrong operand count for " + mnemonic + ": " + line);
        }
    };

    switch (opcode) {
    case Opcode::Nop:
    case Opcode::Halt:
        requireCount(0);
        break;
    case Opcode::Bra:
        requireCount(1);
        inst.label = operands[0];
        break;
    case Opcode::BraPred:
        requireCount(2);
        inst.pred = parsePredicate(operands[0]);
        inst.label = operands[1];
        break;
    case Opcode::Mov:
        requireCount(2);
        inst.dst = parseRegister(operands[0]);
        inst.srcA = parseRegister(operands[1]);
        break;
    case Opcode::MovI:
        requireCount(2);
        inst.dst = parseRegister(operands[0]);
        inst.imm = parseImmediate(operands[1]);
        break;
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
        requireCount(3);
        inst.dst = parseRegister(operands[0]);
        inst.srcA = parseRegister(operands[1]);
        inst.srcB = parseRegister(operands[2]);
        break;
    case Opcode::AddI:
    case Opcode::Shl:
    case Opcode::Shr:
        requireCount(3);
        inst.dst = parseRegister(operands[0]);
        inst.srcA = parseRegister(operands[1]);
        inst.imm = parseImmediate(operands[2]);
        break;
    case Opcode::Mad:
        requireCount(4);
        inst.dst = parseRegister(operands[0]);
        inst.srcA = parseRegister(operands[1]);
        inst.srcB = parseRegister(operands[2]);
        inst.srcC = parseRegister(operands[3]);
        break;
    case Opcode::SetpEq:
    case Opcode::SetpNe:
    case Opcode::SetpLt:
    case Opcode::SetpGe:
        requireCount(3);
        inst.predDst = parsePredicate(operands[0]);
        inst.srcA = parseRegister(operands[1]);
        inst.srcB = parseRegister(operands[2]);
        break;
    case Opcode::LdGlobal:
    case Opcode::LdShared:
        requireCount(2);
        inst.dst = parseRegister(operands[0]);
        parseMemoryOperand(operands[1], inst.srcA, inst.imm);
        break;
    case Opcode::StGlobal:
    case Opcode::StShared:
        requireCount(2);
        parseMemoryOperand(operands[0], inst.srcA, inst.imm);
        inst.srcB = parseRegister(operands[1]);
        break;
    case Opcode::MovLaneId:
    case Opcode::MovWarpId:
    case Opcode::MovCtaId:
    case Opcode::MovNtid:
        requireCount(1);
        inst.dst = parseRegister(operands[0]);
        break;
    }

    return inst;
}

} // namespace simt
