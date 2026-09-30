#pragma once

#include "audio_engine/IAudioDecoder.hpp"
#include "dr_libs/dr_wav.h"
#include <string>

namespace audio_engine {

/**
 * @brief 基于 dr_wav 的发烧级 WAV 原生解码器
 * 支持 16/24/32-bit PCM 及 Float32 WAV，最高 384kHz 采样率，源码零拷贝直出
 */
class WavDecoder : public IAudioDecoder {
public:
    WavDecoder();
    ~WavDecoder() override;

    bool open(const std::string& filepath) override;
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double target_seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override;
    void close() override;

private:
    drwav* wav_handle_ = nullptr;
    AudioFormatSpec spec_;
    double duration_sec_ = 0.0;
    uint64_t total_frames_ = 0;
};

} // namespace audio_engine
