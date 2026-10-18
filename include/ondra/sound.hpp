// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace ondra {
// Independent implementation of the SN76489 register protocol and divider/LFSR.
class Psg {
public:
    Psg();
    void write(std::uint8_t value);
    std::int16_t sample();
private:
    std::array<unsigned, 8> registers_;
    std::array<unsigned, 4> counters_;
    std::array<bool, 4> polarity_;
    unsigned latch_, lfsr_;
    double fraction_;
};
class Sound {
public:
    Sound();
    void reset();
    void beginFrame();
    void advance(std::uint64_t frameCycles, unsigned multiplier);
    void speaker(unsigned tone);
    void psgWrite(std::uint8_t value) { psg_.write(value); }
    void setMelodik(bool enabled) { melodik_ = enabled; }
    bool melodik() const { return melodik_; }
    const std::vector<std::int16_t>& finishFrame();
private:
    void generate(std::size_t count);
    std::array<std::vector<std::uint8_t>, 8> samples_;
    Psg psg_;
    unsigned tone_;
    std::size_t samplePosition_;
    bool melodik_;
    std::vector<std::int16_t> frame_;
};
}
