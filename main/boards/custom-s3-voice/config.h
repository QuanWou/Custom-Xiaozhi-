#ifndef _CUSTOM_S3_VOICE_CONFIG_H_
#define _CUSTOM_S3_VOICE_CONFIG_H_

#include <driver/gpio.h>

// ================= AUDIO =================

// INMP441: code test của bạn dùng 16 kHz
#define AUDIO_INPUT_SAMPLE_RATE 16000

// XiaoZhi thường phát ở 24 kHz
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_METHOD_SIMPLEX

// MAX98357A
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_16

// INMP441
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_6

// ================= BUTTON =================

#define BOOT_BUTTON_GPIO GPIO_NUM_8
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_9
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_10

// ================= ST7789 =================

#define DISPLAY_SPI_SCK_PIN GPIO_NUM_39
#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_40
#define DISPLAY_SPI_CS_PIN GPIO_NUM_1
#define DISPLAY_DC_PIN GPIO_NUM_42
#define DISPLAY_RST_PIN GPIO_NUM_41

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_INVERT_COLOR true
#define DISPLAY_SPI_MODE 0

// BL của bạn nối thẳng 3V3 nên không có GPIO backlight
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_NC

#endif
