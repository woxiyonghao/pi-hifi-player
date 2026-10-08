#pragma once

#include "audio_engine/IAudioSink.hpp"
#include <AudioUnit/AudioUnit.h>
#include <atomic>

namespace audio_engine {

// iPad AirPods 专用输出后端。RemoteIO 由系统音频实时线程直接拉取 PCM，
// 避免 AudioQueue 在 A2DP 路径上的额外队列调度与隐式格式转换。
class RemoteIOSink : public IAudioSink {
public:
    RemoteIOSink() = default;
    ~RemoteIOSink() override;

    bool open(const AudioFormatSpec& requested_spec, AudioCallback callback) override;
    void start() override;
    void pause() override;
    void stop() override;
    void close() override;

    double getHardwareLatencySec() const override;
    bool isOpen() const override;
    AudioFormatSpec getActualSpec() const override;

    void setBufferSize(uint32_t samples) override { preferred_buffer_frames_ = samples; }
    uint32_t getBufferSize() const override { return preferred_buffer_frames_; }

private:
    static OSStatus renderCallback(void* ref_con,
                                   AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* timestamp,
                                   UInt32 bus_number,
                                   UInt32 frame_count,
                                   AudioBufferList* io_data);

    AudioUnit audio_unit_{nullptr};
    AudioFormatSpec actual_spec_{};
    AudioCallback callback_;
    std::atomic<bool> is_running_{false};
    uint32_t preferred_buffer_frames_{256};
};

} // namespace audio_engine
