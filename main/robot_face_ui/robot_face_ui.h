#pragma once

#include "display/lcd_display.h"
#include "robot_face_animator.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

class RobotFaceDisplay : public SpiLcdDisplay {
public:
    RobotFaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                     int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                     bool swap_xy);
    ~RobotFaceDisplay() override;

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void ShowNotification(const char* notification, int duration_ms = 3000) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void SetTheme(Theme* theme) override;
    void UpdateStatusBar(bool update_all = false) override;
    void SetPowerSaveMode(bool on) override;
    void SetMotionProvider(std::function<bool()> provider);

private:
    static void EyeDrawCallback(lv_event_t* event);
    RobotEyeFrame eye_frame_;

    static void AnimationTimerCallback(lv_timer_t* timer);
    static void NotificationTimerCallback(lv_timer_t* timer);

    void CreateStatusArea();
    void CreateEyes();
    void CreateResponseArea();
    void ApplyTextFont();

    void SetState(RobotFaceState state);
    void SyncDeviceState();
    void UpdateAnimation();
    void UpdateEyeGeometry(const RobotEyeFrame& frame);
    void UpdateColors();
    void UpdateResponseAppearance(size_t word_count);
    void RefreshResponseText();
    void SetWifiConnected(bool connected);

    static size_t CountWords(const char* text);
    static RobotFaceState StateForStatus(const char* status, RobotFaceState current);
    static RobotFaceState StateForEmotion(const char* emotion, RobotFaceState current);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* root_ = nullptr;

    lv_obj_t* wifi_icon_ = nullptr;
    lv_obj_t* status_dot_ = nullptr;
    lv_obj_t* activity_label_ = nullptr;

    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* left_top_mask_ = nullptr;
    lv_obj_t* right_top_mask_ = nullptr;
    lv_obj_t* left_bottom_mask_ = nullptr;
    lv_obj_t* right_bottom_mask_ = nullptr;

    lv_obj_t* response_box_ = nullptr;
    lv_obj_t* response_prompt_ = nullptr;
    lv_obj_t* response_label_ = nullptr;

    lv_timer_t* animation_timer_ = nullptr;
    lv_timer_t* notification_restore_timer_ = nullptr;

    RobotFaceAnimator animator_;
    std::function<bool()> motion_provider_;

    std::string status_text_;
    std::string response_text_;
    std::string notification_text_;

    bool initialized_ = false;
    bool wifi_connected_ = false;
    bool notification_active_ = false;
    bool power_save_ = false;
    bool awaiting_reply_ = false;
    uint32_t reply_wait_started_ = 0;

    bool color_initialized_ = false;
    int last_device_state_ = -1;
    uint8_t last_r_ = 0;
    uint8_t last_g_ = 0;
    uint8_t last_b_ = 0;

    static constexpr int WIFI_X = 10;
    static constexpr int WIFI_Y = 7;

    static constexpr int STATUS_X = 218;
    static constexpr int STATUS_Y = 10;
    static constexpr int STATUS_SIZE = 10;

    static constexpr int LEFT_EYE_X = 27;
    static constexpr int RIGHT_EYE_X = 139;
    static constexpr int EYE_Y = 45;
    static constexpr int EYE_W = 74;
    static constexpr int EYE_H = 68;
    static constexpr int EYE_RADIUS = 18;

    static constexpr int RESPONSE_X = 8;
    static constexpr int RESPONSE_Y = 139;
    static constexpr int RESPONSE_W = 224;
    static constexpr int RESPONSE_H = 93;
    static constexpr int RESPONSE_RADIUS = 7;
};
