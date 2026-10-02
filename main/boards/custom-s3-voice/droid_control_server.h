#pragma once

#include <esp_http_server.h>

#include <functional>
#include <string>

class DroidAudioCodec;
class DroidRobotController;

class DroidControlServer {
public:
    DroidControlServer(DroidRobotController& controller, DroidAudioCodec& audio_codec,
                       std::function<void()> reconfigure_wifi);
    ~DroidControlServer();

    bool Start(uint16_t port = 8080);
    void Stop();

private:
    static esp_err_t RootHandler(httpd_req_t* req);
    static esp_err_t ControlPageHandler(httpd_req_t* req);
    static esp_err_t WifiPageHandler(httpd_req_t* req);
    static esp_err_t StatusHandler(httpd_req_t* req);
    static esp_err_t RobotCommandHandler(httpd_req_t* req);
    static esp_err_t WifiReconfigureHandler(httpd_req_t* req);
    static void ReconfigureTask(void* arg);

    esp_err_t HandleRobotCommand(httpd_req_t* req);
    std::string BuildStatusJson() const;
    static bool ReadRequestBody(httpd_req_t* req, std::string& body);
    static esp_err_t SendJson(httpd_req_t* req, const std::string& json, int status_code = 200);
    esp_err_t SendCommandResult(httpd_req_t* req, bool success, const char* message) const;

    DroidRobotController& controller_;
    DroidAudioCodec& audio_codec_;
    std::function<void()> reconfigure_wifi_;
    httpd_handle_t server_ = nullptr;
    uint16_t port_ = 8080;

    static DroidControlServer* instance_;
};
