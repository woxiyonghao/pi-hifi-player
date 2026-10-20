#pragma once

#include <cstdint>
#include <string>

namespace audio_engine {

enum class SampleFormat {
    Int16,      // 16-bit 有符号整数 PCM
    Int24,      // 24-bit 有符号整数 PCM (存储于 int32 低 24 位)
    Int32,      // 32-bit 有符号整数 PCM
    Float32,    // 32-bit 单精度浮点 (标准化范围 -1.0f ~ 1.0f)
    DsdDop16    // DoP v1.1 (16-bit DSD packed with 8-bit DSD marker into 24/32bit PCM)
};

struct AudioFormatSpec {
    uint32_t sample_rate = 44100;
    uint32_t channels = 2;
    uint8_t bit_depth = 16;
    SampleFormat format = SampleFormat::Float32;

    uint32_t bytesPerSample() const {
        switch (format) {
            case SampleFormat::Int16:    return 2;
            case SampleFormat::Int24:    return 4; // 内存对齐为 32-bit
            case SampleFormat::Int32:    return 4;
            case SampleFormat::Float32:  return 4;
            case SampleFormat::DsdDop16: return 4;
        }
        return 4;
    }

    uint32_t bytesPerFrame() const {
        return channels * bytesPerSample();
    }

    bool isDsd() const {
        return format == SampleFormat::DsdDop16;
    }
};

} // namespace audio_engine
