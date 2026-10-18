// SPDX-License-Identifier: GPL-3.0-or-later
#include <ondra/tape.hpp>
#include <ondra/files.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ondra {
namespace {
bool signature(const std::vector<std::uint8_t>& d, std::size_t p, const char* text, std::size_t n) {
    return p <= d.size() && n <= d.size() - p && std::memcmp(d.data() + p, text, n) == 0;
}
}
Tape::Tape() : position_(0), rate_(22050), remaining_(0), initial_(true), level_(true),
    motor_(false), recording_(false), output_(false), recordInitial_(false), recordLast_(false),
    recordFraction_(0), recordLength_(0) {}
void Tape::load(const std::string& path) {
    const auto data = readFile(path);
    Tape next;
    if (signature(data, 0, "Compressed Square Wave", 22)) next.loadCsw(data);
    else if (signature(data, 0, "RIFF", 4)) next.loadWav(data);
    else next.loadTap(data);
    if (next.pulses_.empty()) throw std::runtime_error("Tape contains no samples");
    next.motor_ = motor_; next.rewind(); *this = next;
}
void Tape::rewind() {
    position_ = 0; level_ = initial_;
    remaining_ = pulses_.empty() ? 0 : pulses_[0];
}
void Tape::loadCsw(const std::vector<std::uint8_t>& d) {
    if (d.size() < 32 || d[22] != 0x1a) throw std::runtime_error("Invalid CSW header");
    std::size_t p;
    unsigned compression, flags;
    if (d[23] == 1) {
        rate_ = word(d, 25); compression = d[27]; flags = d[28]; p = 32;
    } else if (d[23] == 2 && d.size() >= 52) {
        rate_ = dword(d, 25); compression = d[33]; flags = d[34]; p = 52 + d[35];
    } else throw std::runtime_error("Unsupported CSW version");
    if (!rate_ || rate_ > 10000000 || p > d.size()) throw std::runtime_error("Invalid CSW sample rate/header");
    if (compression != 1) throw std::runtime_error("CSW supports RLE compression (type 1) only; decompress Z-RLE first");
    initial_ = (flags & 1) != 0;
    while (p < d.size()) {
        std::uint32_t run = d[p++];
        if (!run) { run = dword(d, p); p += 4; }
        if (!run) throw std::runtime_error("Zero-length CSW pulse");
        pulses_.push_back(run);
    }
}
void Tape::loadWav(const std::vector<std::uint8_t>& d) {
    if (!signature(d, 8, "WAVE", 4)) throw std::runtime_error("Invalid WAV header");
    unsigned bits = 0, align = 0;
    std::size_t start = 0, length = 0;
    for (std::size_t p = 12; p <= d.size() && d.size() - p >= 8;) {
        const std::uint32_t size = dword(d, p + 4);
        if (size > d.size() - p - 8) throw std::runtime_error("Truncated WAV chunk");
        if (signature(d, p, "fmt ", 4)) {
            if (size < 16 || word(d, p + 8) != 1 || word(d, p + 10) != 1)
                throw std::runtime_error("WAV must be mono PCM");
            rate_ = dword(d, p + 12); align = word(d, p + 20); bits = word(d, p + 22);
        } else if (signature(d, p, "data", 4)) { start = p + 8; length = size; }
        p += 8 + static_cast<std::size_t>(size) + (size & 1);
    }
    if (!start || !rate_ || rate_ > 10000000 || (bits != 8 && bits != 16) || align != bits / 8 || length % align)
        throw std::runtime_error("WAV must have 8/16-bit mono PCM samples and a valid sample rate");
    std::array<int, 256> history;
    history.fill(0); unsigned head = 0;
    bool previous = false; std::uint32_t count = 0;
    for (std::size_t p = start; p < start + length; p += align) {
        int sample = d[p] - 128;
        if (bits == 16) { unsigned v = word(d, p); sample = v < 32768 ? static_cast<int>(v) : static_cast<int>(v) - 65536; }
        history[head] = sample; head = (head + 1) & 255;
        const auto minmax = std::minmax_element(history.begin(), history.end());
        const bool level = sample > (*minmax.first + *minmax.second) / 2;
        if (!count) { previous = initial_ = level; count = 1; }
        else if (previous == level) ++count;
        else { pulses_.push_back(count); count = 1; previous = level; }
    }
    if (count) pulses_.push_back(count);
}
void Tape::loadTap(const std::vector<std::uint8_t>& d) {
    // Generate a 2500 bit/s biphase signal accepted by the monitor. The Java source's
    // 70 samples at a nominal 500 kHz depended on its timer dropping M-cycle
    // overshoot; using that nominal rate with a precise clock is too fast for
    // the ROM reader. One source sample here is one 1/5000-second half-bit.
    rate_ = 5000;
    std::vector<bool> bits;
    auto byte = [&bits](std::uint8_t v) {
        bits.push_back(!(v & 1));
        for (unsigned b = 0; b < 8; ++b) bits.push_back((v & (1u << b)) != 0);
    };
    auto pilot = [&bits]() { bits.insert(bits.end(), 1600, true); bits.insert(bits.end(), 7, true); bits.push_back(false); };
    auto alignBits = [&bits]() { while (bits.size() % 8) bits.push_back(false); bits.insert(bits.end(), 16, false); };
    std::size_t p = 0;
    while (p < d.size()) {
        if (d.size() - p < 25 || d[p] != 'H' || d[p + 16] != 'D') throw std::runtime_error("Invalid Ondra TAP header");
        const unsigned size = word(d, p + 21);
        bits.insert(bits.end(), 800, false); pilot();
        for (unsigned n = 0; n < 25; ++n) byte(d[p + n]);
        p += 25; byte(0); alignBits();
        if (p + 1 < d.size() && d[p] == 10 && d[p + 1] == 13) p += 2;
        if (p >= d.size() || d[p++] != 'D' || d.size() - p < size + 1) throw std::runtime_error("Truncated TAP data");
        pilot(); byte('D'); unsigned checksum = 0;
        for (unsigned n = 0; n < size; ++n) { checksum += d[p]; byte(d[p++]); }
        if ((checksum & 255) != d[p]) throw std::runtime_error("TAP data checksum mismatch");
        byte(d[p++]); byte(0); alignBits();
        if (p + 1 < d.size() && d[p] == 10 && d[p + 1] == 13) p += 2;
    }
    bool previous = false; std::uint32_t run = 0;
    for (bool b : bits) {
        for (unsigned half = 0; half < 2; ++half) {
            const bool level = half ? !b : b;
            if (!run) { previous = initial_ = level; run = 1; }
            else if (previous == level) ++run;
            else { pulses_.push_back(run); run = 1; previous = level; }
        }
    }
    if (run) pulses_.push_back(run);
}
void Tape::advance(std::uint64_t cycles, unsigned multiplier) {
    if (!motor_) return;
    if (recording_) {
        recordFraction_ += static_cast<double>(cycles) * 22050 / (2000000 * multiplier);
        const std::uint64_t n = static_cast<std::uint64_t>(recordFraction_);
        recordFraction_ -= n;
        if (n) {
            if (output_ != recordLast_) {
                if (recordLength_) { recorded_.push_back(static_cast<std::uint32_t>(recordLength_)); recordLength_ = 0; }
                if (recorded_.empty()) recordInitial_ = output_;
                recordLast_ = output_;
            }
            recordLength_ += n;
        }
        if (recordLength_ > std::numeric_limits<std::uint32_t>::max() || recorded_.size() > 16000000)
            throw std::runtime_error("Tape recording size limit exceeded");
        return;
    }
    if (finished()) return;
    remaining_ -= static_cast<double>(cycles) * rate_ / (2000000 * multiplier);
    while (remaining_ <= 0 && !finished()) {
        ++position_;
        if (!finished()) { level_ = !level_; remaining_ += pulses_[position_]; }
    }
}
void Tape::startRecording() {
    recorded_.clear(); recording_ = true; recordInitial_ = recordLast_ = output_;
    recordFraction_ = 0; recordLength_ = 0;
}
void Tape::recordLevel(bool level) {
    output_ = level;
}
void Tape::saveRecording(const std::string& path) const {
    std::vector<std::uint8_t> data;
    const char sig[] = "Compressed Square Wave";
    data.insert(data.end(), sig, sig + 22); data.push_back(0x1a); data.push_back(2); data.push_back(0);
    putDword(data, 22050);
    putDword(data, static_cast<std::uint32_t>(recorded_.size() + (recordLength_ ? 1 : 0)));
    data.push_back(1); data.push_back(recordInitial_ ? 1 : 0); data.push_back(0);
    const char app[16] = "Ondra C++";
    data.insert(data.end(), app, app + 16);
    auto put = [&data](std::uint32_t v) { if (v <= 255) data.push_back(static_cast<std::uint8_t>(v)); else { data.push_back(0); putDword(data, v); } };
    for (auto pulse : recorded_) put(pulse);
    if (recordLength_) put(static_cast<std::uint32_t>(recordLength_));
    writeFile(path, data);
}
}
