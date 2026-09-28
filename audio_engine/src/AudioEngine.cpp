#include "audio_engine/AudioEngine.hpp"
#include "audio_engine/decoders/FlacDecoder.hpp"
#include "audio_engine/decoders/WavDecoder.hpp"
#include "audio_engine/decoders/Mp3Decoder.hpp"
#include "audio_engine/DsfParser.hpp"
#include "audio_engine/sinks/SdlAudioSink.hpp"
#include "audio_engine/sinks/AlsaAudioSink.hpp"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <random>

namespace audio_engine {

AudioEngine& AudioEngine::getInstance() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::AudioEngine() : ring_buffer_(131072) {
    // 默认输出驱动：使用跨平台低延迟 SdlAudioSink
    sink_ = std::make_unique<SdlAudioSink>();
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::init() {
    return true;
}

std::unique_ptr<IAudioDecoder> AudioEngine::createDecoderForFile(const std::string& filepath) {
    std::filesystem::path p(filepath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".flac") {
        return std::make_unique<FlacDecoder>();
    } else if (ext == ".wav") {
        return std::make_unique<WavDecoder>();
    } else if (ext == ".mp3") {
        return std::make_unique<Mp3Decoder>();
    } else if (ext == ".dsf" || ext == ".dff") {
        return std::make_unique<DsfParser>();
    }
    return nullptr;
}

bool AudioEngine::openAndPlay(const std::string& filepath) {
    stop();

    std::unique_ptr<IAudioDecoder> new_decoder = createDecoderForFile(filepath);
    if (!new_decoder) {
        std::cerr << "[AudioEngine] 不支持或无法识别的音频扩展名: " << filepath << std::endl;
        return false;
    }

    if (!new_decoder->open(filepath)) {
        std::cerr << "[AudioEngine] 解码器打开音频文件失败: " << filepath << std::endl;
        return false;
    }

    {
        std::lock_guard lock(decoder_mutex_);
        decoder_ = std::move(new_decoder);
        current_filepath_ = filepath;
        current_spec_ = decoder_->getSpec();
        duration_sec_ = decoder_->getDuration();
        ring_buffer_.reset();
        frames_consumed_by_sink_.store(0, std::memory_order_relaxed);
        seek_base_time_.store(0.0, std::memory_order_relaxed);
        is_eof_.store(false, std::memory_order_relaxed);
    }

    // 打开硬件输出设备 (根据音频文件的真实采样率和声道动态协商)
    if (!sink_->isOpen() || sink_->getActualSpec().sample_rate != current_spec_.sample_rate) {
        sink_->close();
        if (!sink_->open(current_spec_, [this](float* output, size_t frame_count) {
                onSinkDataNeeded(output, frame_count);
            })) {
            std::cerr << "[AudioEngine] 无法打开硬件输出设备" << std::endl;
            return false;
        }
    }

    is_playing_.store(true, std::memory_order_release);
    is_paused_.store(false, std::memory_order_release);
    is_decoding_.store(true, std::memory_order_release);

    sink_->start();

    // 启动后台无阻塞异步解码线程
    decode_thread_ = std::jthread([this](std::stop_token st) { decodeWorker(st); });

    return true;
}

void AudioEngine::play() {
    if (isIdle()) return;
    if (is_paused_.load(std::memory_order_acquire)) {
        is_paused_.store(false, std::memory_order_release);
        is_playing_.store(true, std::memory_order_release);
        if (sink_) sink_->start();
    }
}

void AudioEngine::pause() {
    if (is_playing_.load(std::memory_order_acquire)) {
        is_paused_.store(true, std::memory_order_release);
        is_playing_.store(false, std::memory_order_release);
        if (sink_) sink_->pause();
    }
}

void AudioEngine::togglePlayPause() {
    if (is_paused_.load(std::memory_order_acquire)) {
        play();
    } else {
        pause();
    }
}

void AudioEngine::stop() {
    is_decoding_.store(false, std::memory_order_release);
    is_playing_.store(false, std::memory_order_release);
    is_paused_.store(false, std::memory_order_release);

    if (decode_thread_.joinable()) {
        decode_thread_.request_stop();
        decode_thread_.join();
    }

    if (sink_) {
        sink_->stop();
    }

    {
        std::lock_guard lock(decoder_mutex_);
        if (decoder_) {
            decoder_->close();
            decoder_.reset();
        }
        ring_buffer_.reset();
        frames_consumed_by_sink_.store(0, std::memory_order_relaxed);
        seek_base_time_.store(0.0, std::memory_order_relaxed);
        duration_sec_ = 0.0;
        is_eof_.store(false, std::memory_order_relaxed);
    }
}

void AudioEngine::seek(double target_seconds) {
    std::lock_guard lock(decoder_mutex_);
    if (!decoder_) return;

    double clamped = std::clamp(target_seconds, 0.0, duration_sec_);
    if (decoder_->seek(clamped)) {
        ring_buffer_.reset();
        frames_consumed_by_sink_.store(0, std::memory_order_release);
        seek_base_time_.store(clamped, std::memory_order_release);
        is_eof_.store(false, std::memory_order_release);
    }
}

void AudioEngine::setVolume(float volume) {
    volume_.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_release);
}

float AudioEngine::getVolume() const {
    return is_muted_.load(std::memory_order_acquire) ? 0.0f : volume_.load(std::memory_order_acquire);
}

void AudioEngine::setMuted(bool muted) {
    is_muted_.store(muted, std::memory_order_release);
}

bool AudioEngine::isMuted() const {
    return is_muted_.load(std::memory_order_acquire);
}

bool AudioEngine::isBitPerfectDirect() const {
    return !is_muted_.load(std::memory_order_acquire) && (volume_.load(std::memory_order_acquire) >= 0.9999f);
}

bool AudioEngine::isPlaying() const {
    return is_playing_.load(std::memory_order_acquire);
}

bool AudioEngine::isPaused() const {
    return is_paused_.load(std::memory_order_acquire);
}

bool AudioEngine::isIdle() const {
    return !is_playing_.load(std::memory_order_acquire) && !is_paused_.load(std::memory_order_acquire);
}

double AudioEngine::getCurrentTimeSec() const {
    if (isIdle()) return 0.0;

    double base = seek_base_time_.load(std::memory_order_relaxed);
    uint64_t consumed = frames_consumed_by_sink_.load(std::memory_order_relaxed);
    double sample_rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100.0;

    double played_sec = static_cast<double>(consumed) / sample_rate;
    double latency = sink_ ? sink_->getHardwareLatencySec() : 0.0;
    double current = base + std::max(0.0, played_sec - latency);

    return std::clamp(current, 0.0, duration_sec_);
}

double AudioEngine::getDurationSec() const {
    return duration_sec_;
}

float AudioEngine::getProgress() const {
    if (duration_sec_ <= 0.0) return 0.0f;
    return static_cast<float>(getCurrentTimeSec() / duration_sec_);
}

AudioFormatSpec AudioEngine::getCurrentSpec() const {
    return current_spec_;
}

std::string AudioEngine::getCurrentFilePath() const {
    return current_filepath_;
}

void AudioEngine::setEofCallback(std::function<void()> callback) {
    eof_callback_ = std::move(callback);
}

void AudioEngine::decodeWorker(std::stop_token stop_token) {
    constexpr size_t CHUNK_FRAMES = 1024;
    std::vector<float> decode_buf(CHUNK_FRAMES * current_spec_.channels);

    while (!stop_token.stop_requested() && is_decoding_.load(std::memory_order_acquire)) {
        // 如果无锁环形缓冲区剩余空间不足 2 个 Block，休眠 5 毫秒等待声卡消费，避免突发忙轮询
        if (ring_buffer_.available_write() < (CHUNK_FRAMES * current_spec_.channels * 2)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        uint64_t frames_read = 0;
        {
            std::lock_guard lock(decoder_mutex_);
            if (!decoder_) break;
            frames_read = decoder_->readFrames(decode_buf.data(), CHUNK_FRAMES);
        }

        if (frames_read > 0) {
            size_t samples_to_write = frames_read * current_spec_.channels;
            ring_buffer_.write(decode_buf.data(), samples_to_write);
        } else {
            // 解码完毕 (EOF)
            is_eof_.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void AudioEngine::onSinkDataNeeded(float* output, size_t frame_count) {
    size_t samples_needed = frame_count * current_spec_.channels;
    size_t samples_read = ring_buffer_.read(output, samples_needed);

    // 若环形缓冲未填满 (或处于末尾)，补 0 防止爆音
    if (samples_read < samples_needed) {
        std::memset(output + samples_read, 0, (samples_needed - samples_read) * sizeof(float));
    }

    size_t frames_read = samples_read / current_spec_.channels;
    frames_consumed_by_sink_.fetch_add(frames_read, std::memory_order_relaxed);

    // 音量与 Bit-Perfect 控制处理
    bool muted = is_muted_.load(std::memory_order_acquire);
    float vol = volume_.load(std::memory_order_acquire);

    if (muted) {
        std::memset(output, 0, samples_needed * sizeof(float));
    } else if (vol >= 0.9999f) {
        // 【Bit-Perfect 0dB 源码直出】：不进行任何浮点乘法计算，保持原始数据绝对纯净！
    } else {
        // 【发烧级 64-bit 浮点音量衰减 + TPDF (三角概率分布) Dither 抖动】
        // 消除量化截断产生的谐波失真，维持高解析动态
        static thread_local std::mt19937 dither_gen(1337);
        static thread_local std::uniform_real_distribution<double> dither_dist(-1.0, 1.0);
        constexpr double DITHER_SCALE = 1.0 / 8388608.0; // 对应 24-bit LSB 尺度

        double double_vol = static_cast<double>(vol);
        for (size_t i = 0; i < samples_needed; ++i) {
            double sample = static_cast<double>(output[i]) * double_vol;
            double tpdf_dither = (dither_dist(dither_gen) + dither_dist(dither_gen)) * 0.5 * DITHER_SCALE;
            output[i] = static_cast<float>(sample + tpdf_dither);
        }
    }

    // 实时频谱分析更新
    updateSpectrumAnalysis(output, frame_count);

    // 播放完毕检测
    if (is_eof_.load(std::memory_order_acquire) && ring_buffer_.available_read() == 0) {
        if (eof_callback_) {
            eof_callback_();
        }
    }
}

static void fft256(float* re, float* im) {
    int j = 0;
    for (int i = 0; i < 256 - 1; ++i) {
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
        int k = 128;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
    for (int len = 2; len <= 256; len <<= 1) {
        float ang = -2.0f * 3.14159265358979323846f / len;
        float wlen_re = std::cos(ang);
        float wlen_im = std::sin(ang);
        for (int i = 0; i < 256; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                float u_re = re[i + k];
                float u_im = im[i + k];
                float v_re = re[i + k + len / 2] * w_re - im[i + k + len / 2] * w_im;
                float v_im = re[i + k + len / 2] * w_im + im[i + k + len / 2] * w_re;
                re[i + k] = u_re + v_re;
                im[i + k] = u_im + v_im;
                re[i + k + len / 2] = u_re - v_re;
                im[i + k + len / 2] = u_im - v_im;
                float next_w_re = w_re * wlen_re - w_im * wlen_im;
                float next_w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = next_w_re;
                w_im = next_w_im;
            }
        }
    }
}

void AudioEngine::updateSpectrumAnalysis(const float* samples, size_t frame_count) {
    if (!samples || frame_count == 0 || isMuted()) {
        std::lock_guard lock(spectrum_mutex_);
        for (auto& lvl : spectrum_levels_) {
            lvl *= 0.85f;
        }
        return;
    }

    constexpr int FFT_SIZE = 256;
    float re[FFT_SIZE] = {0.0f};
    float im[FFT_SIZE] = {0.0f};

    size_t samples_to_use = std::min(frame_count, static_cast<size_t>(FFT_SIZE));
    uint32_t channels = current_spec_.channels;

    for (size_t i = 0; i < samples_to_use; ++i) {
        float mono = 0.0f;
        if (channels >= 2) {
            mono = 0.5f * (samples[i * 2] + samples[i * 2 + 1]);
        } else {
            mono = samples[i];
        }
        // Hann window
        float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (FFT_SIZE - 1)));
        re[i] = mono * window;
    }

    fft256(re, im);

    // 12 个对数分布频段的 Bin 起止索引 (~86Hz 至 ~11kHz)
    static const int band_bins[12][2] = {
        {1, 1},    // ~86 Hz
        {2, 2},    // ~172 Hz
        {3, 4},    // ~258 - 344 Hz
        {5, 7},    // ~430 - 602 Hz
        {8, 11},   // ~688 - 946 Hz
        {12, 16},  // ~1.0k - 1.4k Hz
        {17, 23},  // ~1.5k - 2.0k Hz
        {24, 32},  // ~2.1k - 2.8k Hz
        {33, 45},  // ~2.9k - 3.9k Hz
        {46, 64},  // ~4.0k - 5.5k Hz
        {65, 90},  // ~5.6k - 7.7k Hz
        {91, 127}  // ~7.8k - 11.0k Hz
    };

    // 高频人耳等响度视觉补偿增益 (Treble Pre-emphasis)
    static const float band_weights[12] = {
        2.5f, 2.2f, 2.0f, 1.9f, 2.0f, 2.2f,
        2.5f, 2.9f, 3.4f, 4.0f, 4.8f, 5.8f
    };

    std::lock_guard lock(spectrum_mutex_);
    for (int b = 0; b < 12; ++b) {
        int b_start = band_bins[b][0];
        int b_end = band_bins[b][1];
        float sum_mag = 0.0f;
        for (int k = b_start; k <= b_end; ++k) {
            sum_mag += std::sqrt(re[k] * re[k] + im[k] * im[k]);
        }
        float avg_mag = sum_mag / (b_end - b_start + 1);
        float target = std::clamp(avg_mag * band_weights[b] * 0.15f, 0.0f, 1.0f);

        // 动效弹道：快速起音 (Attack) + 平滑自然衰减 (Decay)
        if (target > spectrum_levels_[b]) {
            spectrum_levels_[b] = target;
        } else {
            spectrum_levels_[b] = spectrum_levels_[b] * 0.85f + target * 0.15f;
        }
    }
}

void AudioEngine::getSpectrumLevels(float* out_levels, size_t count) {
    if (!out_levels || count == 0) return;

    if (!isPlaying()) {
        std::fill(out_levels, out_levels + count, 0.0f);
        return;
    }

    std::lock_guard lock(spectrum_mutex_);
    size_t copy_cnt = std::min(count, spectrum_levels_.size());
    for (size_t i = 0; i < copy_cnt; ++i) {
        out_levels[i] = spectrum_levels_[i];
    }
    for (size_t i = copy_cnt; i < count; ++i) {
        out_levels[i] = 0.0f;
    }
}

} // namespace audio_engine
