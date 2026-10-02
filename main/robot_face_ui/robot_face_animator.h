#pragma once

#include <cstdint>

enum class RobotEyeShape {
    Normal,
    Happy,
    Sad,
    Angry,
    Surprised,
    Scared,
    Sleepy,
    Mischievous,
    Loving
};

struct RobotEyeFrame {
    RobotEyeShape shape = RobotEyeShape::Normal;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    float left_height_factor = 1.0f;
    float right_height_factor = 1.0f;

    float left_top_cut = 0.0f;
    float right_top_cut = 0.0f;

    float left_bottom_cut = 0.0f;
    float right_bottom_cut = 0.0f;
};

enum class RobotFaceState {
    Idle,
    Moving,
    Listening,
    Thinking,
    Speaking,
    Happy,
    Angry,
    Tired,
    Confused,
    Sad,
    Surprised,
    Error,
    Scared,
    Mischievous,
    Loving
};

class RobotFaceAnimator {
public:
    void Init(uint32_t now);

    void SetState(RobotFaceState state, uint32_t now);
    void SetExpression(RobotFaceState expression) { expression_ = expression; }

    RobotFaceState GetState() const { return state_; }

    void Update(uint32_t now, RobotEyeFrame& frame);

    void GetColor(uint8_t& r, uint8_t& g, uint8_t& b) const;

private:
    float Ease(float current, float target, float amount);

    void SetTargetColor(uint8_t r, uint8_t g, uint8_t b);

    void UpdateColor();
    void UpdateBlink(uint32_t now);
    void UpdateIdle(uint32_t now);

private:
    RobotFaceState state_ = RobotFaceState::Idle;
    RobotFaceState expression_ = RobotFaceState::Idle;

    uint32_t state_started_ = 0;

    // -----------------------------
    // Blink
    // -----------------------------

    enum class BlinkState { Idle, Closing, Opening };

    BlinkState blink_state_ = BlinkState::Idle;

    float blink_amount_ = 0.0f;

    uint32_t next_blink_ = 0;

    // -----------------------------
    // Eye movement
    // -----------------------------

    float eye_x_ = 0.0f;
    float eye_y_ = 0.0f;

    float target_eye_x_ = 0.0f;
    float target_eye_y_ = 0.0f;

    uint32_t next_idle_move_ = 0;

    // -----------------------------
    // Colors
    // -----------------------------

    float current_r_ = 198.0f;
    float current_g_ = 255.0f;
    float current_b_ = 74.0f;

    float target_r_ = 198.0f;
    float target_g_ = 255.0f;
    float target_b_ = 74.0f;
};
