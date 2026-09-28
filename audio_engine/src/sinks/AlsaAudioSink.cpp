#include "audio_engine/sinks/AlsaAudioSink.hpp"
#include <iostream>
#include <vector>

namespace audio_engine {

AlsaAudioSink::AlsaAudioSink(std::string device_name) : device_name_(std::move(device_name)) {}

AlsaAudioSink::~AlsaAudioSink() {
    close();
}

#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)

bool AlsaAudioSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();
    callback_ = callback;

    int err = snd_pcm_open(&pcm_handle_, device_name_.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "[AlsaAudioSink] 打开 ALSA 设备失败 (" << device_name_ << "): "
                  << snd_strerror(err) << std::endl;
        return false;
    }

    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm_handle_, hw_params);

    // 启用交错存取 (Interleaved)
    snd_pcm_hw_params_set_access(pcm_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // 设置格式：优先请求 Float32 或 S32_LE
    snd_pcm_format_t alsa_fmt = SND_PCM_FORMAT_FLOAT_LE;
    if (snd_pcm_hw_params_set_format(pcm_handle_, hw_params, alsa_fmt) < 0) {
        alsa_fmt = SND_PCM_FORMAT_S32_LE;
        if (snd_pcm_hw_params_set_format(pcm_handle_, hw_params, alsa_fmt) < 0) {
            alsa_fmt = SND_PCM_FORMAT_S16_LE;
            snd_pcm_hw_params_set_format(pcm_handle_, hw_params, alsa_fmt);
        }
    }

    snd_pcm_hw_params_set_channels(pcm_handle_, hw_params, requested_spec.channels);
    unsigned int rate = requested_spec.sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm_handle_, hw_params, &rate, 0);

    // 缓冲区大小设定 (低抖动 1024 帧)
    snd_pcm_uframes_t period_size = 1024;
    snd_pcm_hw_params_set_period_size_near(pcm_handle_, hw_params, &period_size, 0);

    err = snd_pcm_hw_params(pcm_handle_, hw_params);
    if (err < 0) {
        std::cerr << "[AlsaAudioSink] 应用硬件参数失败: " << snd_strerror(err) << std::endl;
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
        return false;
    }

    actual_spec_.sample_rate = rate;
    actual_spec_.channels = requested_spec.channels;
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;
    buffer_latency_sec_ = (rate > 0) ? (static_cast<double>(period_size) / rate) : 0.0;

    return true;
}

void AlsaAudioSink::start() {
    if (!pcm_handle_) return;
    is_running_.store(true, std::memory_order_release);
    is_paused_.store(false, std::memory_order_release);

    if (!alsa_thread_.joinable()) {
        alsa_thread_ = std::jthread([this](std::stop_token st) { alsaLoop(st); });
    }
}

void AlsaAudioSink::pause() {
    is_paused_.store(true, std::memory_order_release);
    if (pcm_handle_) {
        snd_pcm_pause(pcm_handle_, 1);
    }
}

void AlsaAudioSink::stop() {
    is_running_.store(false, std::memory_order_release);
    if (alsa_thread_.joinable()) {
        alsa_thread_.request_stop();
        alsa_thread_.join();
    }
    if (pcm_handle_) {
        snd_pcm_drop(pcm_handle_);
    }
}

void AlsaAudioSink::close() {
    stop();
    if (pcm_handle_) {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
    }
}

void AlsaAudioSink::alsaLoop(std::stop_token stop_token) {
    std::vector<float> period_buf(1024 * actual_spec_.channels);
    while (!stop_token.stop_requested() && is_running_.load(std::memory_order_acquire)) {
        if (is_paused_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (callback_) {
            callback_(period_buf.data(), 1024);
        }

        snd_pcm_sframes_t written = snd_pcm_writei(pcm_handle_, period_buf.data(), 1024);
        if (written < 0) {
            // 处理 XRUN (Buffer Underrun)
            snd_pcm_recover(pcm_handle_, static_cast<int>(written), 0);
        }
    }
}

#else

// macOS / 非 ALSA 环境下的优雅空实现 (转由 SdlAudioSink 处理)
bool AlsaAudioSink::open([[maybe_unused]] const AudioFormatSpec& requested_spec, [[maybe_unused]] AudioCallback callback) {
    std::cerr << "[AlsaAudioSink] 当前平台为 macOS 或非 ALSA 环境，请使用 SdlAudioSink 作为输出驱动。" << std::endl;
    return false;
}

void AlsaAudioSink::start() {}
void AlsaAudioSink::pause() {}
void AlsaAudioSink::stop() {}
void AlsaAudioSink::close() {}

#endif

double AlsaAudioSink::getHardwareLatencySec() const {
    return buffer_latency_sec_;
}

bool AlsaAudioSink::isOpen() const {
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    return pcm_handle_ != nullptr;
#else
    return false;
#endif
}

AudioFormatSpec AlsaAudioSink::getActualSpec() const {
    return actual_spec_;
}

} // namespace audio_engine
