#ifndef _CUSTOM_S3_VOICE_CONFIG_H_
#define _CUSTOM_S3_VOICE_CONFIG_H_

#include <driver/gpio.h>

// ================= AUDIO =================

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_METHOD_SIMPLEX

// MAX98357A
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_12
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_11
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_13

// INMP441
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_16
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_17
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_18
// User-selected mic gain; gain 12 clipped on the physical board.
#define AUDIO_MIC_GAIN_DEFAULT 9

// ================= TOUCH =================

#define TOUCH_BUTTON_GPIO GPIO_NUM_21
#define TOUCH_WIFI_CONFIG_HOLD_MS 5000

// ================= CONTROL ACCESS POINT =================

#define CONTROL_AP_SSID "DroidE3D-Control"
#define CONTROL_AP_IP "192.168.4.1"
#define CONTROL_SERVER_PORT 8080

// ================= ROBOT =================

#define LEFT_TRACK_GPIO GPIO_NUM_4
#define RIGHT_TRACK_GPIO GPIO_NUM_5
#define HEAD_SERVO_GPIO GPIO_NUM_6
#define LEFT_ARM_SERVO_GPIO GPIO_NUM_7
#define RIGHT_ARM_SERVO_GPIO GPIO_NUM_15

// ================= ST7789 =================

#define DISPLAY_SPI_SCK_PIN GPIO_NUM_38
#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_39
#define DISPLAY_SPI_CS_PIN GPIO_NUM_42
#define DISPLAY_DC_PIN GPIO_NUM_41
#define DISPLAY_RST_PIN GPIO_NUM_40

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_INVERT_COLOR true
#define DISPLAY_SPI_MODE 3

// BL is wired directly to 3V3.
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_NC

#endif
