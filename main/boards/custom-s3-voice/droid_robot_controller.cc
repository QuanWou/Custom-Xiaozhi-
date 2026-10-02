#include "droid_robot_controller.h"

#include "config.h"
#include "mcp_server.h"
#include "settings.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#define TAG "DroidRobot"

namespace {

constexpr uint32_t kPwmFrequencyHz = 50;
constexpr ledc_timer_bit_t kPwmResolution = LEDC_TIMER_14_BIT;
constexpr uint32_t kPwmPeriodUs = 1000000UL / kPwmFrequencyHz;
constexpr uint32_t kPwmCounts = 1UL << 14;
constexpr int kTrackRangeUs = 420;
constexpr uint32_t kDefaultTrackTimeoutMs = 700;
constexpr uint32_t kWebGestureTimeoutMs = 1200;
constexpr uint32_t kControlPeriodMs = 20;
constexpr size_t kMaxMovingOutputs = 4;

constexpr std::array<gpio_num_t, 5> kOutputPins = {
    LEFT_TRACK_GPIO, RIGHT_TRACK_GPIO, HEAD_SERVO_GPIO, LEFT_ARM_SERVO_GPIO, RIGHT_ARM_SERVO_GPIO};
constexpr std::array<ledc_channel_t, 5> kOutputChannels = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4};
constexpr std::array<int, 3> kJointMinPulseUs = {1000, 544, 544};
constexpr std::array<int, 3> kJointMaxPulseUs = {2000, 2400, 2400};
constexpr std::array<int, 2> kDefaultTrackStopUs = {1505, 1500};
// The assembled robot's observed F->right, B->left, L->back, R->forward mapping
// at -1/-1 showed that only the right track needed reversing. Its physical
// forward polarity is opposite the left track's.
constexpr std::array<int, 2> kDefaultForwardSign = {-1, 1};
constexpr std::array<int, 2> kLegacyForwardSign = {1, 1};
constexpr std::array<int, 3> kDefaultHome = {90, 90, 90};
constexpr std::array<const char*, 3> kHomeKeys = {"headHome", "leftHome", "rightHome"};

