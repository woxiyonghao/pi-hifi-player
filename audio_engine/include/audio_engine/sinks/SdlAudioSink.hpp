#pragma once

#include "audio_engine/IAudioSink.hpp"
#include <SDL2/SDL.h>
#include <atomic>

namespace audio_engine {

/**
 * @brief 基于 SDL2 Audio Callback 的低延迟跨平台硬件输出驱动
 * 适用 macOS 开发调试环境及通用 Linux 桌面，支持任意采样率与 32-bit Float 硬件推流
 */
class SdlAudioSink : public IAudioSink {
public:
    SdlAudioSink();
    ~SdlAudioSink() override;

    bool open(const AudioFormatSpec& requested_spec, AudioCallback callback) override;
    void start() override;
    void pause() override;
    void stop() override;
    void close() override;

    double getHardwareLatencySec() const override;
    bool isOpen() const override;
    AudioFormatSpec getActualSpec() const override;

private:
    SDL_AudioDeviceID device_id_ = 0;
    AudioCallback callback_;
    AudioFormatSpec actual_spec_;
    std::atomic<bool> is_running_{false};
    double buffer_latency_sec_ = 0.0;

    static void sdlCallbackThunk(void* userdata, Uint8* stream, int len);
};

} // namespace audio_engine
