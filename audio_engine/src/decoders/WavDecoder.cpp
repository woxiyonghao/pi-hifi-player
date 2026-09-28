#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#include "audio_engine/decoders/WavDecoder.hpp"
#include <iostream>
#include <algorithm>

namespace audio_engine {

WavDecoder::WavDecoder() = default;

WavDecoder::~WavDecoder() {
    close();
}

bool WavDecoder::open(const std::string& filepath) {
    close();

    wav_handle_ = new drwav();
    if (!drwav_init_file(wav_handle_, filepath.c_str(), nullptr)) {
        std::cerr << "[WavDecoder] 打开 WAV 文件失败: " << filepath << std::endl;
        delete wav_handle_;
        wav_handle_ = nullptr;
        return false;
    }

    spec_.sample_rate = wav_handle_->sampleRate;
    spec_.channels = wav_handle_->channels;
    spec_.bit_depth = static_cast<uint8_t>(wav_handle_->bitsPerSample);
    spec_.format = SampleFormat::Float32;

    total_frames_ = wav_handle_->totalPCMFrameCount;
    duration_sec_ = (spec_.sample_rate > 0) ? (static_cast<double>(total_frames_) / spec_.sample_rate) : 0.0;

    return true;
}

uint64_t WavDecoder::readFrames(float* buffer, uint64_t max_frames) {
    if (!wav_handle_ || !buffer || max_frames == 0) return 0;
    return drwav_read_pcm_frames_f32(wav_handle_, max_frames, buffer);
}

bool WavDecoder::seek(double target_seconds) {
    if (!wav_handle_ || spec_.sample_rate == 0) return false;
    double clamped_sec = std::clamp(target_seconds, 0.0, duration_sec_);
    uint64_t target_frame = static_cast<uint64_t>(clamped_sec * spec_.sample_rate);
    return drwav_seek_to_pcm_frame(wav_handle_, target_frame) == DRWAV_TRUE;
}

double WavDecoder::getDuration() const {
    return duration_sec_;
}

AudioFormatSpec WavDecoder::getSpec() const {
    return spec_;
}

void WavDecoder::close() {
    if (wav_handle_) {
        drwav_uninit(wav_handle_);
        delete wav_handle_;
        wav_handle_ = nullptr;
    }
    duration_sec_ = 0.0;
    total_frames_ = 0;
}

} // namespace audio_engine
