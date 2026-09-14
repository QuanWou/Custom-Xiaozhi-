#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#define TAG "CustomS3Voice"

class CustomS3Voice : public WifiBoard {
private:
    Button boot_button_;
    Button volume_down_button_;
    Button volume_up_button_;

    Display* display_ = nullptr;

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

        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));

        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));

        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));

        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y));

        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();

            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }

            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();

            int volume = codec->output_volume() + 10;

            if (volume > 100) {
                volume = 100;
            }

            codec->SetOutputVolume(volume);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();

            int volume = codec->output_volume() - 10;

            if (volume < 0) {
                volume = 0;
            }

            codec->SetOutputVolume(volume);
        });
    }

public:
    CustomS3Voice()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO) {
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,

                                               // MAX98357A
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
                                               AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_BOTH,

                                               // INMP441
                                               AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
                                               AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT);

        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(CustomS3Voice);
