// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ondra {
// Pulse durations are measured in source samples, not host wall clock time.
class Tape {
public:
    Tape();
    void load(const std::string& path);
    void rewind();
    void advance(std::uint64_t cpuCycles, unsigned cpuMultiplier);
    void setMotor(bool on) { motor_ = on; }
    bool motor() const { return motor_; }
    bool level() const { return level_; }
    bool finished() const { return position_ >= pulses_.size(); }
    bool loaded() const { return !pulses_.empty(); }
    void startRecording();
    void recordLevel(bool level);
    void saveRecording(const std::string& path) const;
    bool recording() const { return recording_; }
    void stopRecording() { recording_ = false; }
    double progress() const { return pulses_.empty() ? 0.0 : static_cast<double>(position_) / pulses_.size(); }
    std::size_t pulseCount() const { return pulses_.size(); }
private:
    void loadCsw(const std::vector<std::uint8_t>& data);
    void loadWav(const std::vector<std::uint8_t>& data);
    void loadTap(const std::vector<std::uint8_t>& data);
    std::vector<std::uint32_t> pulses_;
    std::size_t position_;
    std::uint32_t rate_;
    double remaining_;
    bool initial_, level_, motor_, recording_, output_, recordInitial_, recordLast_;
    double recordFraction_;
    std::uint64_t recordLength_;
    std::vector<std::uint32_t> recorded_;
};
}
