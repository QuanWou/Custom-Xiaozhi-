#include "robot_face_animator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

static constexpr float PI_F = 3.14159265358979323846f;

// ============================================================
// COLOR TABLE
// ============================================================

// #C6FF4A
static constexpr uint8_t IDLE_R = 198;
static constexpr uint8_t IDLE_G = 255;
static constexpr uint8_t IDLE_B = 74;

// #FFEB5C
static constexpr uint8_t LISTEN_R = 255;
static constexpr uint8_t LISTEN_G = 235;
static constexpr uint8_t LISTEN_B = 92;

// #64D7FF
static constexpr uint8_t THINK_R = 100;
static constexpr uint8_t THINK_G = 215;
static constexpr uint8_t THINK_B = 255;

// #5AFFCD
static constexpr uint8_t SPEAK_R = 90;
static constexpr uint8_t SPEAK_G = 255;
static constexpr uint8_t SPEAK_B = 205;

// #FF665C
static constexpr uint8_t ANGRY_R = 255;
static constexpr uint8_t ANGRY_G = 102;
static constexpr uint8_t ANGRY_B = 92;

// #BEAFFF
static constexpr uint8_t TIRED_R = 190;
static constexpr uint8_t TIRED_G = 175;
static constexpr uint8_t TIRED_B = 255;

// #FF466E
static constexpr uint8_t ERROR_R = 255;
static constexpr uint8_t ERROR_G = 70;
static constexpr uint8_t ERROR_B = 110;

// ============================================================

float RobotFaceAnimator::Ease(float current, float target, float amount) {
    float delta = target - current;
    if (std::fabs(delta) < 0.01f) {
        return target;
    }
    return current + (delta * amount);
}

// ============================================================

void RobotFaceAnimator::Init(uint32_t now) {
    state_ = RobotFaceState::Idle;
    state_started_ = now;
    expression_ = RobotFaceState::Idle;
    blink_state_ = BlinkState::Idle;
    blink_amount_ = 0.0f;
    next_blink_ = now + 1800;
    eye_x_ = 0.0f;
    eye_y_ = 0.0f;
    target_eye_x_ = 0.0f;
    target_eye_y_ = 0.0f;
    next_idle_move_ = now + 1000;
    current_r_ = target_r_ = IDLE_R;
    current_g_ = target_g_ = IDLE_G;
    current_b_ = target_b_ = IDLE_B;
}

// ============================================================

void RobotFaceAnimator::SetTargetColor(uint8_t r, uint8_t g, uint8_t b) {
    target_r_ = r;
    target_g_ = g;
    target_b_ = b;
}

// ============================================================

void RobotFaceAnimator::SetState(RobotFaceState state, uint32_t now) {
    state_ = state;
    state_started_ = now;

    target_eye_x_ = 0.0f;
    target_eye_y_ = 0.0f;

    switch (state_) {
        case RobotFaceState::Idle:
        case RobotFaceState::Moving:
            SetTargetColor(IDLE_R, IDLE_G, IDLE_B);
            break;

        case RobotFaceState::Listening:
            SetTargetColor(LISTEN_R, LISTEN_G, LISTEN_B);
            break;

        case RobotFaceState::Thinking:
            SetTargetColor(THINK_R, THINK_G, THINK_B);
            break;

        case RobotFaceState::Speaking:
            SetTargetColor(SPEAK_R, SPEAK_G, SPEAK_B);
            break;

        case RobotFaceState::Happy:
            SetTargetColor(SPEAK_R, SPEAK_G, SPEAK_B);
            break;

        case RobotFaceState::Angry:
            SetTargetColor(ANGRY_R, ANGRY_G, ANGRY_B);
            break;

        case RobotFaceState::Tired:
        case RobotFaceState::Sad:
            SetTargetColor(TIRED_R, TIRED_G, TIRED_B);
            break;

        case RobotFaceState::Confused:
        case RobotFaceState::Surprised:
            SetTargetColor(THINK_R, THINK_G, THINK_B);
            break;

        case RobotFaceState::Scared:
            SetTargetColor(255, 155, 0);
            break;
        case RobotFaceState::Mischievous:
            SetTargetColor(165, 255, 32);
            break;
        case RobotFaceState::Loving:
            SetTargetColor(255, 80, 180);
            break;
        case RobotFaceState::Error:
            SetTargetColor(ERROR_R, ERROR_G, ERROR_B);
            break;
    }
}

// ============================================================

void RobotFaceAnimator::UpdateColor() {
    current_r_ = Ease(current_r_, target_r_, 0.12f);

    current_g_ = Ease(current_g_, target_g_, 0.12f);

    current_b_ = Ease(current_b_, target_b_, 0.12f);
}

// ============================================================

