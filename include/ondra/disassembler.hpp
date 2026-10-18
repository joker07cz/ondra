// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
namespace ondra {
class Machine;
struct Instruction { std::string text; unsigned length; };
Instruction disassemble(Machine& machine, std::uint16_t address);
}
