#pragma once

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include "droid_motion_program.h"

class DroidRobotController {
public:
    enum class Joint : uint8_t { Head = 0, LeftArm = 1, RightArm = 2 };
    enum class Source : uint8_t { None, Web, Voice, Touch };

    DroidRobotController();
    ~DroidRobotController();

    bool Initialize();
    void RegisterMcpTools();

    bool Drive(const std::string& direction, int speed, uint32_t duration_ms,
               Source source = Source::Web);
    bool SetTracks(int left_percent, int right_percent, uint32_t duration_ms,
                   Source source = Source::Web);
    bool SetJoint(Joint joint, int angle_degrees, int speed_degrees_per_second = 100,
                  Source source = Source::Web);
    bool StartGesture(const std::string& name, Source source = Source::Web);
    bool RunSequence(const std::string& json, std::string& error, Source source = Source::Voice,
                     bool dry_run = false);
    bool WaveArms(const std::string& arms, int cycles, int amplitude, const std::string& pattern,
                  std::string& error, Source source = Source::Voice);
    bool SpinTurns(const std::string& direction, int turns, std::string& error,
                   Source source = Source::Voice);
    bool SaveSpinCalibration(int left_ms, int right_ms);
    void Home(Source source = Source::Touch);
    void Stop(Source source = Source::Touch);
    void StopTracks(Source source = Source::Web);
    void HeartbeatWebControl();

    bool SaveHome(Joint joint, int angle_degrees);
    bool SaveHeadSpanUs(int span_us);
    bool SetTrackNeutral(bool left, int pulse_us);
    bool FlipTrack(bool left);
    void ResetCalibration();

    std::string GetStateJson() const;
    bool IsReady() const;
    bool IsMoving() const;

    static bool ParseJoint(const std::string& name, Joint& joint);

private:
    static void TaskEntry(void* arg);
    void Run();

    bool InitializeOutputs();
    void LoadCalibration();
    void SaveCalibrationLocked();
    void InvalidateSpinCalibrationLocked();

    void WritePulseLocked(size_t output, int pulse_us);
    void ApplyTrackLocked(size_t side);
    void SetTracksLocked(int left_percent, int right_percent, uint32_t duration_ms);
    void StopTracksLocked();
    void CancelGestureLocked(bool freeze_joints);
    void ApplyGestureFrameLocked(uint32_t now);
    void UpdateJointsLocked(uint32_t now);
    void UpdateGestureLocked(uint32_t now);
    bool StartProgram(const DroidMotionProgram& program, std::string& error, Source source);

    static uint32_t NowMs();
    static bool DeadlineReached(uint32_t now, uint32_t deadline);
    static const DroidGestureDefinition* FindGesture(const std::string& name);
    static const char* SourceName(Source source);

    mutable SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    bool pwm_ready_ = false;

    std::array<int, 5> output_pulse_us_{};
    std::array<int, 2> track_speed_{};
    std::array<int, 2> track_stop_us_{{1505, 1500}};
    std::array<int, 2> track_forward_sign_{{1, 1}};

    std::array<int, 3> joint_home_{{90, 90, 90}};
    std::array<int, 3> joint_angle_{{90, 90, 90}};
    std::array<int, 3> joint_target_{{90, 90, 90}};
    std::array<int, 3> joint_speed_{{100, 100, 100}};
    int head_span_us_ = 500;

    const DroidGestureDefinition* active_gesture_ = nullptr;
    size_t gesture_frame_ = 0;
    bool frame_reached_ = false;
    bool frame_tracks_running_ = false;
    bool head_waiting_for_tracks_ = false;
    uint32_t last_tracks_stopped_at_ = 0;
    uint32_t frame_started_at_ = 0;
    uint32_t frame_reached_at_ = 0;
    uint32_t track_deadline_ = 0;
    uint32_t last_joint_step_at_ = 0;
    uint32_t last_web_heartbeat_ = 0;
    Source source_ = Source::None;
    DroidMotionProgram custom_program_;
    DroidGestureDefinition custom_gesture_{"custom", custom_program_.frames.data(), 0};
    int program_cycles_done_ = 0;
    int program_repeat_ = 1;
    uint32_t program_started_at_ = 0;
    const char* program_status_ = "idle";
    std::array<int, 2> spin_turn_ms_{{0, 0}};
};
