#include "application.h"
#include "button.h"
#include "config.h"
#include "droid_audio_codec.h"
#include "droid_control_ap.h"
#include "droid_control_server.h"
#include "droid_robot_controller.h"
#include "robot_face_ui/robot_face_ui.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#include <memory>

#define TAG "CustomS3Voice"

class CustomS3Voice : public WifiBoard {
private:
    Button touch_button_;

    Display* display_ = nullptr;
    std::unique_ptr<DroidAudioCodec> audio_codec_;
    std::unique_ptr<DroidRobotController> robot_controller_;
    std::unique_ptr<DroidControlAccessPoint> control_ap_;
    std::unique_ptr<DroidControlServer> control_server_;

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};

        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;

        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);

        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        ESP_LOGI(TAG, "Initialize ST7789");
        Settings settings("droid-display", false);
        int rotation = settings.GetInt("rotation", 2);
        if (rotation < 0 || rotation > 3)
            rotation = 2;
        // ST7789 240x240 rotation table, including its hidden 80 rows.
        const bool mirror_x = rotation == 0 || rotation == 3;
        const bool mirror_y = rotation == 0 || rotation == 1;
        const bool swap_xy = rotation == 1 || rotation == 3;
        const int gap_x = rotation == 1 ? 80 : 0;
        const int gap_y = rotation == 0 ? 80 : 0;

        esp_lcd_panel_io_spi_config_t io_config = {};

        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io_));

        esp_lcd_panel_dev_config_t panel_config = {};

        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));

        uint8_t vcom_data[] = {0x38};
        esp_lcd_panel_io_tx_param(panel_io_, 0xBB, vcom_data, sizeof(vcom_data));

        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));

        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, swap_xy));

        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, mirror_x, mirror_y));

        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_, gap_x, gap_y));
        ESP_LOGI(TAG, "Display 240x240 rotation=%d gap=(%d,%d), LVGL offset=(0,0)", rotation, gap_x,
                 gap_y);

        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new RobotFaceDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0,
                                        mirror_x, mirror_y, swap_xy);
    }

    void InitializeTouchButton() {
        touch_button_.OnClick([]() { Application::GetInstance().ToggleChatState(); });

        touch_button_.OnLongPress([this]() {
            robot_controller_->Stop(DroidRobotController::Source::Touch);
            Application::GetInstance().Schedule([this]() { PrepareWifiReconfiguration(); });
        });

        touch_button_.OnDoubleClick(
            [this]() { robot_controller_->Stop(DroidRobotController::Source::Touch); });

        touch_button_.OnMultipleClick(
            [this]() { robot_controller_->Home(DroidRobotController::Source::Touch); }, 3);
    }

    void InitializeAudio() {
        audio_codec_ = std::make_unique<DroidAudioCodec>(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, AUDIO_MIC_GAIN_DEFAULT);
    }

    void InitializeRobot() {
        robot_controller_ = std::make_unique<DroidRobotController>();
        if (!robot_controller_->Initialize()) {
            ESP_LOGE(TAG, "Robot controller initialization failed");
        }
        static_cast<RobotFaceDisplay*>(display_)->SetMotionProvider(
            [this]() { return robot_controller_->IsMoving(); });
    }

    void InitializeControlServer() {
        control_ap_ = std::make_unique<DroidControlAccessPoint>();
        control_server_ = std::make_unique<DroidControlServer>(
            *robot_controller_, *audio_codec_, [this]() { PrepareWifiReconfiguration(); });
    }

    void PrepareWifiReconfiguration() {
        control_ap_->PrepareForWifiProvisioning();
        WifiBoard::EnterWifiConfigMode();
    }

    void StartNetwork() override {
        WifiBoard::StartNetwork();
        if (!control_server_->Start(CONTROL_SERVER_PORT)) {
            ESP_LOGE(TAG, "Control web server failed to start");
        }
        if (!control_ap_->StartMonitor()) {
            ESP_LOGE(TAG, "Control AP monitor failed to start");
        }
    }

public:
    CustomS3Voice() : touch_button_(TOUCH_BUTTON_GPIO, true, TOUCH_WIFI_CONFIG_HOLD_MS) {
        InitializeSpi();
        InitializeDisplay();
        InitializeAudio();
        InitializeRobot();
        InitializeControlServer();
        InitializeTouchButton();
    }

    AudioCodec* GetAudioCodec() override { return audio_codec_.get(); }

    Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(CustomS3Voice);
