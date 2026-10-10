#pragma once

#include "audio_engine/IAudioSink.hpp"
#include <aaudio/AAudio.h>
#include <atomic>
#include <string>

namespace audio_engine {

/**
 * @brief 基于 Android AAudio 原生 C API 的高保真低延迟硬件输出驱动
 * 支持 32-bit Float 直通、独占流 (AAUDIO_SHARING_MODE_EXCLUSIVE) 与低延迟性能模式
 */
class AAudioSink : public IAudioSink {
public:
    AAudioSink();
    ~AAudioSink() override;

    bool open(const AudioFormatSpec& requested_spec, AudioCallback callback) override;
    void start() override;
    void pause() override;
    void stop() override;
    void close() override;

    double getHardwareLatencySec() const override;
    bool isOpen() const override;
    AudioFormatSpec getActualSpec() const override;
    void setBufferSize(uint32_t samples) override;
    uint32_t getBufferSize() const override { return buffer_size_samples_; }
    std::string getDeviceName() const override { return active_device_name_; }

private:
    AAudioStream* stream_ = nullptr;
    std::string active_device_name_ = "Android AAudio Direct";
    AudioCallback callback_;
    AudioFormatSpec actual_spec_;
    std::atomic<bool> is_running_{false};
    uint32_t buffer_size_samples_ = 512;

    static aaudio_data_callback_result_t dataCallbackThunk(
        AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);

    static void errorCallbackThunk(
        AAudioStream* stream, void* userData, aaudio_result_t error);
};

} // namespace audio_engine
