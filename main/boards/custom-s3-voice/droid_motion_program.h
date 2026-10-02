#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

struct DroidMotionFrame {
    uint8_t joint_mask;
    std::array<int16_t, 3> joint_offset;
    uint16_t hold_ms;
    int8_t left_track;
    int8_t right_track;
    uint16_t track_ms;
    uint8_t joint_step_degrees;
    uint16_t joint_step_ms;
    bool absolute = false;
};

struct DroidGestureDefinition {
    const char* name;
    const DroidMotionFrame* frames;
    size_t frame_count;
};

struct DroidMotionProgram {
    static constexpr size_t kMaxFrames = 64;
    static constexpr size_t kMaxJsonBytes = 4096;
    static constexpr uint32_t kMaxRunMs = 120000;
    std::array<DroidMotionFrame, kMaxFrames> frames{};
    size_t count = 0;
    int repeat = 1;
};

using DroidGestureLookup = const DroidGestureDefinition* (*)(const std::string&);

// Pure compilation and validation: never changes hardware or saved calibration.
bool ParseDroidProgram(const std::string& json, DroidGestureLookup lookup,
                       DroidMotionProgram& result, std::string& error);
bool ValidateDroidProgram(const DroidMotionProgram& program, std::string& error);

// One control tick. Returns the joint bit mask whose PWM targets changed.
uint8_t AdvanceDroidJoints(std::array<int, 3>& angles, const std::array<int, 3>& targets,
                           const std::array<int, 3>& max_steps, size_t moving_tracks,
                           bool defer_head);
bool AdvanceDroidFrame(size_t& frame, int& cycles_done, size_t frame_count, int repeat);

bool BuildDroidWaveProgram(const std::string& arms, int cycles, int amplitude,
                           const std::string& pattern, DroidMotionProgram& program,
                           std::string& error);
