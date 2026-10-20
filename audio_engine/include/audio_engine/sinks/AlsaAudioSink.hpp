#pragma once

#include "audio_engine/IAudioSink.hpp"
#include <string>
#include <atomic>
#include <thread>

#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
#include <alsa/asoundlib.h>
#endif

namespace audio_engine {

/**
 * @brief 基于 ALSA Direct (hw:X,Y) + MMAP 的树莓派 5 专属发烧硬件直出驱动
 * 完全跳过 Linux PulseAudio / PipeWire 等混音服务，物理级 Bit-Perfect 直通 DAC (如 AK4499EX)
 */
class AlsaAudioSink : public IAudioSink {
public:
    explicit AlsaAudioSink(std::string device_name = "hw:0,0");
    ~AlsaAudioSink() override;

    bool open(const AudioFormatSpec& requested_spec, AudioCallback callback) override;
    void start() override;
    void pause() override;
    void stop() override;
    void close() override;

    double getHardwareLatencySec() const override;
    bool isOpen() const override;
    AudioFormatSpec getActualSpec() const override;
    void setBufferSize(uint32_t samples) override { buffer_size_samples_ = samples; }
    uint32_t getBufferSize() const override { return buffer_size_samples_; }

private:
    std::string device_name_ = "hw:0,0";
    uint32_t buffer_size_samples_ = 1024;
    AudioCallback callback_;
    AudioFormatSpec actual_spec_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_paused_{false};
    double buffer_latency_sec_ = 0.0;

#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    snd_pcm_t* pcm_handle_ = nullptr;
    std::jthread alsa_thread_;
    void alsaLoop(std::stop_token stop_token);
#endif
};

} // namespace audio_engine
