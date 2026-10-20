#pragma once

#include "audio_engine/IAudioDecoder.hpp"
#include "dr_libs/dr_flac.h"
#include <string>

namespace audio_engine {

/**
 * @brief 基于 dr_flac 的发烧级 FLAC 原生解码器
 * 支持 16/24/32-bit FLAC，最高 192kHz/384kHz 采样率，无任何系统重采样
 */
class FlacDecoder : public IAudioDecoder {
public:
    FlacDecoder();
    ~FlacDecoder() override;

    bool open(const std::string& filepath) override;
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double target_seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override;
    void close() override;

private:
    drflac* flac_handle_ = nullptr;
    AudioFormatSpec spec_;
    double duration_sec_ = 0.0;
    uint64_t total_frames_ = 0;
};

} // namespace audio_engine