void RobotFaceAnimator::UpdateBlink(uint32_t now) {
    if (blink_state_ == BlinkState::Idle && static_cast<int32_t>(now - next_blink_) >= 0) {
        blink_state_ = BlinkState::Closing;
    }

    if (blink_state_ == BlinkState::Closing) {
        blink_amount_ = Ease(blink_amount_, 1.0f, 0.48f);

        if (blink_amount_ >= 0.94f) {
            blink_amount_ = 1.0f;

            blink_state_ = BlinkState::Opening;
        }
    }

    else if (blink_state_ == BlinkState::Opening) {
        blink_amount_ = Ease(blink_amount_, 0.0f, 0.33f);

        if (blink_amount_ <= 0.025f) {
            blink_amount_ = 0.0f;

            blink_state_ = BlinkState::Idle;

            next_blink_ = now + 2200 + (std::rand() % 2400);
        }
    }
}

// ============================================================

void RobotFaceAnimator::UpdateIdle(uint32_t now) {
    if (state_ != RobotFaceState::Idle && state_ != RobotFaceState::Happy &&
        state_ != RobotFaceState::Tired) {
        return;
    }

    if (static_cast<int32_t>(now - next_idle_move_) >= 0) {
        target_eye_x_ = static_cast<float>((std::rand() % 13) - 6);

        target_eye_y_ = static_cast<float>((std::rand() % 9) - 4);

        next_idle_move_ = now + 1200 + (std::rand() % 2200);
    }
}

// ============================================================

