#define DR_FLAC_IMPLEMENTATION
#include "dr_libs/dr_flac.h"

#include "audio_engine/decoders/FlacDecoder.hpp"
#include <iostream>
#include <algorithm>

namespace audio_engine {

FlacDecoder::FlacDecoder() = default;

FlacDecoder::~FlacDecoder() {
    close();
}

bool FlacDecoder::open(const std::string& filepath) {
    close();

    flac_handle_ = drflac_open_file(filepath.c_str(), nullptr);
    if (!flac_handle_) {
        std::cerr << "[FlacDecoder] 打开 FLAC 文件失败: " << filepath << std::endl;
        return false;
    }

    spec_.sample_rate = flac_handle_->sampleRate;
    spec_.channels = flac_handle_->channels;
    spec_.bit_depth = static_cast<uint8_t>(flac_handle_->bitsPerSample);
    spec_.format = SampleFormat::Float32;

    total_frames_ = flac_handle_->totalPCMFrameCount;
    duration_sec_ = (spec_.sample_rate > 0) ? (static_cast<double>(total_frames_) / spec_.sample_rate) : 0.0;

    return true;
}

uint64_t FlacDecoder::readFrames(float* buffer, uint64_t max_frames) {
    if (!flac_handle_ || !buffer || max_frames == 0) return 0;
    return drflac_read_pcm_frames_f32(flac_handle_, max_frames, buffer);
}

bool FlacDecoder::seek(double target_seconds) {
    if (!flac_handle_ || spec_.sample_rate == 0) return false;
    double clamped_sec = std::clamp(target_seconds, 0.0, duration_sec_);
    uint64_t target_frame = static_cast<uint64_t>(clamped_sec * spec_.sample_rate);
    return drflac_seek_to_pcm_frame(flac_handle_, target_frame) == DRFLAC_TRUE;
}

double FlacDecoder::getDuration() const {
    return duration_sec_;
}

AudioFormatSpec FlacDecoder::getSpec() const {
    return spec_;
}

void FlacDecoder::close() {
    if (flac_handle_) {
        drflac_close(flac_handle_);
        flac_handle_ = nullptr;
    }
    duration_sec_ = 0.0;
    total_frames_ = 0;
}

} // namespace audio_engine
