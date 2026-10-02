#pragma once

#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class DroidControlAccessPoint {
public:
    DroidControlAccessPoint();
    ~DroidControlAccessPoint();

    bool StartMonitor();
    void PrepareForWifiProvisioning();
    bool IsStarted() const;

private:
    static void TaskEntry(void* arg);
    void Run();
    bool StartLocked();
    void StopLocked();

    mutable SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    esp_netif_t* ap_netif_ = nullptr;
    bool started_ = false;
    bool suspended_ = false;
    bool saw_config_mode_ = false;
    TickType_t suspended_at_ = 0;
};
