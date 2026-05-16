#pragma once

#include "instruction.hpp"

#include <string>

namespace simt {

Opcode opcodeFromMnemonic(const std::string& mnemonic);
std::string normalizeMnemonic(const std::string& mnemonic);

} // namespace simt
