#pragma once

#include "audio_engine/IAudioSink.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <atomic>

namespace audio_engine {

/**
 * @brief Apple 原生低延迟硬件推流驱动 (基于 AudioToolbox AudioQueue)
 * 专为 iOS / iPadOS 与 Apple 平台设计，零第三方依赖，直通 CoreAudio 硬件管道
 */
class AudioQueueSink : public IAudioSink {
public:
    AudioQueueSink();
    ~AudioQueueSink() override;

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
    static void outputCallbackThunk(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer);

    AudioQueueRef audio_queue_{nullptr};
    static constexpr int kNumBuffers = 4;
    AudioQueueBufferRef buffers_[kNumBuffers]{nullptr};

    AudioFormatSpec actual_spec_{};
    AudioCallback callback_;
    std::atomic<bool> is_running_{false};
    bool is_stopped_{true};
    uint32_t buffer_size_samples_{2048};
    double buffer_latency_sec_{0.0};
};

} // namespace audio_engine
