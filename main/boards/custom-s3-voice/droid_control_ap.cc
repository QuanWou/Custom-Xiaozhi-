#include "droid_control_ap.h"

#include "config.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <lwip/ip_addr.h>
#include <wifi_manager.h>

#include <cstring>

#define TAG "DroidControlAp"

namespace {

constexpr TickType_t kMonitorPeriod = pdMS_TO_TICKS(500);
constexpr TickType_t kProvisioningStartTimeout = pdMS_TO_TICKS(10000);

}  // namespace

DroidControlAccessPoint::DroidControlAccessPoint() { mutex_ = xSemaphoreCreateMutex(); }

DroidControlAccessPoint::~DroidControlAccessPoint() {
    if (task_ != nullptr) {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        StopLocked();
        xSemaphoreGive(mutex_);
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

bool DroidControlAccessPoint::StartMonitor() {
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create AP mutex");
        return false;
    }
    if (task_ != nullptr) {
        return true;
    }
    if (xTaskCreate(TaskEntry, "droid_control_ap", 4096, this, 2, &task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create AP monitor task");
        return false;
    }
    return true;
}

void DroidControlAccessPoint::PrepareForWifiProvisioning() {
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    suspended_ = true;
    saw_config_mode_ = false;
    suspended_at_ = xTaskGetTickCount();
    StopLocked();
    xSemaphoreGive(mutex_);
}

bool DroidControlAccessPoint::IsStarted() const {
    if (mutex_ == nullptr) {
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool started = started_;
    xSemaphoreGive(mutex_);
    return started;
}

void DroidControlAccessPoint::TaskEntry(void* arg) {
    static_cast<DroidControlAccessPoint*>(arg)->Run();
}

void DroidControlAccessPoint::Run() {
    auto& wifi = WifiManager::GetInstance();
    while (true) {
        const bool config_mode = wifi.IsConfigMode();
        const bool station_connected = wifi.IsConnected();

        xSemaphoreTake(mutex_, portMAX_DELAY);
        if (suspended_) {
            if (config_mode) {
                saw_config_mode_ = true;
            } else if (saw_config_mode_ && station_connected) {
                suspended_ = false;
                saw_config_mode_ = false;
            } else if (!saw_config_mode_ &&
                       static_cast<TickType_t>(xTaskGetTickCount() - suspended_at_) >=
                           kProvisioningStartTimeout) {
                ESP_LOGW(TAG, "Provisioning did not start; restoring control AP");
                suspended_ = false;
            }
        }

        if (!suspended_ && station_connected && !config_mode && !started_) {
            StartLocked();
        }
        xSemaphoreGive(mutex_);
        vTaskDelay(kMonitorPeriod);
    }
}

bool DroidControlAccessPoint::StartLocked() {
    if (started_) {
        return true;
    }

    ap_netif_ = esp_netif_create_default_wifi_ap();
    if (ap_netif_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create control AP network interface");
        return false;
    }

    esp_netif_ip_info_t ip_info = {};
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif_);
    esp_err_t error = esp_netif_set_ip_info(ap_netif_, &ip_info);
    if (error == ESP_OK) {
        error = esp_netif_dhcps_start(ap_netif_);
    }

    wifi_config_t config = {};
    std::strncpy(reinterpret_cast<char*>(config.ap.ssid), CONTROL_AP_SSID,
                 sizeof(config.ap.ssid) - 1);
    config.ap.ssid_len = std::strlen(CONTROL_AP_SSID);
    config.ap.channel = 1;
    config.ap.authmode = WIFI_AUTH_OPEN;
    config.ap.max_connection = 4;
    config.ap.beacon_interval = 100;

    if (error == ESP_OK) {
        error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    if (error == ESP_OK) {
        error = esp_wifi_set_config(WIFI_IF_AP, &config);
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start control AP: %s", esp_err_to_name(error));
        esp_netif_destroy_default_wifi(ap_netif_);
        ap_netif_ = nullptr;
        return false;
    }

    started_ = true;
    ESP_LOGI(TAG, "Control AP '%s' ready at http://%s:%u/control", CONTROL_AP_SSID, CONTROL_AP_IP,
             CONTROL_SERVER_PORT);
    return true;
}

void DroidControlAccessPoint::StopLocked() {
    if (!started_) {
        return;
    }

    const esp_err_t mode_error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (mode_error != ESP_OK) {
        ESP_LOGW(TAG, "Failed to switch back to STA mode: %s", esp_err_to_name(mode_error));
    }
    if (ap_netif_ != nullptr) {
        esp_netif_dhcps_stop(ap_netif_);
        esp_netif_destroy_default_wifi(ap_netif_);
        ap_netif_ = nullptr;
    }
    started_ = false;
    ESP_LOGI(TAG, "Control AP stopped for Wi-Fi provisioning");
}