void RobotFaceAnimator::Update(uint32_t now, RobotEyeFrame& frame) {
    UpdateBlink(now);
    UpdateIdle(now);
    frame.shape = RobotEyeShape::Normal;

    uint32_t elapsed = now - state_started_;

    float state_wave = 0.0f;

    frame.left_top_cut = 0.0f;
    frame.right_top_cut = 0.0f;

    frame.left_bottom_cut = 0.0f;
    frame.right_bottom_cut = 0.0f;

    frame.left_height_factor = 1.0f;
    frame.right_height_factor = 1.0f;

    // ========================================================
    // STATE MOTION
    // ========================================================

    switch (state_) {
        case RobotFaceState::Idle: {
            // Gentle, repeating idle expressions without changing the established palette.
            const uint32_t phase = elapsed % 18000;
            if (phase >= 4500 && phase < 6500) {
                frame.left_height_factor = 0.8f;
                frame.right_height_factor = 1.05f;
                target_eye_x_ = -4.0f;
            } else if (phase >= 8000 && phase < 10300) {
                frame.left_bottom_cut = frame.right_bottom_cut = 0.22f;
                target_eye_y_ = -2.0f;
            } else if (phase >= 13000 && phase < 14200) {
                frame.left_height_factor = 0.32f;
                frame.right_height_factor = 1.08f;
            }
            break;
        }

        case RobotFaceState::Moving: {
            const float scan = std::sin(elapsed * 0.006f);
            target_eye_x_ = scan * 5.0f;
            target_eye_y_ = -1.0f;
            const uint32_t phase = (elapsed / 1600) % 3;
            if (phase == 0) {
                frame.left_height_factor = frame.right_height_factor = 0.82f;
            } else if (phase == 1) {
                frame.left_bottom_cut = frame.right_bottom_cut = 0.28f;
            } else {
                frame.left_height_factor = 1.08f;
                frame.right_height_factor = 0.92f;
            }
            break;
        }

        case RobotFaceState::Listening: {
            state_wave = std::sin(elapsed * 0.006f);

            target_eye_x_ = 0.0f;

            target_eye_y_ = state_wave * 1.5f;

            frame.left_height_factor = 1.0f + (state_wave * 0.035f);

            frame.right_height_factor = frame.left_height_factor;

            break;
        }

        case RobotFaceState::Thinking: {
            state_wave = std::sin(elapsed * 0.004f);

            target_eye_x_ = state_wave * 6.0f;

            target_eye_y_ = 0.0f;

            break;
        }

        case RobotFaceState::Speaking: {
            state_wave = std::sin(elapsed * 0.012f);

            target_eye_y_ = state_wave * 2.0f;

            target_eye_x_ = 0.0f;

            frame.left_height_factor = 1.0f + (state_wave * 0.035f);

            frame.right_height_factor = 1.0f - (state_wave * 0.025f);

            break;
        }

        case RobotFaceState::Happy: {
            frame.left_bottom_cut = 0.40f;
            frame.right_bottom_cut = 0.40f;

            break;
        }

        case RobotFaceState::Angry: {
            frame.left_top_cut = 0.38f;
            frame.right_top_cut = 0.38f;

            break;
        }

        case RobotFaceState::Tired: {
            frame.left_top_cut = 0.28f;
            frame.right_top_cut = 0.28f;

            frame.left_height_factor = 0.82f;

            frame.right_height_factor = 0.82f;

            break;
        }

        case RobotFaceState::Confused: {
            float t = static_cast<float>(elapsed) / 700.0f;

            t = std::clamp(t, 0.0f, 1.0f);

            float envelope = std::sin(PI_F * t);

            float wave = std::sin(2.0f * PI_F * 3.0f * t);

            target_eye_x_ = wave * envelope * 7.0f;

            break;
        }

        case RobotFaceState::Error: {
            state_wave = std::sin(elapsed * 0.04f);

            target_eye_x_ = state_wave * 3.5f;

            target_eye_y_ = 0.0f;

            break;
        }

        default:
            break;
    }

    // Keep activity motion; emotion controls the silhouette and its reference palette.
    RobotFaceState shown = state_;
    if (state_ == RobotFaceState::Idle || state_ == RobotFaceState::Speaking) {
        shown = expression_;
        if (state_ == RobotFaceState::Idle && expression_ == RobotFaceState::Idle) {
            // Neutral pauses between expressions keep standby varied without rapid flashing.
            static constexpr RobotFaceState sequence[] = {
                RobotFaceState::Idle,        RobotFaceState::Confused,  RobotFaceState::Happy,
                RobotFaceState::Mischievous, RobotFaceState::Surprised, RobotFaceState::Loving,
                RobotFaceState::Sad,         RobotFaceState::Scared,    RobotFaceState::Angry,
                RobotFaceState::Tired};
            shown = sequence[(elapsed / 4000) % 10];
        }
    }
    if (shown != RobotFaceState::Idle &&
        (state_ == RobotFaceState::Idle || state_ == RobotFaceState::Speaking)) {
        frame.left_top_cut = frame.right_top_cut = 0.0f;
        frame.left_bottom_cut = frame.right_bottom_cut = 0.0f;
    }
    switch (shown) {
        case RobotFaceState::Happy:
            frame.shape = RobotEyeShape::Happy;
            frame.left_bottom_cut = frame.right_bottom_cut = 0.40f;
            SetTargetColor(255, 220, 40);
            break;
        case RobotFaceState::Sad:
            frame.shape = RobotEyeShape::Sad;
            SetTargetColor(0, 235, 245);
            break;
        case RobotFaceState::Angry:
            frame.shape = RobotEyeShape::Angry;
            SetTargetColor(255, 35, 45);
            break;
        case RobotFaceState::Surprised:
            frame.shape = RobotEyeShape::Surprised;
            frame.left_height_factor = frame.right_height_factor = 1.12f;
            SetTargetColor(205, 100, 255);
            break;
        case RobotFaceState::Scared:
            frame.shape = RobotEyeShape::Scared;
            target_eye_x_ = std::sin(elapsed * 0.035f) * 2.0f;
            SetTargetColor(255, 155, 0);
            break;
        case RobotFaceState::Tired:
            frame.shape = RobotEyeShape::Sleepy;
            frame.left_height_factor = frame.right_height_factor = 0.85f;
            SetTargetColor(40, 255, 90);
            break;
        case RobotFaceState::Mischievous:
            frame.shape = RobotEyeShape::Mischievous;
            SetTargetColor(165, 255, 32);
            break;
        case RobotFaceState::Loving:
            frame.shape = RobotEyeShape::Loving;
            SetTargetColor(255, 80, 180);
            break;
        case RobotFaceState::Confused:
            frame.left_height_factor *= 0.65f;
            frame.right_height_factor *= 1.05f;
            SetTargetColor(IDLE_R, IDLE_G, IDLE_B);
            break;
        default:
            // Restore activity color after an emotion ends.
            if (state_ == RobotFaceState::Idle || state_ == RobotFaceState::Moving) {
                SetTargetColor(IDLE_R, IDLE_G, IDLE_B);
            } else if (state_ == RobotFaceState::Speaking) {
                SetTargetColor(SPEAK_R, SPEAK_G, SPEAK_B);
            }
            break;
    }
    UpdateColor();

    eye_x_ = Ease(eye_x_, target_eye_x_, 0.16f);

    eye_y_ = Ease(eye_y_, target_eye_y_, 0.16f);

    // ========================================================
    // BLINK
    // ========================================================

    float open_factor = 1.0f - (blink_amount_ * 0.965f);

    open_factor = std::max(open_factor, 0.05f);

    frame.left_height_factor *= open_factor;

    frame.right_height_factor *= open_factor;

    frame.offset_x = eye_x_;
    frame.offset_y = eye_y_;
}

// ============================================================

void RobotFaceAnimator::GetColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    r = static_cast<uint8_t>(std::clamp(current_r_, 0.0f, 255.0f));

    g = static_cast<uint8_t>(std::clamp(current_g_, 0.0f, 255.0f));

    b = static_cast<uint8_t>(std::clamp(current_b_, 0.0f, 255.0f));
}
