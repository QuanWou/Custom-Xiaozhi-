#include "droid_audio_codec.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>
#include <cmath>

DroidAudioCodec::DroidAudioCodec(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk,
                                 gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck,
                                 gpio_num_t mic_ws, gpio_num_t mic_din, int default_mic_gain)
    : mic_gain_(std::clamp(default_mic_gain, 1, 16)) {
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_gain_ = mic_gain_;

    // XIAOzITEST: I2S0 RX, Philips 32-bit stereo, INMP441 L/R tied to GND.
    i2s_chan_config_t rx = I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(0), I2S_ROLE_MASTER);
    rx.dma_frame_num = kBlockFrames;
    // Continuous capture shares CPU time with AFE, Wi-Fi and the display.
    rx.dma_desc_num = 12;
    ESP_ERROR_CHECK(i2s_new_channel(&rx, nullptr, &rx_handle_));
    i2s_std_config_t mic = {};
    mic.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(input_sample_rate));
    mic.slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    mic.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    mic.gpio_cfg.bclk = mic_sck;
    mic.gpio_cfg.ws = mic_ws;
    mic.gpio_cfg.dout = I2S_GPIO_UNUSED;
    mic.gpio_cfg.din = mic_din;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &mic));
    i2s_event_callbacks_t callbacks = {};
    callbacks.on_recv_q_ovf = [](i2s_chan_handle_t, i2s_event_data_t*, void* context) {
        auto* codec = static_cast<DroidAudioCodec*>(context);
        codec->input_overruns_.fetch_add(1, std::memory_order_relaxed);
        return false;
    };
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle_, &callbacks, this));

    // XIAOzITEST: I2S1 TX, 16-bit stereo, duplicate mono into both slots.
    i2s_chan_config_t tx = I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(1), I2S_ROLE_MASTER);
    // 120 ms of TX storage provides margin for decoded network audio.
    tx.dma_frame_num = kTxDmaFrames;
    tx.dma_desc_num = kTxDmaDescriptors;
    tx.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&tx, &tx_handle_, nullptr));
    i2s_std_config_t speaker = {};
    speaker.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(output_sample_rate));
    speaker.slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    speaker.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    speaker.gpio_cfg.bclk = spk_bclk;
    speaker.gpio_cfg.ws = spk_ws;
    speaker.gpio_cfg.dout = spk_dout;
    speaker.gpio_cfg.din = I2S_GPIO_UNUSED;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &speaker));
    ESP_LOGI(
        "DroidAudio",
        "XIAOzITEST: mic I2S0 32-bit stereo left %d Hz gain %d; speaker I2S1 16-bit stereo %d Hz",
        input_sample_rate_, mic_gain_, output_sample_rate_);
}

DroidAudioCodec::~DroidAudioCodec() {
    EnableInput(false);
    EnableOutput(false);
    if (rx_handle_)
        i2s_del_channel(rx_handle_);
    if (tx_handle_)
        i2s_del_channel(tx_handle_);
}

void DroidAudioCodec::SetInputGain(float gain) {
    (void)gain;
    AudioCodec::SetInputGain(mic_gain_);
}

void DroidAudioCodec::SetOutputVolume(int volume) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    AudioCodec::SetOutputVolume(std::clamp(volume, 0, 100));
}

void DroidAudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(input_mutex_);
    if (enable == input_enabled_)
        return;
    ESP_ERROR_CHECK(enable ? i2s_channel_enable(rx_handle_) : i2s_channel_disable(rx_handle_));
    if (enable) {
        input_samples_ = input_clipped_ = 0;
        input_peak_ = 0;
        input_square_sum_ = 0;
        input_overruns_.store(0, std::memory_order_relaxed);
        input_report_time_ = esp_timer_get_time();
    }
    AudioCodec::EnableInput(enable);
}

void DroidAudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (enable == output_enabled_)
        return;
    if (enable) {
        // Clear every DMA slot before restarting, so an old tail cannot replay.
        const std::array<int16_t, kTxDmaFrames * 2> silence{};
        for (size_t i = 0; i < kTxDmaDescriptors; ++i) {
            size_t loaded = 0;
            ESP_ERROR_CHECK(
                i2s_channel_preload_data(tx_handle_, silence.data(), sizeof(silence), &loaded));
        }
    }
    ESP_ERROR_CHECK(enable ? i2s_channel_enable(tx_handle_) : i2s_channel_disable(tx_handle_));
    AudioCodec::EnableOutput(enable);
}

bool DroidAudioCodec::InputData(std::vector<int16_t>& data) {
    return !data.empty() && Read(data.data(), data.size()) == static_cast<int>(data.size());
}

int DroidAudioCodec::Read(int16_t* dest, int samples) {
    std::lock_guard<std::mutex> lock(input_mutex_);
    if (!input_enabled_ || dest == nullptr || samples <= 0)
        return 0;
    int total = 0;
    while (total < samples) {
        const int frames = std::min<int>(samples - total, kBlockFrames);
        size_t bytes = 0;
        const esp_err_t result = i2s_channel_read(rx_handle_, input_buffer_.data(),
                                                  frames * 2 * sizeof(int32_t), &bytes, 200);
        const int received = bytes / (2 * sizeof(int32_t));
        for (int i = 0; i < received; ++i) {
            const int32_t value = (input_buffer_[i * 2] >> 16) * mic_gain_;
            dest[total + i] = std::clamp<int32_t>(value, -30000, 30000);
            input_peak_ = std::max(input_peak_, std::abs(value));
            input_clipped_ += value > 30000 || value < -30000;
            const int64_t sample = dest[total + i];
            input_square_sum_ += sample * sample;
        }
        total += received;
        input_samples_ += received;
        if (result != ESP_OK || received == 0)
            break;
    }
    const int64_t now = esp_timer_get_time();
    if (input_samples_ > 0 && now - input_report_time_ >= 5000000) {
        const int rms = std::sqrt(static_cast<double>(input_square_sum_) / input_samples_);
        const uint32_t overruns = input_overruns_.exchange(0, std::memory_order_relaxed);
        ESP_LOGI("DroidAudio", "Mic gain=%d peak=%ld rms=%d clipped=%lu/%lu overruns=%lu",
                 mic_gain_, static_cast<long>(input_peak_), rms,
                 static_cast<unsigned long>(input_clipped_),
                 static_cast<unsigned long>(input_samples_), static_cast<unsigned long>(overruns));
        input_samples_ = input_clipped_ = 0;
        input_peak_ = 0;
        input_square_sum_ = 0;
        input_report_time_ = now;
    }
    return total;
}

int DroidAudioCodec::Write(const int16_t* data, int samples) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (!output_enabled_ || data == nullptr || samples <= 0)
        return 0;
    // Linear amplitude: 70% now retains 70%, rather than squaring it to 49%.
    const int volume = std::clamp(output_volume_, 0, 100);
    int total = 0;
    while (total < samples) {
        const int frames = std::min<int>(samples - total, kBlockFrames);
        for (int i = 0; i < frames; ++i) {
            const int16_t value = static_cast<int32_t>(data[total + i]) * volume / 100;
            output_buffer_[i * 2] = value;
            output_buffer_[i * 2 + 1] = value;
        }
        size_t bytes = 0;
        const esp_err_t result = i2s_channel_write(tx_handle_, output_buffer_.data(),
                                                   frames * 2 * sizeof(int16_t), &bytes, 200);
        const int written = bytes / (2 * sizeof(int16_t));
        total += written;
        if (result != ESP_OK || written == 0) {
            ESP_LOGW("DroidAudio", "Speaker write interrupted: %s, %d/%d samples",
                     esp_err_to_name(result), total, samples);
            break;
        }
    }
    return total;
}
