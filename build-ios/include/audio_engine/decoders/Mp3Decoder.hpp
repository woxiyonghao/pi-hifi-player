#pragma once

#include "audio_engine/IAudioDecoder.hpp"
#include "dr_libs/dr_mp3.h"
#include <string>

namespace audio_engine {

/**
 * @brief 基于 dr_mp3 的轻量级 MP3 原生解码器
 * 浮点高保真解码输出，极低内存占用与 CPU 消耗
 */
class Mp3Decoder : public IAudioDecoder {
public:
    Mp3Decoder();
    ~Mp3Decoder() override;

    bool open(const std::string& filepath) override;
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double target_seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override;
    void close() override;

private:
    drmp3* mp3_handle_ = nullptr;
    AudioFormatSpec spec_;
    double duration_sec_ = 0.0;
    uint64_t total_frames_ = 0;
};

} // namespace audio_engine
