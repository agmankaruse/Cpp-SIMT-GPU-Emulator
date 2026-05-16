#pragma once

#include "instruction.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace simt {

struct Program {
    std::string kernelName = "unnamed";
    std::vector<Instruction> instructions;
    std::unordered_map<std::string, std::size_t> labels;
};

class Parser {
public:
    static Program parseText(const std::string& text);
    static Program parseFile(const std::string& path);

private:
    static Instruction parseInstruction(const std::string& line);
};

} // namespace simt