constexpr DroidMotionFrame kLookLeft[] = {
    {1, {-30, 0, 0}, 180, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kLookRight[] = {
    {1, {30, 0, 0}, 180, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kLookAround[] = {
    {1, {-30, 0, 0}, 400, 0, 0, 0, 2, 20},
    {1, {30, 0, 0}, 400, 0, 0, 0, 2, 20},
    {1, {0, 0, 0}, 200, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kShakeHead[] = {
    {1, {-25, 0, 0}, 120, 0, 0, 0, 2, 20}, {1, {25, 0, 0}, 120, 0, 0, 0, 2, 20},
    {1, {-25, 0, 0}, 120, 0, 0, 0, 2, 20}, {1, {25, 0, 0}, 120, 0, 0, 0, 2, 20},
    {1, {0, 0, 0}, 120, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kWaveLeft[] = {
    {2, {0, -40, 0}, 140, 0, 0, 0, 2, 20}, {2, {0, -15, 0}, 140, 0, 0, 0, 2, 20},
    {2, {0, -40, 0}, 140, 0, 0, 0, 2, 20}, {2, {0, -15, 0}, 140, 0, 0, 0, 2, 20},
    {2, {0, -40, 0}, 140, 0, 0, 0, 2, 20}, {2, {0, 0, 0}, 140, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kWaveRight[] = {
    {4, {0, 0, 40}, 140, 0, 0, 0, 2, 20}, {4, {0, 0, 15}, 140, 0, 0, 0, 2, 20},
    {4, {0, 0, 40}, 140, 0, 0, 0, 2, 20}, {4, {0, 0, 15}, 140, 0, 0, 0, 2, 20},
    {4, {0, 0, 40}, 140, 0, 0, 0, 2, 20}, {4, {0, 0, 0}, 140, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kCheer[] = {
    {7, {0, -40, 40}, 160, 30, 30, 240, 2, 20},   {7, {-20, -15, 15}, 140, -28, 28, 200, 2, 20},
    {7, {20, -40, 40}, 140, 28, -28, 200, 2, 20}, {7, {-20, -15, 15}, 140, -28, 28, 200, 2, 20},
    {7, {20, -40, 40}, 160, 28, -28, 200, 2, 20}, {7, {0, 0, 0}, 200, -30, -30, 240, 2, 20},
};
constexpr DroidMotionFrame kSad[] = {
    {7, {-15, 25, -25}, 650, -22, -22, 280, 1, 40},
    {7, {15, 18, -18}, 500, 0, 0, 0, 1, 40},
    {7, {-10, 25, -25}, 650, 0, 0, 0, 1, 40},
    {7, {0, 0, 0}, 250, 0, 0, 0, 1, 40},
};
constexpr DroidMotionFrame kCurious[] = {
    {7, {-25, -20, 10}, 450, -24, 24, 200, 1, 25},
    {7, {25, -10, 25}, 450, 24, -24, 200, 1, 25},
    {7, {0, -15, 15}, 300, 22, 22, 200, 1, 25},
    {7, {0, 0, 0}, 200, -22, -22, 200, 1, 25},
};
constexpr DroidMotionFrame kGreet[] = {
    {7, {0, -35, 35}, 200, 25, 25, 220, 2, 20}, {7, {-12, -10, 35}, 150, 0, 0, 0, 2, 20},
    {7, {12, -35, 10}, 150, 0, 0, 0, 2, 20},    {7, {-12, -10, 35}, 150, 0, 0, 0, 2, 20},
    {7, {12, -35, 10}, 150, 0, 0, 0, 2, 20},    {7, {0, 0, 0}, 200, -25, -25, 220, 2, 20},
};
constexpr DroidMotionFrame kShow[] = {
    {7, {-25, -35, 0}, 180, -32, 32, 260, 2, 20},  {7, {25, 0, 35}, 180, 32, -32, 260, 2, 20},
    {7, {-25, -35, 0}, 180, -32, 32, 260, 2, 20},  {7, {25, 0, 35}, 180, 32, -32, 260, 2, 20},
    {7, {0, -40, 40}, 250, 30, 30, 280, 2, 20},    {7, {0, -10, 10}, 180, -30, -30, 280, 2, 20},
    {7, {-20, -35, 15}, 150, -28, 28, 200, 2, 20}, {7, {20, -15, 35}, 150, 28, -28, 200, 2, 20},
    {7, {0, 0, 0}, 250, 0, 0, 0, 2, 20},
};
constexpr DroidMotionFrame kHome[] = {
    {7, {0, 0, 0}, 150, 0, 0, 0, 2, 20},
};

#define FRAME_COUNT(frames) (sizeof(frames) / sizeof((frames)[0]))
constexpr DroidGestureDefinition kGestures[] = {
    {"lookLeft", kLookLeft, FRAME_COUNT(kLookLeft)},
    {"lookRight", kLookRight, FRAME_COUNT(kLookRight)},
    {"lookAround", kLookAround, FRAME_COUNT(kLookAround)},
    {"shakeHead", kShakeHead, FRAME_COUNT(kShakeHead)},
    {"waveLeft", kWaveLeft, FRAME_COUNT(kWaveLeft)},
    {"waveRight", kWaveRight, FRAME_COUNT(kWaveRight)},
    {"cheer", kCheer, FRAME_COUNT(kCheer)},
    {"sad", kSad, FRAME_COUNT(kSad)},
    {"curious", kCurious, FRAME_COUNT(kCurious)},
    {"greet", kGreet, FRAME_COUNT(kGreet)},
    {"show", kShow, FRAME_COUNT(kShow)},
    {"home", kHome, FRAME_COUNT(kHome)},
};
#undef FRAME_COUNT

int AngleToPulse(size_t joint, int angle, int head_span_us) {
    if (joint == 0)
        return 1500 - head_span_us + (2 * head_span_us * angle) / 180;
    return kJointMinPulseUs[joint] +
           (kJointMaxPulseUs[joint] - kJointMinPulseUs[joint]) * angle / 180;
}

uint32_t PulseToDuty(int pulse_us) {
    return (static_cast<uint32_t>(pulse_us) * kPwmCounts + kPwmPeriodUs / 2) / kPwmPeriodUs;
}

}  // namespace

DroidRobotController::DroidRobotController() { mutex_ = xSemaphoreCreateMutex(); }

DroidRobotController::~DroidRobotController() {
    if (task_ != nullptr) {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool DroidRobotController::Initialize() {
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create motion mutex");
        return false;
    }

    LoadCalibration();
    if (!InitializeOutputs()) {
        return false;
    }

    if (xTaskCreate(TaskEntry, "droid_motion", 4096, this, 4, &task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motion task");
        pwm_ready_ = false;
        return false;
    }

    RegisterMcpTools();
    ESP_LOGI(TAG, "Robot controller ready");
    return true;
}

bool DroidRobotController::InitializeOutputs() {
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = kPwmResolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = kPwmFrequencyHz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&timer_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure servo timer");
        return false;
    }

    for (size_t output = 0; output < kOutputPins.size(); ++output) {
        const int initial_pulse =
            output < 2 ? track_stop_us_[output]
                       : AngleToPulse(output - 2, joint_angle_[output - 2], head_span_us_);
        ledc_channel_config_t channel_config = {
            .gpio_num = kOutputPins[output],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = kOutputChannels[output],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = PulseToDuty(initial_pulse),
            .hpoint = 0,
        };
        if (ledc_channel_config(&channel_config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure output %u on GPIO %d", static_cast<unsigned>(output),
                     kOutputPins[output]);
            return false;
        }
        output_pulse_us_[output] = initial_pulse;
    }
    pwm_ready_ = true;
    return true;
}

void DroidRobotController::LoadCalibration() {
    Settings motion_settings("droid-motion", false);
    spin_turn_ms_[0] = motion_settings.GetInt("spinLeftMs", 0);
    spin_turn_ms_[1] = motion_settings.GetInt("spinRightMs", 0);
    for (auto& value : spin_turn_ms_) {
        if (value < 1000 || value > 10000)
            value = 0;
    }
    Settings settings("droid-tracks", false);
    head_span_us_ = settings.GetInt("headSpanUs", 500);
    if (head_span_us_ < 400 || head_span_us_ > 700)
        head_span_us_ = 500;
    track_stop_us_[0] =
        std::clamp<int>(settings.GetInt("leftStop", kDefaultTrackStopUs[0]), 1400, 1600);
    track_stop_us_[1] =
        std::clamp<int>(settings.GetInt("rightStop", kDefaultTrackStopUs[1]), 1400, 1600);
    const bool left_flip = settings.GetBool("leftFlip", false);
    const bool right_flip = settings.GetBool("rightFlip", false);
    if (settings.GetInt("polarityV2", 0) == 1) {
        track_forward_sign_[0] = left_flip ? -kDefaultForwardSign[0] : kDefaultForwardSign[0];
        track_forward_sign_[1] = right_flip ? -kDefaultForwardSign[1] : kDefaultForwardSign[1];
    } else {
        // An empty NVS partition gets the new defaults. Existing r3 flip keys
        // were relative to +1/+1 and must retain their effective directions.
        nvs_handle_t probe = 0;
        bool has_legacy_flip = false;
        if (nvs_open("droid-tracks", NVS_READONLY, &probe) == ESP_OK) {
            uint8_t value;
            has_legacy_flip = nvs_get_u8(probe, "leftFlip", &value) == ESP_OK ||
                              nvs_get_u8(probe, "rightFlip", &value) == ESP_OK;
            nvs_close(probe);
        }
        const auto& prior_sign = has_legacy_flip ? kLegacyForwardSign : kDefaultForwardSign;
        track_forward_sign_[0] = left_flip ? -prior_sign[0] : prior_sign[0];
        track_forward_sign_[1] = right_flip ? -prior_sign[1] : prior_sign[1];
        Settings migrated("droid-tracks", true);
        migrated.SetBool("leftFlip", track_forward_sign_[0] != kDefaultForwardSign[0]);
        migrated.SetBool("rightFlip", track_forward_sign_[1] != kDefaultForwardSign[1]);
        migrated.SetInt("polarityV2", 1);
        ESP_LOGI(TAG, "Migrated track polarity; effective left=%d right=%d", track_forward_sign_[0],
                 track_forward_sign_[1]);
    }

    for (size_t joint = 0; joint < joint_home_.size(); ++joint) {
        joint_home_[joint] =
            std::clamp<int>(settings.GetInt(kHomeKeys[joint], kDefaultHome[joint]), 0, 180);
        joint_angle_[joint] = joint_home_[joint];
        joint_target_[joint] = joint_home_[joint];
    }
}

void DroidRobotController::SaveCalibrationLocked() {
    Settings settings("droid-tracks", true);
    settings.SetInt("headSpanUs", head_span_us_);
    settings.SetInt("leftStop", track_stop_us_[0]);
    settings.SetInt("rightStop", track_stop_us_[1]);
    settings.SetBool("leftFlip", track_forward_sign_[0] != kDefaultForwardSign[0]);
    settings.SetBool("rightFlip", track_forward_sign_[1] != kDefaultForwardSign[1]);
    settings.SetInt("polarityV2", 1);
    for (size_t joint = 0; joint < joint_home_.size(); ++joint) {
        settings.SetInt(kHomeKeys[joint], joint_home_[joint]);
    }
}

void DroidRobotController::TaskEntry(void* arg) { static_cast<DroidRobotController*>(arg)->Run(); }

void DroidRobotController::Run() {
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kControlPeriodMs));
        const uint32_t now = NowMs();

        xSemaphoreTake(mutex_, portMAX_DELAY);
        if (track_deadline_ != 0 && DeadlineReached(now, track_deadline_)) {
            frame_tracks_running_ = false;
            StopTracksLocked();
        }
        UpdateJointsLocked(now);
        UpdateGestureLocked(now);
        xSemaphoreGive(mutex_);
    }
}

void DroidRobotController::WritePulseLocked(size_t output, int pulse_us) {
    if (!pwm_ready_ || output >= kOutputChannels.size() || pulse_us < 500 || pulse_us > 2500 ||
        output_pulse_us_[output] == pulse_us) {
        return;
    }
    const uint32_t duty = PulseToDuty(pulse_us);
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, kOutputChannels[output], duty) == ESP_OK &&
        ledc_update_duty(LEDC_LOW_SPEED_MODE, kOutputChannels[output]) == ESP_OK) {
        output_pulse_us_[output] = pulse_us;
    } else {
        ESP_LOGE(TAG, "Failed to write output %u", static_cast<unsigned>(output));
        pwm_ready_ = false;
    }
}

void DroidRobotController::ApplyTrackLocked(size_t side) {
    const int pulse =
        track_stop_us_[side] + track_forward_sign_[side] * track_speed_[side] * kTrackRangeUs / 100;
    WritePulseLocked(side, std::clamp(pulse, 1000, 2000));
}

void DroidRobotController::SetTracksLocked(int left_percent, int right_percent,
                                           uint32_t duration_ms) {
    track_speed_[0] = std::clamp(left_percent, -100, 100);
    track_speed_[1] = std::clamp(right_percent, -100, 100);
    ApplyTrackLocked(0);
    ApplyTrackLocked(1);

    const uint32_t timeout = std::clamp<uint32_t>(duration_ms, 100, 5000);
    track_deadline_ = NowMs() + timeout;
}

void DroidRobotController::StopTracksLocked() {
    if (track_speed_[0] != 0 || track_speed_[1] != 0) {
        last_tracks_stopped_at_ = NowMs();
    }
    track_speed_[0] = 0;
    track_speed_[1] = 0;
    track_deadline_ = 0;
    ApplyTrackLocked(0);
    ApplyTrackLocked(1);
}

void DroidRobotController::CancelGestureLocked(bool freeze_joints) {
    if (active_gesture_ != nullptr)
        program_status_ = "stopped";
    active_gesture_ = nullptr;
    gesture_frame_ = 0;
    frame_reached_ = false;
    frame_tracks_running_ = false;
    head_waiting_for_tracks_ = false;
    StopTracksLocked();
    if (freeze_joints) {
        joint_target_ = joint_angle_;
    }
}

bool DroidRobotController::Drive(const std::string& direction, int speed, uint32_t duration_ms,
                                 Source source) {
    const int safe_speed = std::clamp(speed, 0, 100);
    if (direction == "forward") {
        return SetTracks(safe_speed, safe_speed, duration_ms, source);
    }
    if (direction == "backward") {
        return SetTracks(-safe_speed, -safe_speed, duration_ms, source);
    }
    if (direction == "left") {
        return SetTracks(-safe_speed, safe_speed, duration_ms, source);
    }
    if (direction == "right") {
        return SetTracks(safe_speed, -safe_speed, duration_ms, source);
    }
    return false;
}

bool DroidRobotController::SetTracks(int left_percent, int right_percent, uint32_t duration_ms,
                                     Source source) {
    if (left_percent < -100 || left_percent > 100 || right_percent < -100 || right_percent > 100 ||
        duration_ms < 100 || duration_ms > 5000) {
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    source_ = source;
    SetTracksLocked(left_percent, right_percent, duration_ms);
    if (source == Source::Web) {
        last_web_heartbeat_ = NowMs();
    }
    const bool ready = pwm_ready_;
    xSemaphoreGive(mutex_);
    return ready;
}

bool DroidRobotController::SetJoint(Joint joint, int angle_degrees, int speed_degrees_per_second,
                                    Source source) {
    const size_t index = static_cast<size_t>(joint);
    if (index >= joint_target_.size() || angle_degrees < 0 || angle_degrees > 180 ||
        speed_degrees_per_second < 10 || speed_degrees_per_second > 180) {
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    source_ = source;
    joint_target_[index] = angle_degrees;
    joint_speed_[index] = speed_degrees_per_second;
    const bool ready = pwm_ready_;
    xSemaphoreGive(mutex_);
    return ready;
}

bool DroidRobotController::StartGesture(const std::string& name, Source source) {
    const auto* gesture = FindGesture(name);
    if (gesture == nullptr) {
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    active_gesture_ = gesture;
    gesture_frame_ = 0;
    program_started_at_ = NowMs();
    program_status_ = "running";
    program_cycles_done_ = 0;
    program_repeat_ = 1;
    source_ = source;
    last_web_heartbeat_ = NowMs();
    ApplyGestureFrameLocked(NowMs());
    const bool ready = pwm_ready_;
    xSemaphoreGive(mutex_);
    return ready;
}

bool DroidRobotController::RunSequence(const std::string& json, std::string& error, Source source,
                                       bool dry_run) {
    auto candidate = std::make_unique<DroidMotionProgram>();
    if (!ParseDroidProgram(json, FindGesture, *candidate, error))
        return false;
    if (dry_run)
        return true;
    return StartProgram(*candidate, error, source);
}

bool DroidRobotController::StartProgram(const DroidMotionProgram& program, std::string& error,
                                        Source source) {
    if (!ValidateDroidProgram(program, error))
        return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (!pwm_ready_ || active_gesture_ != nullptr || track_speed_[0] != 0 || track_speed_[1] != 0 ||
        joint_target_ != joint_angle_) {
        xSemaphoreGive(mutex_);
        error = "Robot busy or PWM unavailable. Wait for completion, or explicitly stop first.";
        return false;
    }
    CancelGestureLocked(true);
    custom_program_ = program;
    custom_gesture_.frame_count = custom_program_.count;
    active_gesture_ = &custom_gesture_;
    program_repeat_ = program.repeat;
    program_cycles_done_ = 0;
    program_started_at_ = NowMs();
    program_status_ = "running";
    source_ = source;
    last_web_heartbeat_ = NowMs();
    ApplyGestureFrameLocked(NowMs());
    xSemaphoreGive(mutex_);
    return true;
}

bool DroidRobotController::WaveArms(const std::string& arms, int cycles, int amplitude,
                                    const std::string& pattern, std::string& error, Source source) {
    auto program = std::make_unique<DroidMotionProgram>();
    if (!BuildDroidWaveProgram(arms, cycles, amplitude, pattern, *program, error))
        return false;
    return StartProgram(*program, error, source);
}

bool DroidRobotController::SpinTurns(const std::string& direction, int turns, std::string& error,
                                     Source source) {
    if ((direction != "left" && direction != "right") || turns < 1 || turns > 10) {
        error = "direction=left/right, turns=1..10";
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const int turn_ms = spin_turn_ms_[direction == "left" ? 0 : 1];
    xSemaphoreGive(mutex_);
    if (turn_ms == 0) {
        error =
            "Spin is not calibrated. Ask user to measure one turn at speed 40% and save it "
            "in web settings. Do not invent a duration or claim measured revolutions.";
        return false;
    }
    auto program = std::make_unique<DroidMotionProgram>();
    program->repeat = turns;
    const int parts = (turn_ms + 4999) / 5000;
    const int sign = direction == "left" ? -1 : 1;
    for (int part = 0; part < parts; ++part) {
        const int duration = turn_ms / parts + (part < turn_ms % parts ? 1 : 0);
        program->frames[program->count++] = {0,
                                             {0, 0, 0},
                                             0,
                                             static_cast<int8_t>(sign * 40),
                                             static_cast<int8_t>(-sign * 40),
                                             static_cast<uint16_t>(duration),
                                             2,
                                             20};
    }
    program->frames[program->count++] = {0, {0, 0, 0}, 250, 0, 0, 0, 2, 20};
    return StartProgram(*program, error, source);
}

void DroidRobotController::InvalidateSpinCalibrationLocked() {
    spin_turn_ms_ = {0, 0};
    Settings settings("droid-motion", true);
    settings.SetInt("spinLeftMs", 0);
    settings.SetInt("spinRightMs", 0);
}

bool DroidRobotController::SaveSpinCalibration(int left_ms, int right_ms) {
    if (left_ms < 1000 || left_ms > 10000 || right_ms < 1000 || right_ms > 10000)
        return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    spin_turn_ms_ = {left_ms, right_ms};
    Settings settings("droid-motion", true);
    settings.SetInt("spinLeftMs", left_ms);
    settings.SetInt("spinRightMs", right_ms);
    xSemaphoreGive(mutex_);
    return true;
}

void DroidRobotController::Home(Source source) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    source_ = source;
    joint_target_ = joint_home_;
    joint_speed_ = {100, 100, 100};
    xSemaphoreGive(mutex_);
}

void DroidRobotController::Stop(Source source) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    source_ = source;
    xSemaphoreGive(mutex_);
}

void DroidRobotController::HeartbeatWebControl() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    last_web_heartbeat_ = NowMs();
    xSemaphoreGive(mutex_);
}

void DroidRobotController::StopTracks(Source source) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(false);
    source_ = source;
    xSemaphoreGive(mutex_);
}

bool DroidRobotController::SaveHome(Joint joint, int angle_degrees) {
    const size_t index = static_cast<size_t>(joint);
    if (index >= joint_home_.size() || angle_degrees < 0 || angle_degrees > 180) {
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (track_speed_[0] != 0 || track_speed_[1] != 0 || active_gesture_ != nullptr) {
        xSemaphoreGive(mutex_);
        return false;
    }
    joint_home_[index] = angle_degrees;
    Settings settings("droid-tracks", true);
    settings.SetInt(kHomeKeys[index], angle_degrees);
    xSemaphoreGive(mutex_);
    return true;
}

bool DroidRobotController::SaveHeadSpanUs(int span_us) {
    if (span_us < 400 || span_us > 700 || span_us % 25 != 0)
        return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    // Changing endpoints at the center leaves the current PWM pulse at 1500 us.
    const bool safe = pwm_ready_ && active_gesture_ == nullptr && track_speed_[0] == 0 &&
                      track_speed_[1] == 0 && joint_angle_[0] == 90 && joint_target_[0] == 90;
    if (safe) {
        head_span_us_ = span_us;
        Settings settings("droid-tracks", true);
        settings.SetInt("headSpanUs", span_us);
    }
    xSemaphoreGive(mutex_);
    return safe;
}

bool DroidRobotController::SetTrackNeutral(bool left, int pulse_us) {
    if (pulse_us < 1400 || pulse_us > 1600) {
        return false;
    }
    const size_t side = left ? 0 : 1;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    track_stop_us_[side] = pulse_us;
    InvalidateSpinCalibrationLocked();
    ApplyTrackLocked(side);
    Settings settings("droid-tracks", true);
    settings.SetInt(left ? "leftStop" : "rightStop", pulse_us);
    xSemaphoreGive(mutex_);
    return true;
}

bool DroidRobotController::FlipTrack(bool left) {
    const size_t side = left ? 0 : 1;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    track_forward_sign_[side] = -track_forward_sign_[side];
    InvalidateSpinCalibrationLocked();
    Settings settings("droid-tracks", true);
    settings.SetBool(left ? "leftFlip" : "rightFlip",
                     track_forward_sign_[side] != kDefaultForwardSign[side]);
    xSemaphoreGive(mutex_);
    return true;
}

void DroidRobotController::ResetCalibration() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    CancelGestureLocked(true);
    track_stop_us_ = kDefaultTrackStopUs;
    track_forward_sign_ = kDefaultForwardSign;
    head_span_us_ = 500;
    InvalidateSpinCalibrationLocked();
    joint_home_ = kDefaultHome;
    joint_target_ = joint_home_;
    joint_speed_ = {100, 100, 100};
    ApplyTrackLocked(0);
    ApplyTrackLocked(1);
    SaveCalibrationLocked();
    xSemaphoreGive(mutex_);
}

void DroidRobotController::ApplyGestureFrameLocked(uint32_t now) {
    if (active_gesture_ == nullptr || gesture_frame_ >= active_gesture_->frame_count) {
        return;
    }
    const auto& frame = active_gesture_->frames[gesture_frame_];
    for (size_t joint = 0; joint < joint_target_.size(); ++joint) {
        if ((frame.joint_mask & (1U << joint)) != 0) {
            joint_target_[joint] = std::clamp(
                (frame.absolute ? 0 : joint_home_[joint]) + frame.joint_offset[joint], 0, 180);
        }
    }
    frame_started_at_ = now;
    frame_reached_ = false;
    frame_tracks_running_ = frame.track_ms > 0 && (frame.left_track != 0 || frame.right_track != 0);
    head_waiting_for_tracks_ = frame_tracks_running_;
    if (frame_tracks_running_) {
        SetTracksLocked(frame.left_track, frame.right_track, frame.track_ms);
    } else {
        StopTracksLocked();
    }
}

void DroidRobotController::UpdateJointsLocked(uint32_t now) {
    if (head_waiting_for_tracks_ && track_speed_[0] == 0 && track_speed_[1] == 0 &&
        static_cast<uint32_t>(now - last_tracks_stopped_at_) >= 120) {
        head_waiting_for_tracks_ = false;
    }
    uint32_t step_ms = kControlPeriodMs;
    int step_degrees = 0;
    if (active_gesture_ != nullptr) {
        const auto& frame = active_gesture_->frames[gesture_frame_];
        step_ms = frame.joint_step_ms;
        step_degrees = frame.joint_step_degrees;
    }
    if (static_cast<uint32_t>(now - last_joint_step_at_) < step_ms) {
        return;
    }
    last_joint_step_at_ = now;

    size_t moving_tracks = 0;
    for (int speed : track_speed_) {
        moving_tracks += speed != 0 ? 1 : 0;
    }
    std::array<int, 3> max_steps;
    for (size_t joint = 0; joint < max_steps.size(); ++joint) {
        max_steps[joint] =
            active_gesture_ != nullptr
                ? step_degrees
                : std::max(1, (joint_speed_[joint] * static_cast<int>(step_ms) + 999) / 1000);
    }
    // TEST_5: head waits for both tracks and the 120 ms settling gap.
    const bool defer_head =
        active_gesture_ != nullptr && (head_waiting_for_tracks_ || moving_tracks != 0);
    const uint8_t changed =
        AdvanceDroidJoints(joint_angle_, joint_target_, max_steps, moving_tracks, defer_head);
    for (size_t joint = 0; joint < joint_angle_.size(); ++joint) {
        if (changed & (1U << joint))
            WritePulseLocked(joint + 2, AngleToPulse(joint, joint_angle_[joint], head_span_us_));
    }
}

void DroidRobotController::UpdateGestureLocked(uint32_t now) {
    if (active_gesture_ == nullptr) {
        if (track_speed_[0] == 0 && track_speed_[1] == 0 && joint_angle_ == joint_target_) {
            source_ = Source::None;
        }
        return;
    }

    if (static_cast<uint32_t>(now - program_started_at_) >= DroidMotionProgram::kMaxRunMs ||
        !pwm_ready_) {
        CancelGestureLocked(true);
        program_status_ = pwm_ready_ ? "timeout" : "pwm_error";
        source_ = Source::None;
        return;
    }

    if (source_ == Source::Web &&
        static_cast<uint32_t>(now - last_web_heartbeat_) >= kWebGestureTimeoutMs) {
        ESP_LOGW(TAG, "Stopping web gesture after heartbeat timeout");
        CancelGestureLocked(true);
        program_status_ = "heartbeat_timeout";
        source_ = Source::None;
        return;
    }

    const auto& frame = active_gesture_->frames[gesture_frame_];
    if (frame_tracks_running_) {
        return;
    }
    for (size_t joint = 0; joint < joint_angle_.size(); ++joint) {
        if ((frame.joint_mask & (1U << joint)) != 0 &&
            joint_angle_[joint] != joint_target_[joint]) {
            return;
        }
    }
    if (!frame_reached_) {
        frame_reached_ = true;
        frame_reached_at_ = now;
        return;
    }
    if (static_cast<uint32_t>(now - frame_reached_at_) < frame.hold_ms) {
        return;
    }

    if (AdvanceDroidFrame(gesture_frame_, program_cycles_done_, active_gesture_->frame_count,
                          program_repeat_)) {
        active_gesture_ = nullptr;
        StopTracksLocked();
        program_status_ = "completed";
        source_ = Source::None;
        return;
    }
    ApplyGestureFrameLocked(now);
}

std::string DroidRobotController::GetStateJson() const {
    std::array<int, 2> tracks;
    std::array<int, 2> stop_us;
    std::array<int, 2> signs;
    std::array<int, 3> angles;
    std::array<int, 3> targets;
    std::array<int, 3> homes;
    const char* gesture = "idle";
    Source source = Source::None;
    bool ready = false;
    std::array<int, 2> spin_ms;
    int cycles_done, repeat, head_span_us;
    size_t frame_index, frame_count;
    const char* program_status;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    tracks = track_speed_;
    stop_us = track_stop_us_;
    signs = track_forward_sign_;
    angles = joint_angle_;
    targets = joint_target_;
    homes = joint_home_;
    gesture = active_gesture_ != nullptr ? active_gesture_->name : "idle";
    source = source_;
    ready = pwm_ready_;
    spin_ms = spin_turn_ms_;
    head_span_us = head_span_us_;
    cycles_done = program_cycles_done_;
    repeat = program_repeat_;
    frame_index = gesture_frame_;
    frame_count = active_gesture_ ? active_gesture_->frame_count : 0;
    program_status = program_status_;
    xSemaphoreGive(mutex_);

    char json[1024];
    std::snprintf(json, sizeof(json),
                  "{\"pwmReady\":%s,\"source\":\"%s\",\"gesture\":\"%s\","
                  "\"maxMovingOutputs\":%u,"
                  "\"program\":{\"status\":\"%s\",\"cyclesDone\":%d,\"repeat\":%d,"
                  "\"frame\":%u,\"frameCount\":%u,\"limitMs\":120000},"
                  "\"spinCalibration\":{\"leftMs\":%d,\"rightMs\":%d,\"speed\":40,"
                  "\"measuredFeedback\":false},"
                  "\"tracks\":{\"left\":%d,\"right\":%d,\"leftStopUs\":%d,"
                  "\"rightStopUs\":%d,\"leftSign\":%d,\"rightSign\":%d},"
                  "\"joints\":{\"head\":{\"angle\":%d,\"target\":%d,\"home\":%d,\"spanUs\":%d},"
                  "\"leftArm\":{\"angle\":%d,\"target\":%d,\"home\":%d},"
                  "\"rightArm\":{\"angle\":%d,\"target\":%d,\"home\":%d}}}",
                  ready ? "true" : "false", SourceName(source), gesture,
                  static_cast<unsigned>(kMaxMovingOutputs), program_status, cycles_done, repeat,
                  static_cast<unsigned>(frame_index), static_cast<unsigned>(frame_count),
                  spin_ms[0], spin_ms[1], tracks[0], tracks[1], stop_us[0], stop_us[1], signs[0],
                  signs[1], angles[0], targets[0], homes[0], head_span_us, angles[1], targets[1],
                  homes[1], angles[2], targets[2], homes[2]);
    return json;
}

bool DroidRobotController::IsReady() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool ready = pwm_ready_;
    xSemaphoreGive(mutex_);
    return ready;
}

bool DroidRobotController::IsMoving() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool moving = active_gesture_ != nullptr || track_speed_[0] != 0 ||
                        track_speed_[1] != 0 || joint_angle_ != joint_target_;
    xSemaphoreGive(mutex_);
    return moving;
}

bool DroidRobotController::ParseJoint(const std::string& name, Joint& joint) {
    if (name == "head") {
        joint = Joint::Head;
        return true;
    }
    if (name == "leftArm") {
        joint = Joint::LeftArm;
        return true;
    }
    if (name == "rightArm") {
        joint = Joint::RightArm;
        return true;
    }
    return false;
}

uint32_t DroidRobotController::NowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

bool DroidRobotController::DeadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

const DroidGestureDefinition* DroidRobotController::FindGesture(const std::string& name) {
    for (const auto& gesture : kGestures) {
        if (name == gesture.name) {
            return &gesture;
        }
    }
    return nullptr;
}

const char* DroidRobotController::SourceName(Source source) {
    switch (source) {
        case Source::Web:
            return "web";
        case Source::Voice:
            return "voice";
        case Source::Touch:
            return "touch";
        default:
            return "idle";
    }
}

void DroidRobotController::RegisterMcpTools() {
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool(
        "self.robot.run_sequence",
        "Execute a custom ordered motion program, not arbitrary code. Use ONE call for compound "
        "requests so later commands do not cancel earlier ones. program is a JSON string: "
        "{\"repeat\":1,\"steps\":[{\"drive\":\"forward\",\"speed\":35,\"duration_ms\":500},"
        "{\"pose\":{\"leftArm\":50,\"rightArm\":130},\"hold_ms\":200},"
        "{\"gesture\":\"home\"}]}. Each step: either gesture name, OR pose/drive/hold_ms. "
        "pose has optional head/leftArm/rightArm degrees 0-180 (omitted joints stay still); "
        "relative:true uses offsets -60..60 from saved home instead. joint_speed=10..100 deg/s. "
        "drive=forward/backward/left/right with speed=10..60%, OR "
        "tracks:{left:-60..60,right:-60..60} for independent physical signed track speeds; "
        "both need duration_ms=100..5000; never combine drive and tracks in one step. "
        "hold_ms=0..5000 AFTER movement. repeat=1..20, max32 steps/64 expanded frames,120s total. "
        "Up to four commanded moving outputs; head waits for tracks. Use wave_arms for both arms "
        "and spin_turns for calibrated turn counts. Returns acceptance, NOT completion; query "
        "self.robot.state for progress. No distance/angle feedback. Never fabricate measured "
        "centimeters or turns. Busy programs are rejected; stop only when user wants replacement.",
        PropertyList({Property("program", kPropertyTypeString).SetMaxLength(4096),
                      Property("dry_run", kPropertyTypeBoolean, false)}),
        [this](const PropertyList& p) -> ToolResult {
            std::string error;
            const bool dry_run = p["dry_run"].value<bool>();
            if (!RunSequence(p["program"].value<std::string>(), error, Source::Voice, dry_run))
                return std::unexpected(error);
            if (dry_run)
                return std::string("Program valid; no movement was started.");
            return std::string("Program accepted, not yet completed. ") + GetStateJson();
        });

    mcp.AddTool(
        "self.robot.wave_arms",
        "Wave left arm, right arm, or both arms independently. For both on this robot: "
        "pattern=mirror moves arms in opposite PHYSICAL directions (same servo signs); "
        "parallel moves arms in the same PHYSICAL direction (opposite servo signs); "
        "alternate waves left then right. cycles=1..20; amplitude_deg=10..45 relative to "
        "saved home. No track movement. "
        "Returns started, not finished. Read state for completion; stop interrupts immediately.",
        PropertyList(
            {Property("arms", kPropertyTypeString, std::string("both")).SetMaxLength(5),
             Property("cycles", kPropertyTypeInteger, 3, 1, 20),
             Property("amplitude_deg", kPropertyTypeInteger, 35, 10, 45),
             Property("pattern", kPropertyTypeString, std::string("mirror")).SetMaxLength(9)}),
        [this](const PropertyList& p) -> ToolResult {
            std::string error;
            if (!WaveArms(p["arms"].value<std::string>(), p["cycles"].value<int>(),
                          p["amplitude_deg"].value<int>(), p["pattern"].value<std::string>(),
                          error))
                return std::unexpected(error);
            return std::string("Arm wave started. ") + GetStateJson();
        });

    mcp.AddTool(
        "self.robot.spin_turns",
        "Spin the robot left or right for an ESTIMATED 1..10 full turns (xoay 10 vong). "
        "Uses user-measured per-turn duration saved in web settings at fixed40% speed. "
        "Rejects if uncalibrated; do not substitute invented timed drive commands. "
        "Explain that there is no encoder/IMU feedback, so count is approximate and depends on "
        "floor, battery and load. Ask for clear space; no obstacle sensor. Stops at bounded "
        "deadline. "
        "Return means started, NOT measured completion.",
        PropertyList({Property("direction", kPropertyTypeString).SetMaxLength(5),
                      Property("turns", kPropertyTypeInteger, 1, 10)}),
        [this](const PropertyList& p) -> ToolResult {
            std::string error;
            if (!SpinTurns(p["direction"].value<std::string>(), p["turns"].value<int>(), error))
                return std::unexpected(error);
            return std::string("Timed spin estimate started; revolutions are NOT measured. ") +
                   GetStateJson();
        });

    mcp.AddTool(
        "self.robot.set_joint",
        "Move one Droid E3D joint. joint must be head, leftArm, or rightArm. The returned angle is "
        "commanded; the robot has no position feedback.",
        PropertyList({Property("joint", kPropertyTypeString).SetMaxLength(16),
                      Property("angle_deg", kPropertyTypeInteger, 0, 180),
                      Property("speed_deg_s", kPropertyTypeInteger, 100, 10, 180)}),
        [this](const PropertyList& properties) -> ToolResult {
            Joint joint;
            const auto& name = properties["joint"].value<std::string>();
            if (!ParseJoint(name, joint)) {
                return std::unexpected("joint must be head, leftArm, or rightArm");
            }
            if (!SetJoint(joint, properties["angle_deg"].value<int>(),
                          properties["speed_deg_s"].value<int>(), Source::Voice)) {
                return std::unexpected("joint command rejected");
            }
            return std::string("joint command accepted");
        });

    mcp.AddTool(
        "self.robot.drive_tracks",
        "Independently command the left and right continuous-rotation TRACK servos (not joint "
        "angles). left_pct and right_pct are physical signed directions: positive=forward, "
        "negative=backward, zero=stop, each -60..60. For left-only set right_pct=0; for "
        "right-only set left_pct=0. The command stops automatically after 100..5000 ms. "
        "Never claim measured distance or angle; check open space before moving.",
        PropertyList({Property("left_pct", kPropertyTypeInteger, 0, -60, 60),
                      Property("right_pct", kPropertyTypeInteger, 0, -60, 60),
                      Property("duration_ms", kPropertyTypeInteger, 500, 100, 5000)}),
        [this](const PropertyList& p) -> ToolResult {
            const int left = p["left_pct"].value<int>();
            const int right = p["right_pct"].value<int>();
            if (!SetTracks(left, right, p["duration_ms"].value<int>(), Source::Voice))
                return std::unexpected("track command rejected");
            return std::string("Bounded independent track command accepted. ") + GetStateJson();
        });

    mcp.AddTool(
        "self.robot.drive",
        "Move Droid E3D in one physical direction. direction must be forward, backward, left, or "
        "right. Use this tool for Vietnamese requests such as tien, lui, re trai, re phai; never "
        "infer individual track signs. The motion stops automatically after duration_ms.",
        PropertyList({Property("direction", kPropertyTypeString).SetMaxLength(12),
                      Property("speed_pct", kPropertyTypeInteger, 45, 10, 100),
                      Property("duration_ms", kPropertyTypeInteger, 700, 100, 5000)}),
        [this](const PropertyList& properties) -> ToolResult {
            const auto& direction = properties["direction"].value<std::string>();
            if (!Drive(direction, properties["speed_pct"].value<int>(),
                       properties["duration_ms"].value<int>(), Source::Voice)) {
                return std::unexpected("direction must be forward, backward, left, or right");
            }
            return std::string("bounded drive command accepted: ") + direction;
        });

    mcp.AddTool(
        "self.robot.run_gesture",
        "Run a built-in safe gesture. name: lookLeft, lookRight, lookAround, shakeHead, waveLeft, "
        "waveRight, cheer, sad, curious, greet, show, or home. repeat 1..20 subject to120s budget. "
        "Use wave_arms for both-arm patterns; use run_sequence for combined custom actions.",
        PropertyList({Property("name", kPropertyTypeString).SetMaxLength(20),
                      Property("repeat", kPropertyTypeInteger, 1, 1, 20)}),
        [this](const PropertyList& properties) -> ToolResult {
            const auto& name = properties["name"].value<std::string>();
            if (FindGesture(name) == nullptr) {
                return std::unexpected("unknown gesture");
            }
            const std::string program =
                "{\"repeat\":" + std::to_string(properties["repeat"].value<int>()) +
                ",\"steps\":[{\"gesture\":\"" + name + "\"}]}";
            std::string error;
            if (!RunSequence(program, error))
                return std::unexpected(error);
            return std::string("Gesture started, not finished: ") + name + " " + GetStateJson();
        });

    mcp.AddTool("self.robot.stop",
                "Immediately stop both tracks and cancel the current gesture. Joint servos hold "
                "their last commanded position.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Stop(Source::Voice);
                    return true;
                });

    mcp.AddTool("self.robot.home", "Move the head and both arms to their saved home angles.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Home(Source::Voice);
                    return true;
                });

    mcp.AddTool("self.robot.state",
                "Return commanded track speeds, joint angles, calibration, active gesture, program "
                "status/frame/cyclesDone/repeat and spinCalibration (zero means not calibrated). "
                "program completed means command schedule finished, not sensor-confirmed motion. "
                "Values are commands, not sensor measurements.",
                PropertyList(),
                [this](const PropertyList&) -> ToolResult { return GetStateJson(); });
}
