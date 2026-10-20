#pragma once

#if defined(__APPLE__)

#include "audio_engine/IAudioDecoder.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <string>

namespace audio_engine {

/**
 * @brief Apple 原生音频解码器 (基于 CoreAudio ExtAudioFile)
 * 支持 iOS / iPadOS / macOS 硬件加速解码 M4A (ALAC / AAC), AIFF, CAF, WAV, MP3, FLAC 等格式
 */
class AppleAudioDecoder : public IAudioDecoder {
public:
    explicit AppleAudioDecoder(uint32_t output_sample_rate = 0);
    ~AppleAudioDecoder() override;

    bool open(const std::string& filepath) override;
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double target_seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override;
    void close() override;

private:
    ExtAudioFileRef ext_file_{nullptr};
    AudioFormatSpec spec_{};
    uint64_t total_frames_{0};
    uint32_t requested_output_sample_rate_{0};
    double file_sample_rate_{0.0};
    double duration_sec_{0.0};
};

} // namespace audio_engine

#endif // __APPLE__
