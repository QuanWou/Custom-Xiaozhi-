#include "droid_control_server.h"

#include "config.h"
#include "droid_audio_codec.h"
#include "droid_robot_controller.h"
#include "droid_web_ui.h"

#include <esp_app_desc.h>
#include "application.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_manager.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <utility>

#define TAG "DroidWeb"

DroidControlServer* DroidControlServer::instance_ = nullptr;

namespace {

const char* JsonString(const cJSON* object, const char* name) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(value) ? value->valuestring : nullptr;
}

bool JsonInt(const cJSON* object, const char* name, int& result) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) ||
        value->valuedouble < INT_MIN || value->valuedouble > INT_MAX ||
        std::floor(value->valuedouble) != value->valuedouble) {
        return false;
    }
    result = value->valueint;
    return true;
}

}  // namespace

DroidControlServer::DroidControlServer(DroidRobotController& controller,
                                       DroidAudioCodec& audio_codec,
                                       std::function<void()> reconfigure_wifi)
    : controller_(controller),
      audio_codec_(audio_codec),
      reconfigure_wifi_(std::move(reconfigure_wifi)) {
    instance_ = this;
}

DroidControlServer::~DroidControlServer() {
    Stop();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

bool DroidControlServer::Start(uint16_t port) {
    if (server_ != nullptr) {
        return true;
    }
    port_ = port;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.ctrl_port = 32769;
    config.max_uri_handlers = 8;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start control server on port %u", port_);
        server_ = nullptr;
        return false;
    }

    const httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = RootHandler, .user_ctx = nullptr},
        {.uri = "/control", .method = HTTP_GET, .handler = ControlPageHandler, .user_ctx = nullptr},
        {.uri = "/wifi", .method = HTTP_GET, .handler = WifiPageHandler, .user_ctx = nullptr},
        {.uri = "/api/status", .method = HTTP_GET, .handler = StatusHandler, .user_ctx = nullptr},
        {.uri = "/api/robot",
         .method = HTTP_POST,
         .handler = RobotCommandHandler,
         .user_ctx = nullptr},
        {.uri = "/api/wifi/reconfigure",
         .method = HTTP_POST,
         .handler = WifiReconfigureHandler,
         .user_ctx = nullptr},
    };
    for (const auto& handler : handlers) {
        if (httpd_register_uri_handler(server_, &handler) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s", handler.uri);
            Stop();
            return false;
        }
    }

    ESP_LOGI(TAG, "Control UI: http://<device-ip>:%u/control", port_);
    return true;
}

