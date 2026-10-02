#include "robot_face_ui.h"

#include "application.h"
#include "assets/lang_config.h"
#include "display/lvgl_display/lvgl_font.h"
#include "wifi_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

constexpr uint32_t COLOR_BG = 0x000000;
constexpr uint32_t COLOR_TEXT = 0xF5F7F8;
constexpr uint32_t COLOR_BOX_BG = 0x020506;
constexpr uint32_t COLOR_WIFI_OFF = 0x555B60;
constexpr uint32_t ANIMATION_PERIOD_MS = 40;

uint32_t GetMillis() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }

bool Equals(const char* value, const char* expected) {
    return value != nullptr && expected != nullptr && std::strcmp(value, expected) == 0;
}

}  // namespace

RobotFaceDisplay::RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

RobotFaceDisplay::~RobotFaceDisplay() {
    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    if (animation_timer_ != nullptr) {
        lv_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
    if (notification_restore_timer_ != nullptr) {
        lv_timer_delete(notification_restore_timer_);
        notification_restore_timer_ = nullptr;
    }
    if (root_ != nullptr && lv_obj_is_valid(root_)) {
        lv_obj_delete(root_);
    }

    root_ = nullptr;
    screen_ = nullptr;
    wifi_icon_ = nullptr;
    status_dot_ = nullptr;
    left_eye_ = nullptr;
    right_eye_ = nullptr;
    left_top_mask_ = nullptr;
    right_top_mask_ = nullptr;
    left_bottom_mask_ = nullptr;
    right_bottom_mask_ = nullptr;
    response_box_ = nullptr;
    response_prompt_ = nullptr;
    response_label_ = nullptr;
}

void RobotFaceDisplay::SetupUI() {
    if (initialized_) {
        return;
    }

    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    screen_ = lv_screen_active();
    if (screen_ == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(screen_, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    root_ = lv_obj_create(screen_);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    CreateStatusArea();
    CreateEyes();
    CreateResponseArea();
    ApplyTextFont();

    animator_.Init(GetMillis());
    animation_timer_ = lv_timer_create(AnimationTimerCallback, ANIMATION_PERIOD_MS, this);
    notification_restore_timer_ = lv_timer_create(NotificationTimerCallback, 3000, this);
    lv_timer_pause(notification_restore_timer_);

    status_text_ = Lang::Strings::INITIALIZING;
    initialized_ = true;
    Display::SetupUI();

    SetWifiConnected(WifiManager::GetInstance().IsConnected());
    RefreshResponseText();
    UpdateAnimation();
}

void RobotFaceDisplay::CreateStatusArea() {
    wifi_icon_ = lv_label_create(root_);
    lv_label_set_text(wifi_icon_, LV_SYMBOL_WIFI);
    lv_obj_set_pos(wifi_icon_, WIFI_X, WIFI_Y);
    activity_label_ = lv_label_create(root_);
    lv_obj_set_pos(activity_label_, 36, 6);
    lv_obj_set_width(activity_label_, 174);
    lv_obj_set_style_text_align(activity_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(activity_label_, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(activity_label_, "");

    status_dot_ = lv_obj_create(root_);
    lv_obj_remove_style_all(status_dot_);
    lv_obj_set_size(status_dot_, STATUS_SIZE, STATUS_SIZE);
    lv_obj_set_pos(status_dot_, STATUS_X, STATUS_Y);
    lv_obj_set_style_radius(status_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(status_dot_, LV_OPA_COVER, 0);
}

void RobotFaceDisplay::CreateEyes() {
    left_eye_ = lv_obj_create(root_);
    lv_obj_remove_style_all(left_eye_);
    lv_obj_set_size(left_eye_, EYE_W, EYE_H);
    lv_obj_set_pos(left_eye_, LEFT_EYE_X, EYE_Y);
    lv_obj_set_style_radius(left_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);

    right_eye_ = lv_obj_create(root_);
    lv_obj_remove_style_all(right_eye_);
    lv_obj_set_size(right_eye_, EYE_W, EYE_H);
    lv_obj_set_pos(right_eye_, RIGHT_EYE_X, EYE_Y);
    lv_obj_set_style_radius(right_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(left_eye_, EyeDrawCallback, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(right_eye_, EyeDrawCallback, LV_EVENT_DRAW_MAIN, this);

    left_top_mask_ = lv_obj_create(root_);
    right_top_mask_ = lv_obj_create(root_);
    left_bottom_mask_ = lv_obj_create(root_);
    right_bottom_mask_ = lv_obj_create(root_);

    lv_obj_t* masks[] = {left_top_mask_, right_top_mask_, left_bottom_mask_, right_bottom_mask_};
    for (lv_obj_t* mask : masks) {
        lv_obj_remove_style_all(mask);
        lv_obj_set_style_bg_color(mask, lv_color_hex(COLOR_BG), 0);
        lv_obj_set_style_bg_opa(mask, LV_OPA_COVER, 0);
        lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
    }
}

void RobotFaceDisplay::CreateResponseArea() {
    response_box_ = lv_obj_create(root_);
    lv_obj_set_pos(response_box_, RESPONSE_X, RESPONSE_Y);
    lv_obj_set_size(response_box_, RESPONSE_W, RESPONSE_H);
    lv_obj_clear_flag(response_box_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(response_box_, lv_color_hex(COLOR_BOX_BG), 0);
    lv_obj_set_style_bg_opa(response_box_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(response_box_, 1, 0);
    lv_obj_set_style_radius(response_box_, RESPONSE_RADIUS, 0);
    lv_obj_set_style_pad_all(response_box_, 0, 0);

    response_prompt_ = lv_label_create(response_box_);
    lv_label_set_text(response_prompt_, ">");
    lv_obj_set_pos(response_prompt_, 7, 4);

    response_label_ = lv_label_create(response_box_);
    lv_obj_set_pos(response_label_, 20, 4);
    lv_obj_set_size(response_label_, RESPONSE_W - 28, RESPONSE_H - 8);
    lv_label_set_long_mode(response_label_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(response_label_, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_line_space(response_label_, 2, 0);
}

void RobotFaceDisplay::ApplyTextFont() {
    if (current_theme_ == nullptr) {
        return;
    }

    auto text_font = current_theme_->GetTextFont();
    if (text_font == nullptr || text_font->font() == nullptr) {
        return;
    }

    if (response_prompt_ != nullptr) {
        lv_obj_set_style_text_font(response_prompt_, text_font->font(), 0);
    }
    if (activity_label_ != nullptr) {
        lv_obj_set_style_text_font(activity_label_, text_font->font(), 0);
    }
    if (response_label_ != nullptr) {
        lv_obj_set_style_text_font(response_label_, text_font->font(), 0);
    }
}

void RobotFaceDisplay::SetStatus(const char* status) {
    if (status == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    status_text_ = status;
    if (Equals(status, Lang::Strings::LISTENING)) {
        awaiting_reply_ = false;
        response_text_.clear();
        animator_.SetExpression(RobotFaceState::Idle);
    }
    SetState(StateForStatus(status, animator_.GetState()));
    RefreshResponseText();
}

void RobotFaceDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (notification == nullptr || notification[0] == '\0') {
        return;
    }

    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    notification_text_ = notification;
    notification_active_ = true;
    RefreshResponseText();

    if (notification_restore_timer_ != nullptr) {
        lv_timer_set_period(notification_restore_timer_,
                            static_cast<uint32_t>(std::max(duration_ms, 1)));
        lv_timer_reset(notification_restore_timer_);
        lv_timer_resume(notification_restore_timer_);
    }
}

void RobotFaceDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    animator_.SetExpression(StateForEmotion(emotion, RobotFaceState::Idle));
}

void RobotFaceDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    response_text_ = content != nullptr ? content : "";
    if (Equals(role, "user") && !response_text_.empty()) {
        awaiting_reply_ = true;
        reply_wait_started_ = GetMillis();
    } else if (Equals(role, "assistant")) {
        awaiting_reply_ = false;
    }
    RefreshResponseText();
}

void RobotFaceDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    response_text_.clear();
    awaiting_reply_ = false;
    RefreshResponseText();
}

void RobotFaceDisplay::SetTheme(Theme* theme) {
    if (theme == nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    if (!lock) {
        return;
    }

    Display::SetTheme(theme);
    if (initialized_) {
        ApplyTextFont();
    }
}

void RobotFaceDisplay::UpdateStatusBar(bool update_all) {
    (void)update_all;
    bool connected = WifiManager::GetInstance().IsConnected();

    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    SetWifiConnected(connected);
}

void RobotFaceDisplay::SetPowerSaveMode(bool on) {
    DisplayLockGuard lock(this);
    if (!lock || !initialized_) {
        return;
    }

    power_save_ = on;
    SetState(on ? RobotFaceState::Tired : RobotFaceState::Idle);
}

void RobotFaceDisplay::SetState(RobotFaceState state) {
    if (animator_.GetState() != state) {
        animator_.SetState(state, GetMillis());
    }
}

void RobotFaceDisplay::SetMotionProvider(std::function<bool()> provider) {
    motion_provider_ = std::move(provider);
}

RobotFaceState RobotFaceDisplay::StateForStatus(const char* status, RobotFaceState current) {
    if (Equals(status, Lang::Strings::LISTENING)) {
        return RobotFaceState::Listening;
    }
    if (Equals(status, Lang::Strings::SPEAKING)) {
        return RobotFaceState::Speaking;
    }
    if (Equals(status, Lang::Strings::STANDBY)) {
        return RobotFaceState::Idle;
    }
    if (Equals(status, Lang::Strings::ERROR)) {
        return RobotFaceState::Error;
    }
    if (Equals(status, Lang::Strings::CONNECTING) ||
        Equals(status, Lang::Strings::CHECKING_NEW_VERSION) ||
        Equals(status, Lang::Strings::LOADING_ASSETS) ||
        Equals(status, Lang::Strings::LOADING_PROTOCOL) ||
        Equals(status, Lang::Strings::REGISTERING_NETWORK) ||
        Equals(status, Lang::Strings::PLEASE_WAIT) || Equals(status, Lang::Strings::ACTIVATION) ||
        Equals(status, Lang::Strings::INITIALIZING)) {
        return RobotFaceState::Thinking;
    }
    return current;
}

RobotFaceState RobotFaceDisplay::StateForEmotion(const char* emotion, RobotFaceState current) {
    if (Equals(emotion, "happy") || Equals(emotion, "laughing") || Equals(emotion, "cool") ||
        Equals(emotion, "delicious")) {
        return RobotFaceState::Happy;
    }
    if (Equals(emotion, "loving") || Equals(emotion, "love")) {
        return RobotFaceState::Loving;
    }
    if (Equals(emotion, "scared") || Equals(emotion, "fearful") || Equals(emotion, "afraid")) {
        return RobotFaceState::Scared;
    }
    if (Equals(emotion, "silly") || Equals(emotion, "winking") || Equals(emotion, "mischievous")) {
        return RobotFaceState::Mischievous;
    }
    if (Equals(emotion, "angry")) {
        return RobotFaceState::Angry;
    }
    if (Equals(emotion, "sad") || Equals(emotion, "crying")) {
        return RobotFaceState::Sad;
    }
    if (Equals(emotion, "surprised") || Equals(emotion, "shocked")) {
        return RobotFaceState::Surprised;
    }
    if (Equals(emotion, "sleepy") || Equals(emotion, "tired")) {
        return RobotFaceState::Tired;
    }
    if (Equals(emotion, "confused")) {
        return RobotFaceState::Confused;
    }
    if (Equals(emotion, "warning") || Equals(emotion, "cancel")) {
        return RobotFaceState::Error;
    }
    if (Equals(emotion, "link")) {
        return RobotFaceState::Thinking;
    }
    return current;
}

size_t RobotFaceDisplay::CountWords(const char* text) {
    if (text == nullptr) {
        return 0;
    }

    size_t count = 0;
    bool in_word = false;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != 0;
         ++cursor) {
        bool whitespace = *cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n';
        if (whitespace) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++count;
        }
    }
    return count;
}

void RobotFaceDisplay::RefreshResponseText() {
    if (response_label_ == nullptr) {
        return;
    }

    const std::string* text = &status_text_;
    if (notification_active_) {
        text = &notification_text_;
    } else if (!response_text_.empty()) {
        text = &response_text_;
    }

    UpdateResponseAppearance(CountWords(text->c_str()));
    lv_label_set_text(response_label_, text->c_str());
}

void RobotFaceDisplay::UpdateResponseAppearance(size_t word_count) {
    if (response_label_ == nullptr) {
        return;
    }

    if (word_count <= 12) {
        lv_obj_set_style_text_line_space(response_label_, 1, 0);
    } else {
        lv_obj_set_style_text_line_space(response_label_, 0, 0);
    }
}

void RobotFaceDisplay::SetWifiConnected(bool connected) {
    wifi_connected_ = connected;
    if (wifi_icon_ != nullptr) {
        lv_obj_set_style_text_color(wifi_icon_, lv_color_hex(connected ? 0xFFFFFF : COLOR_WIFI_OFF),
                                    0);
    }
}

void RobotFaceDisplay::UpdateColors() {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    animator_.GetColor(r, g, b);

    if (color_initialized_ && r == last_r_ && g == last_g_ && b == last_b_) {
        return;
    }

    color_initialized_ = true;
    last_r_ = r;
    last_g_ = g;
    last_b_ = b;

    lv_color_t color = lv_color_make(r, g, b);
    lv_obj_set_style_bg_color(left_eye_, color, 0);
    lv_obj_set_style_bg_color(right_eye_, color, 0);
    lv_obj_set_style_bg_color(status_dot_, color, 0);
    lv_obj_set_style_border_color(response_box_, color, 0);
    lv_obj_set_style_text_color(response_prompt_, color, 0);
}

void RobotFaceDisplay::UpdateEyeGeometry(const RobotEyeFrame& frame) {
    eye_frame_ = frame;
    const bool happy = frame.shape == RobotEyeShape::Happy;
    lv_obj_set_style_bg_opa(left_eye_, happy ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(right_eye_, happy ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_invalidate(left_eye_);
    lv_obj_invalidate(right_eye_);
    float left_factor = std::max(0.05f, frame.left_height_factor);
    float right_factor = std::max(0.05f, frame.right_height_factor);

    int left_h = std::max(4, static_cast<int>(EYE_H * left_factor));
    int right_h = std::max(4, static_cast<int>(EYE_H * right_factor));
    int offset_x = static_cast<int>(std::lround(frame.offset_x));
    int offset_y = static_cast<int>(std::lround(frame.offset_y));
    int left_y = EYE_Y + offset_y + ((EYE_H - left_h) / 2);
    int right_y = EYE_Y + offset_y + ((EYE_H - right_h) / 2);

    lv_obj_set_pos(left_eye_, LEFT_EYE_X + offset_x, left_y);
    lv_obj_set_pos(right_eye_, RIGHT_EYE_X + offset_x, right_y);
    lv_obj_set_size(left_eye_, EYE_W, left_h);
    lv_obj_set_size(right_eye_, EYE_W, right_h);
    lv_obj_set_style_radius(left_eye_, std::min(EYE_RADIUS, left_h / 2), 0);
    lv_obj_set_style_radius(right_eye_, std::min(EYE_RADIUS, right_h / 2), 0);

    lv_obj_add_flag(left_top_mask_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_top_mask_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(left_bottom_mask_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_bottom_mask_, LV_OBJ_FLAG_HIDDEN);

    if (frame.shape != RobotEyeShape::Normal) {
        return;
    }

    if (frame.left_top_cut > 0.01f) {
        int height = static_cast<int>(left_h * frame.left_top_cut);
        lv_obj_set_pos(left_top_mask_, LEFT_EYE_X + offset_x - 2, left_y - 1);
        lv_obj_set_size(left_top_mask_, EYE_W + 4, height);
        lv_obj_clear_flag(left_top_mask_, LV_OBJ_FLAG_HIDDEN);
    }
    if (frame.right_top_cut > 0.01f) {
        int height = static_cast<int>(right_h * frame.right_top_cut);
        lv_obj_set_pos(right_top_mask_, RIGHT_EYE_X + offset_x - 2, right_y - 1);
        lv_obj_set_size(right_top_mask_, EYE_W + 4, height);
        lv_obj_clear_flag(right_top_mask_, LV_OBJ_FLAG_HIDDEN);
    }
    if (frame.left_bottom_cut > 0.01f) {
        int height = static_cast<int>(left_h * frame.left_bottom_cut);
        lv_obj_set_pos(left_bottom_mask_, LEFT_EYE_X + offset_x - 2, left_y + left_h - height);
        lv_obj_set_size(left_bottom_mask_, EYE_W + 4, height + 2);
        lv_obj_clear_flag(left_bottom_mask_, LV_OBJ_FLAG_HIDDEN);
    }
    if (frame.right_bottom_cut > 0.01f) {
        int height = static_cast<int>(right_h * frame.right_bottom_cut);
        lv_obj_set_pos(right_bottom_mask_, RIGHT_EYE_X + offset_x - 2, right_y + right_h - height);
        lv_obj_set_size(right_bottom_mask_, EYE_W + 4, height + 2);
        lv_obj_clear_flag(right_bottom_mask_, LV_OBJ_FLAG_HIDDEN);
    }
}

void RobotFaceDisplay::EyeDrawCallback(lv_event_t* event) {
    auto* self = static_cast<RobotFaceDisplay*>(lv_event_get_user_data(event));
    auto* eye = static_cast<lv_obj_t*>(lv_event_get_target(event));
    const RobotEyeShape shape = self->eye_frame_.shape;
    if (shape == RobotEyeShape::Normal || shape == RobotEyeShape::Surprised) {
        return;
    }
    lv_area_t area;
    lv_obj_get_coords(eye, &area);
    const int w = lv_area_get_width(&area);
    const int h = lv_area_get_height(&area);
    if (h < 12) {
        return;
    }
    const bool right = eye == self->right_eye_;
    auto* layer = lv_event_get_layer(event);
    if (shape == RobotEyeShape::Angry) {
        lv_draw_triangle_dsc_t triangle;
        lv_draw_triangle_dsc_init(&triangle);
        triangle.color = lv_color_hex(COLOR_BG);
        triangle.p[0] = {area.x1, area.y1};
        triangle.p[1] = {area.x2, area.y1};
        triangle.p[2] = {right ? area.x1 : area.x2, area.y1 + h * 3 / 10};
        lv_draw_triangle(layer, &triangle);
        return;
    }
    if (shape == RobotEyeShape::Mischievous && !right) {
        return;
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = shape == RobotEyeShape::Happy ? lv_obj_get_style_bg_color(eye, LV_PART_MAIN)
                                               : lv_color_hex(COLOR_BG);
    line.width = shape == RobotEyeShape::Happy ? std::max(3, h / 4) : std::max(2, h / 12);
    line.round_start = line.round_end = 1;
    // Bounded polylines draw curved eyelids without bitmap buffers.
    auto curve_y = [&](float t) {
        const float arch = 4.0f * t * (1.0f - t);
        if (shape == RobotEyeShape::Happy) {
            return h * (0.72f - 0.47f * arch);
        }
        if (shape == RobotEyeShape::Loving || shape == RobotEyeShape::Mischievous) {
            return h * (0.65f - 0.25f * arch);
        }
        if (shape == RobotEyeShape::Sleepy) {
            return h * (0.65f + 0.12f * arch);
        }
        const float tilt = (right ? 1.0f - t : t);
        return h * (shape == RobotEyeShape::Scared ? 0.12f + 0.17f * tilt + 0.10f * arch
                                                   : 0.28f + 0.17f * tilt + 0.10f * arch);
    };
    const int margin = shape == RobotEyeShape::Happy ? line.width / 2 : 8;
    for (int i = 0; i < 16; ++i) {
        const float t0 = i / 16.0f;
        const float t1 = (i + 1) / 16.0f;
        if (shape == RobotEyeShape::Loving || shape == RobotEyeShape::Mischievous ||
            shape == RobotEyeShape::Sleepy) {
            line.width =
                std::max(2, static_cast<int>(h * 0.14f * std::sin(3.14159265f * (t0 + t1) / 2)));
        }
        line.p1 = {area.x1 + margin + static_cast<int>((w - 2 * margin) * t0),
                   area.y1 + static_cast<int>(curve_y(t0))};
        line.p2 = {area.x1 + margin + static_cast<int>((w - 2 * margin) * t1),
                   area.y1 + static_cast<int>(curve_y(t1))};
        lv_draw_line(layer, &line);
    }
    if (shape == RobotEyeShape::Loving) {
        line.color = lv_color_hex(0xA81862);
        line.width = 3;
        for (int i = 0; i < 2; ++i) {
            const int x = area.x1 + (right ? w - 23 : 9) + i * 8;
            line.p1 = {x, area.y1 + h * 4 / 5};
            line.p2 = {x + 4, area.y1 + h * 4 / 5 - 6};
            lv_draw_line(layer, &line);
        }
    }
}

void RobotFaceDisplay::UpdateAnimation() {
    if (!initialized_) {
        return;
    }

    SyncDeviceState();
    RobotEyeFrame frame;
    animator_.Update(GetMillis(), frame);
    UpdateEyeGeometry(frame);
    UpdateColors();
}

void RobotFaceDisplay::SyncDeviceState() {
    const DeviceState device_state = Application::GetInstance().GetDeviceState();
    const bool changed = static_cast<int>(device_state) != last_device_state_;
    last_device_state_ = static_cast<int>(device_state);
    if (awaiting_reply_ && GetMillis() - reply_wait_started_ > 15000) {
        awaiting_reply_ = false;
    }
    const char* activity = "Sẵn sàng";

    switch (device_state) {
        case kDeviceStateIdle:
        case kDeviceStateUnknown:
            if (changed) {
                animator_.SetExpression(RobotFaceState::Idle);
            }
            if (power_save_) {
                SetState(RobotFaceState::Tired);
                activity = "Đang nghỉ";
            } else if (motion_provider_ && motion_provider_()) {
                SetState(RobotFaceState::Moving);
                activity = "Đang chuyển động";
            } else {
                SetState(RobotFaceState::Idle);
                activity = "Sẵn sàng";
            }
            awaiting_reply_ = false;
            break;
        case kDeviceStateListening:
        case kDeviceStateAudioTesting:
            SetState(awaiting_reply_ ? RobotFaceState::Thinking : RobotFaceState::Listening);
            activity = awaiting_reply_ ? "Đang suy nghĩ" : "Đang nghe";
            break;
        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
            SetState(RobotFaceState::Speaking);
            activity = "Đang nói";
            awaiting_reply_ = false;
            break;
        case kDeviceStateWifiConfiguring:
            SetState(RobotFaceState::Confused);
            activity = "Cấu hình Wi-Fi";
            break;
        case kDeviceStateFatalError:
            SetState(RobotFaceState::Error);
            activity = "Lỗi kết nối";
            break;
        case kDeviceStateStarting:
        case kDeviceStateConnecting:
        case kDeviceStateActivating:
        case kDeviceStateUpgrading:
            SetState(RobotFaceState::Thinking);
            activity = "Đang kết nối";
            break;
        default:
            break;
    }
    if (changed || std::strcmp(lv_label_get_text(activity_label_), activity) != 0) {
        lv_label_set_text(activity_label_, activity);
    }
}

void RobotFaceDisplay::AnimationTimerCallback(lv_timer_t* timer) {
    if (timer == nullptr) {
        return;
    }

    auto* self = static_cast<RobotFaceDisplay*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->UpdateAnimation();
    }
}

void RobotFaceDisplay::NotificationTimerCallback(lv_timer_t* timer) {
    if (timer == nullptr) {
        return;
    }

    auto* self = static_cast<RobotFaceDisplay*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->notification_active_ = false;
        self->notification_text_.clear();
        self->RefreshResponseText();
    }
    lv_timer_pause(timer);
}
