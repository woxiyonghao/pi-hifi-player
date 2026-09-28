#define DR_MP3_IMPLEMENTATION
#include "dr_libs/dr_mp3.h"

#include "audio_engine/decoders/Mp3Decoder.hpp"
#include <iostream>
#include <algorithm>

namespace audio_engine {

Mp3Decoder::Mp3Decoder() = default;

Mp3Decoder::~Mp3Decoder() {
    close();
}

bool Mp3Decoder::open(const std::string& filepath) {
    close();

    mp3_handle_ = new drmp3();
    if (!drmp3_init_file(mp3_handle_, filepath.c_str(), nullptr)) {
        std::cerr << "[Mp3Decoder] 打开 MP3 文件失败: " << filepath << std::endl;
        delete mp3_handle_;
        mp3_handle_ = nullptr;
        return false;
    }

    spec_.sample_rate = mp3_handle_->sampleRate;
    spec_.channels = mp3_handle_->channels;
    spec_.bit_depth = 16;
    spec_.format = SampleFormat::Float32;

    total_frames_ = drmp3_get_pcm_frame_count(mp3_handle_);
    duration_sec_ = (spec_.sample_rate > 0) ? (static_cast<double>(total_frames_) / spec_.sample_rate) : 0.0;

    return true;
}

uint64_t Mp3Decoder::readFrames(float* buffer, uint64_t max_frames) {
    if (!mp3_handle_ || !buffer || max_frames == 0) return 0;
    return drmp3_read_pcm_frames_f32(mp3_handle_, max_frames, buffer);
}

bool Mp3Decoder::seek(double target_seconds) {
    if (!mp3_handle_ || spec_.sample_rate == 0) return false;
    double clamped_sec = std::clamp(target_seconds, 0.0, duration_sec_);
    uint64_t target_frame = static_cast<uint64_t>(clamped_sec * spec_.sample_rate);
    return drmp3_seek_to_pcm_frame(mp3_handle_, target_frame) == DRMP3_TRUE;
}

double Mp3Decoder::getDuration() const {
    return duration_sec_;
}

AudioFormatSpec Mp3Decoder::getSpec() const {
    return spec_;
}

void Mp3Decoder::close() {
    if (mp3_handle_) {
        drmp3_uninit(mp3_handle_);
        delete mp3_handle_;
        mp3_handle_ = nullptr;
    }
    duration_sec_ = 0.0;
    total_frames_ = 0;
}

} // namespace audio_engine