void DroidControlServer::Stop() {
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

esp_err_t DroidControlServer::RootHandler(httpd_req_t* req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/control");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t DroidControlServer::ControlPageHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, kDroidControlPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t DroidControlServer::WifiPageHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, kDroidWifiPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t DroidControlServer::StatusHandler(httpd_req_t* req) {
    if (instance_ == nullptr) {
        return SendJson(req, R"({"error":"server unavailable"})", 503);
    }
    return SendJson(req, instance_->BuildStatusJson());
}

esp_err_t DroidControlServer::RobotCommandHandler(httpd_req_t* req) {
    if (instance_ == nullptr) {
        return SendJson(req, R"({"success":false,"error":"server unavailable"})", 503);
    }
    return instance_->HandleRobotCommand(req);
}

esp_err_t DroidControlServer::WifiReconfigureHandler(httpd_req_t* req) {
    if (instance_ == nullptr || !instance_->reconfigure_wifi_) {
        return SendJson(req, R"({"success":false,"error":"Wi-Fi action unavailable"})", 503);
    }

    instance_->controller_.Stop(DroidRobotController::Source::Web);
    const esp_err_t response =
        SendJson(req, R"({"success":true,"message":"Entering Wi-Fi configuration mode"})");
    if (xTaskCreate(ReconfigureTask, "wifi_reconfigure", 3072, instance_, 3, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi reconfiguration task");
    }
    return response;
}

void DroidControlServer::ReconfigureTask(void* arg) {
    auto* server = static_cast<DroidControlServer*>(arg);
    vTaskDelay(pdMS_TO_TICKS(350));
    if (server != nullptr && server->reconfigure_wifi_) {
        server->reconfigure_wifi_();
    }
    vTaskDelete(nullptr);
}

esp_err_t DroidControlServer::HandleRobotCommand(httpd_req_t* req) {
    std::string body;
    if (!ReadRequestBody(req, body)) {
        return SendJson(req, R"({"success":false,"error":"invalid request body"})", 400);
    }

    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (root == nullptr) {
        return SendJson(req, R"({"success":false,"error":"invalid JSON"})", 400);
    }

    const char* command = JsonString(root, "command");
    bool success = false;
    const char* message = "Command accepted";
    std::string motion_error;

    if (command == nullptr) {
        message = "Missing command";
    } else if (std::strcmp(command, "stop") == 0) {
        controller_.Stop(DroidRobotController::Source::Web);
        success = true;
        message = "Robot stopped";
    } else if (std::strcmp(command, "heartbeat") == 0) {
        controller_.HeartbeatWebControl();
        success = true;
        message = "";
    } else if (std::strcmp(command, "stop_tracks") == 0) {
        controller_.StopTracks(DroidRobotController::Source::Web);
        success = true;
        message = "";
    } else if (std::strcmp(command, "ai_toggle") == 0) {
        Application::GetInstance().Schedule([]() { Application::GetInstance().ToggleChatState(); });
        success = true;
        message = "Đã gửi lệnh gọi hoặc dừng XiaoZhi.";
    } else if (std::strcmp(command, "wheels") == 0) {
        int left = 0, right = 0;
        success = JsonInt(root, "left", left) && JsonInt(root, "right", right) &&
                  controller_.SetTracks(left, right, 700, DroidRobotController::Source::Web);
        message = success ? "" : "Invalid wheel command";
    } else if (std::strcmp(command, "display_rotation") == 0) {
        int rotation = 0;
        if (JsonInt(root, "rotation", rotation) && rotation >= 0 && rotation <= 3) {
            controller_.Stop(DroidRobotController::Source::Web);
            Settings settings("droid-display", true);
            settings.SetInt("rotation", rotation);
            success = xTaskCreate(
                          [](void*) {
                              vTaskDelay(pdMS_TO_TICKS(500));
                              Application::GetInstance().Schedule(
                                  []() { Application::GetInstance().Reboot(); });
                              vTaskDelete(nullptr);
                          },
                          "display_restart", 3072, nullptr, 2, nullptr) == pdPASS;
        }
        message = success ? "Đã lưu chiều màn hình; robot đang khởi động lại."
                          : "Invalid rotation or restart failed";
    } else if (std::strcmp(command, "sequence") == 0) {
        const char* program = JsonString(root, "program");
        const auto* dry = cJSON_GetObjectItemCaseSensitive(root, "dry_run");
        success = program && (!dry || cJSON_IsBool(dry)) &&
                  controller_.RunSequence(program, motion_error, DroidRobotController::Source::Web,
                                          cJSON_IsTrue(dry));
        message = success ? (cJSON_IsTrue(dry) ? "Chuỗi hợp lệ; chưa chạy." : "Đã bắt đầu chuỗi.")
                          : (motion_error.empty() ? "Invalid program" : motion_error.c_str());
    } else if (std::strcmp(command, "wave_arms") == 0) {
        const char* arms = JsonString(root, "arms");
        const char* pattern = JsonString(root, "pattern");
        int cycles = 0, amplitude = 0;
        success = arms && JsonInt(root, "cycles", cycles) &&
                  JsonInt(root, "amplitude", amplitude) &&
                  controller_.WaveArms(arms, cycles, amplitude, pattern ? pattern : "mirror",
                                       motion_error, DroidRobotController::Source::Web);
        message = success ? "Đã bắt đầu vẫy tay."
                          : (motion_error.empty() ? "Invalid wave" : motion_error.c_str());
    } else if (std::strcmp(command, "spin_turns") == 0) {
        const char* direction = JsonString(root, "direction");
        int turns = 0;
        success = direction && JsonInt(root, "turns", turns) &&
                  controller_.SpinTurns(direction, turns, motion_error,
                                        DroidRobotController::Source::Web);
        message = success ? "Đã bắt đầu xoay theo thời gian; số vòng chỉ ước lượng."
                          : (motion_error.empty() ? "Invalid spin" : motion_error.c_str());
    } else if (std::strcmp(command, "spin_calibration") == 0) {
        int left = 0, right = 0;
        success = JsonInt(root, "left_ms", left) && JsonInt(root, "right_ms", right) &&
                  controller_.SaveSpinCalibration(left, right);
        message =
            success ? "Đã lưu thời gian một vòng ở tốc độ 40%." : "Mỗi chiều cần 1000-10000 ms.";
    } else if (std::strcmp(command, "home") == 0) {
        controller_.Home(DroidRobotController::Source::Web);
        success = true;
        message = "Moving to saved home pose";
    } else if (std::strcmp(command, "drive") == 0) {
        const char* direction = JsonString(root, "direction");
        int speed = 0;
        success = direction != nullptr && JsonInt(root, "speed", speed) && speed >= 0 &&
                  speed <= 100 &&
                  controller_.Drive(direction, speed, 700, DroidRobotController::Source::Web);
        message = success ? "" : "Invalid drive command";
    } else if (std::strcmp(command, "gesture") == 0) {
        const char* name = JsonString(root, "name");
        success =
            name != nullptr && controller_.StartGesture(name, DroidRobotController::Source::Web);
        message = success ? "Gesture started" : "Unknown gesture";
    } else if (std::strcmp(command, "joint") == 0) {
        const char* name = JsonString(root, "joint");
        int angle = 0;
        int speed = 100;
        const cJSON* speed_item = cJSON_GetObjectItemCaseSensitive(root, "speed");
        if (cJSON_IsNumber(speed_item)) {
            speed = speed_item->valueint;
        }
        DroidRobotController::Joint joint;
        success = name != nullptr && JsonInt(root, "angle", angle) &&
                  DroidRobotController::ParseJoint(name, joint) &&
                  controller_.SetJoint(joint, angle, speed, DroidRobotController::Source::Web);
        message = success ? "Joint target updated" : "Invalid joint command";
    } else if (std::strcmp(command, "save_home") == 0) {
        const char* name = JsonString(root, "joint");
        int angle = 0;
        DroidRobotController::Joint joint;
        success = name != nullptr && JsonInt(root, "angle", angle) &&
                  DroidRobotController::ParseJoint(name, joint) &&
                  controller_.SaveHome(joint, angle);
        message = success ? "Home angle saved" : "Stop the robot before saving home";
    } else if (std::strcmp(command, "head_span") == 0) {
        int span = 0;
        success = JsonInt(root, "span_us", span) && controller_.SaveHeadSpanUs(span);
        message = success ? "Đã lưu biên độ xung đầu. Thử góc nhỏ trước."
                          : "Chỉ chỉnh 400-700 µs (bước 25) khi đầu ở 90°, robot đứng yên.";
    } else if (std::strcmp(command, "neutral") == 0) {
        const char* side = JsonString(root, "side");
        int pulse = 0;
        success = side != nullptr && JsonInt(root, "pulse", pulse) &&
                  (std::strcmp(side, "left") == 0 || std::strcmp(side, "right") == 0) &&
                  controller_.SetTrackNeutral(std::strcmp(side, "left") == 0, pulse);
        message = success ? "Track neutral saved" : "Neutral must be 1400-1600 us";
    } else if (std::strcmp(command, "flip") == 0) {
        const char* side = JsonString(root, "side");
        success = side != nullptr &&
                  (std::strcmp(side, "left") == 0 || std::strcmp(side, "right") == 0) &&
                  controller_.FlipTrack(std::strcmp(side, "left") == 0);
        message = success ? "Track direction saved" : "Invalid track side";
    } else if (std::strcmp(command, "track_test") == 0) {
        const char* side = JsonString(root, "side");
        int direction = 0;
        if (side != nullptr && JsonInt(root, "direction", direction) &&
            (direction == -1 || direction == 1)) {
            const int speed = direction * 25;
            if (std::strcmp(side, "left") == 0) {
                success = controller_.SetTracks(speed, 0, 250, DroidRobotController::Source::Web);
            } else if (std::strcmp(side, "right") == 0) {
                success = controller_.SetTracks(0, speed, 250, DroidRobotController::Source::Web);
            }
        }
        message = success ? "Short track test started" : "Invalid track test";
    } else if (std::strcmp(command, "reset_calibration") == 0) {
        controller_.ResetCalibration();
        success = true;
        message = "Calibration reset to documented defaults";
    } else {
        message = "Unknown command";
    }

    cJSON_Delete(root);
    return SendCommandResult(req, success, message);
}

std::string DroidControlServer::BuildStatusJson() const {
    cJSON* root = cJSON_Parse(controller_.GetStateJson().c_str());
    if (root == nullptr) {
        root = cJSON_CreateObject();
    }

    auto& wifi = WifiManager::GetInstance();
    cJSON* network = cJSON_CreateObject();
    const bool connected = wifi.IsConnected();
    cJSON_AddBoolToObject(network, "connected", connected);
    cJSON_AddBoolToObject(network, "configMode", wifi.IsConfigMode());
    cJSON_AddStringToObject(network, "ssid", wifi.GetSsid().c_str());
    cJSON_AddStringToObject(network, "ip", wifi.GetIpAddress().c_str());
    cJSON_AddNumberToObject(network, "rssi", wifi.GetRssi());
    cJSON_AddStringToObject(network, "controlApSsid", CONTROL_AP_SSID);
    cJSON_AddStringToObject(network, "controlApIp", CONTROL_AP_IP);
    cJSON_AddItemToObject(root, "network", network);

    cJSON* audio = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio, "volume", audio_codec_.output_volume());
    cJSON_AddNumberToObject(audio, "micGain", audio_codec_.mic_gain());
    cJSON_AddNumberToObject(audio, "inputRate", audio_codec_.input_sample_rate());
    cJSON_AddNumberToObject(audio, "outputRate", audio_codec_.output_sample_rate());
    cJSON_AddItemToObject(root, "audio", audio);

    cJSON* touch = cJSON_CreateObject();
    cJSON_AddNumberToObject(touch, "gpio", 21);
    cJSON_AddNumberToObject(touch, "wifiHoldSeconds", 5);
    cJSON_AddStringToObject(touch, "singleTap", "toggleChat");
    cJSON_AddItemToObject(root, "touch", touch);
    cJSON_AddStringToObject(root, "wakeWord", "Hi Wall-E");
    cJSON_AddNumberToObject(root, "controlPort", CONTROL_SERVER_PORT);
    cJSON_AddStringToObject(root, "firmwareRevision", "droid-motion-face-r6");
    cJSON_AddStringToObject(root, "buildTime", esp_app_get_description()->time);
    Settings display_settings("droid-display", false);
    cJSON_AddNumberToObject(root, "displayRotation", display_settings.GetInt("rotation", 2));

    char* encoded = cJSON_PrintUnformatted(root);
    std::string result = encoded != nullptr ? encoded : "{}";
    cJSON_free(encoded);
    cJSON_Delete(root);
    return result;
}

bool DroidControlServer::ReadRequestBody(httpd_req_t* req, std::string& body) {
    constexpr size_t kMaxBodyLength = 8192;
    if (req->content_len <= 0 || req->content_len > kMaxBodyLength) {
        return false;
    }
    body.resize(req->content_len);
    size_t received = 0;
    while (received < body.size()) {
        const int result = httpd_req_recv(req, body.data() + received, body.size() - received);
        if (result <= 0) {
            return false;
        }
        received += static_cast<size_t>(result);
    }
    return true;
}

esp_err_t DroidControlServer::SendJson(httpd_req_t* req, const std::string& json, int status_code) {
    if (status_code == 400) {
        httpd_resp_set_status(req, "400 Bad Request");
    } else if (status_code == 503) {
        httpd_resp_set_status(req, "503 Service Unavailable");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t DroidControlServer::SendCommandResult(httpd_req_t* req, bool success,
                                                const char* message) const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", success);
    if (success) {
        cJSON_AddStringToObject(root, "message", message != nullptr ? message : "");
        cJSON* state = cJSON_Parse(BuildStatusJson().c_str());
        if (state != nullptr) {
            cJSON_AddItemToObject(root, "state", state);
        }
    } else {
        cJSON_AddStringToObject(root, "error", message != nullptr ? message : "Command rejected");
    }
    char* encoded = cJSON_PrintUnformatted(root);
    const std::string json = encoded != nullptr ? encoded : R"({"success":false})";
    cJSON_free(encoded);
    cJSON_Delete(root);
    return SendJson(req, json, success ? 200 : 400);
}
