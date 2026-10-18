// SPDX-License-Identifier: GPL-3.0-or-later
#include <ondra/options.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void debugger(ondra::Machine& m) {
    std::cout << "Debugger: r, s [count], n (step over), c [frames], b addr, d addr [len],\n"
        "w addr bytes..., pc addr, in port, out port value, key row bit 0|1,\n"
        "load file.bin, save/loadsn file.osn, tape file, reset, q. Addresses use 0x.\n";
    std::string line;
    ondra::printRegisters(m);
    while (std::cout << "dbg> " && std::getline(std::cin, line)) {
        try {
            std::istringstream in(line); std::string cmd, a, b; in >> cmd;
            auto value = [&]() { std::string s; if (!(in >> s)) throw std::runtime_error("Missing argument"); return ondra::number(s); };
            if (cmd == "q" || cmd == "quit") break;
            if (cmd == "r") ondra::printRegisters(m);
            else if (cmd == "s") { unsigned n = (in >> a) ? ondra::number(a, 1000000) : 1; while (n--) m.step(); ondra::printRegisters(m); }
            else if (cmd == "n") {
                const unsigned pc = Z80_PC(m.cpu()), op = m.read(static_cast<std::uint16_t>(pc));
                const bool call = op == 0xcd || (op & 0xc7) == 0xc4, rst = (op & 0xc7) == 0xc7;
                const unsigned target = (pc + (call ? 3 : 1)) & 65535;
                m.step();
                if (call || rst) {
                    unsigned n = 1000000;
                    while (Z80_PC(m.cpu()) != target && n) { m.step(); --n; }
                    if (!n) std::cout << "Step-over limit reached.\n";
                }
                ondra::printRegisters(m);
            } else if (cmd == "c") {
                unsigned n = (in >> a) ? ondra::number(a, 1000000) : 50;
                // Resume past the current breakpoint without deleting it.
                if (m.breakpoints().count(Z80_PC(m.cpu()))) m.step();
                while (n-- && m.runFrame()) {}
                ondra::printRegisters(m);
            } else if (cmd == "b") {
                const auto address = static_cast<std::uint16_t>(value());
                if (m.breakpoints().count(address)) m.breakpoints().erase(address); else m.breakpoints().insert(address);
            } else if (cmd == "pc") m.jump(static_cast<std::uint16_t>(value()));
            else if (cmd == "d") {
                unsigned address = value(), n = (in >> b) ? ondra::number(b, 65536) : 128;
                for (unsigned i = 0; i < n; ++i) {
                    if (!(i % 16)) std::cout << '\n' << std::hex << std::setw(4) << std::setfill('0') << ((address + i) & 65535) << ": ";
                    std::cout << std::hex << std::setw(2) << unsigned(m.read(static_cast<std::uint16_t>(address + i))) << ' ';
                }
                std::cout << std::dec << '\n';
            } else if (cmd == "w") { unsigned address = value(); while (in >> b) m.ram()[address++ & 65535] = static_cast<std::uint8_t>(ondra::number(b, 255)); }
            else if (cmd == "in") std::cout << unsigned(m.input(static_cast<std::uint16_t>(value()))) << '\n';
            else if (cmd == "out") { const unsigned port = value(), data = value(); if (data > 255) throw std::runtime_error("Byte out of range"); m.output(static_cast<std::uint16_t>(port), static_cast<std::uint8_t>(data)); }
            else if (cmd == "key") { const unsigned row = value(), bit = value(), down = value(); m.key(row, bit, down != 0); }
            else if (cmd == "reset") m.reset();
            else if (cmd == "load" || cmd == "save" || cmd == "loadsn" || cmd == "tape") {
                if (!(in >> a)) throw std::runtime_error("Missing file name");
                if (cmd == "load") m.loadBinary(a);
                else if (cmd == "save") m.saveSnapshot(a);
                else if (cmd == "loadsn") m.loadSnapshot(a);
                else m.tape().load(a);
            } else if (!cmd.empty()) std::cout << "Unknown command.\n";
        } catch (const std::exception& e) { std::cout << e.what() << '\n'; }
    }
}
}
int main(int argc, char** argv) {
    try {
        const ondra::Options o = ondra::parseOptions(argc, argv);
        if (o.help) { ondra::printHelp(); return 0; }
        ondra::Machine m; ondra::configure(m, o);
        const unsigned frames = o.frames ? o.frames : 150;
        for (unsigned n = 0; n < frames; ++n) {
            if (n == o.loadAfter) ondra::loadProgram(m, o);
            m.runFrame();
        }
        if (o.debug) debugger(m);
        ondra::finish(m, o);
        ondra::printRegisters(m);
        const auto& pixels = m.pixels();
        const auto lit = std::count(pixels.begin(), pixels.end(), 0xffffffffu);
        std::cout << "frames=" << frames << " video=" << m.videoEnabled() << " lines=" << m.resolution()
            << " lit=" << lit << " screen-fnv1a=" << std::hex << m.screenHash() << std::dec << '\n';
        if (o.checkBoot && (!m.videoEnabled() || lit == 0 || lit == 320 * 256)) {
            std::cerr << "ROM boot check failed: no visible screen\n"; return 2;
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << "ondra-headless: " << e.what() << '\n'; return 1; }
}
