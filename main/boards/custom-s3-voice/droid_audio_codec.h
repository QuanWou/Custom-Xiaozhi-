#pragma once

#include "audio_codec.h"

#include <array>
#include <atomic>
#include <mutex>

class DroidAudioCodec : public AudioCodec {
public:
    DroidAudioCodec(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk,
                    gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws,
                    gpio_num_t mic_din, int default_mic_gain);

    ~DroidAudioCodec() override;
    void SetInputGain(float gain) override;
    void SetOutputVolume(int volume) override;
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    bool InputData(std::vector<int16_t>& data) override;
    int mic_gain() const { return mic_gain_; }

protected:
    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;

private:
    const int mic_gain_;
    std::mutex input_mutex_;
    std::mutex output_mutex_;
    static constexpr size_t kBlockFrames = 128;
    static constexpr size_t kTxDmaFrames = 240;
    static constexpr size_t kTxDmaDescriptors = 8;
    std::atomic<uint32_t> input_overruns_{0};
    uint32_t input_samples_ = 0;
    uint32_t input_clipped_ = 0;
    int32_t input_peak_ = 0;
    uint64_t input_square_sum_ = 0;
    int64_t input_report_time_ = 0;
    std::array<int32_t, kBlockFrames * 2> input_buffer_{};
    std::array<int16_t, kBlockFrames * 2> output_buffer_{};
};
