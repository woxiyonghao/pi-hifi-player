#include "audio_engine/sinks/SdlAudioSink.hpp"
#include <iostream>
#include <cstring>

namespace audio_engine {

SdlAudioSink::SdlAudioSink() {
    // 确保 SDL 音频子系统已初始化
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            std::cerr << "[SdlAudioSink] 初始化 SDL 音频子系统失败: " << SDL_GetError() << std::endl;
        }
    }
}

SdlAudioSink::~SdlAudioSink() {
    close();
}

bool SdlAudioSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();

    callback_ = callback;

    SDL_AudioSpec desired;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq = static_cast<int>(requested_spec.sample_rate);
    desired.format = AUDIO_F32SYS; // 原生 32-bit Float
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    // 树莓派 Linux ALSA 平台默认使用 2048 帧 (~46ms)，彻底杜绝 USB 声卡周期欠载 (Underrun) 导致的"滴滴"杂音
    desired.samples = (buffer_size_samples_ > 0) ? static_cast<Uint16>(buffer_size_samples_) : 2048;
#else
    desired.samples = (buffer_size_samples_ > 0) ? static_cast<Uint16>(buffer_size_samples_) : 1024;
#endif
    desired.callback = sdlCallbackThunk;
    desired.userdata = this;

    SDL_AudioSpec obtained;
    std::memset(&obtained, 0, sizeof(obtained));

    const char* target_device_name = nullptr;
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    int num_audio_devices = SDL_GetNumAudioDevices(0);
    std::cout << "[SdlAudioSink] 正在扫描系统物理音频输出设备 (共 " << num_audio_devices << " 个)..." << std::endl;
    for (int i = 0; i < num_audio_devices; ++i) {
        const char* d_name = SDL_GetAudioDeviceName(i, 0);
        if (!d_name) continue;
        std::cout << "  - [" << i << "] " << d_name << std::endl;

        std::string lower(d_name);
        for (char& c : lower) c = static_cast<char>(std::tolower(c));

        // 智能优选：当检测到 USB 声卡、外置 DAC、耳机口、或者 hw/plughw 声卡时，主动优先绑定，
        // 彻底杜绝 HDMI 抢占默认声卡导致 3.5mm 接口静音或无声！
        if ((lower.find("usb") != std::string::npos ||
             lower.find("dac") != std::string::npos ||
             lower.find("headphone") != std::string::npos ||
             lower.find("audio") != std::string::npos ||
             lower.find("generalplus") != std::string::npos ||
             lower.find("c-media") != std::string::npos) &&
            lower.find("hdmi") == std::string::npos &&
            lower.find("vc4") == std::string::npos &&
            lower.find("bcm2835") == std::string::npos) {
            target_device_name = d_name;
            std::cout << "[SdlAudioSink] 🎯 自动优选外接 HiFi DAC / USB 声卡: " << target_device_name << std::endl;
            break;
        }
    }
#endif

    // allowed_changes = 0: 确保 SDL 按照 requested_spec 提供回调数据流；
    // 当声卡硬件采样率与母带不一致时，SDL 底层透明进行高保真重采样，杜绝打开失败与无声静音
    device_id_ = SDL_OpenAudioDevice(target_device_name, 0, &desired, &obtained, 0);
    if (device_id_ == 0 && target_device_name != nullptr) {
        std::cerr << "[SdlAudioSink] 指定外置声卡打开失败，自动降级为系统默认设备重试..." << std::endl;
        device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    }

    if (device_id_ == 0) {
        std::cerr << "[SdlAudioSink] 打开音频输出设备失败: " << SDL_GetError() << std::endl;
        return false;
    }

#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    // 自动重置 ALSA 硬件混音器：解除静音并将 Master/PCM/Headphone/Speaker 增益推至 100% (0dB)，
    // 输出标准的 Line-Out 线路电平 (1.0~2.0Vrms)，彻底解决外接后级功放时音量极微弱或无声的问题！
    std::system("(for c in 0 1 2 3; do "
                "amixer -c $c sset Master 100% unmute 2>/dev/null; "
                "amixer -c $c sset PCM 100% unmute 2>/dev/null; "
                "amixer -c $c sset Headphone 100% unmute 2>/dev/null; "
                "amixer -c $c sset Speaker 100% unmute 2>/dev/null; "
                "amixer -c $c sset 'Line Out' 100% unmute 2>/dev/null; "
                "done) >/dev/null 2>&1 &");
#endif

    actual_spec_.sample_rate = static_cast<uint32_t>(obtained.freq);
    actual_spec_.channels = obtained.channels;
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;

    buffer_latency_sec_ = (obtained.freq > 0) ? (static_cast<double>(obtained.samples) / obtained.freq) : 0.0;

    return true;
}

void SdlAudioSink::start() {
    if (device_id_ > 0) {
        is_running_.store(true, std::memory_order_release);
        SDL_PauseAudioDevice(device_id_, 0); // 0 = 取消暂停，启动播放
    }
}

void SdlAudioSink::pause() {
    if (device_id_ > 0) {
        is_running_.store(false, std::memory_order_release);
        SDL_PauseAudioDevice(device_id_, 1); // 1 = 暂停推流
    }
}

void SdlAudioSink::stop() {
    if (device_id_ > 0) {
        is_running_.store(false, std::memory_order_release);
        SDL_PauseAudioDevice(device_id_, 1);
        SDL_ClearQueuedAudio(device_id_);
    }
}

void SdlAudioSink::close() {
    if (device_id_ > 0) {
        stop();
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    is_running_.store(false, std::memory_order_release);
}

double SdlAudioSink::getHardwareLatencySec() const {
    return buffer_latency_sec_;
}

bool SdlAudioSink::isOpen() const {
    return device_id_ > 0;
}

AudioFormatSpec SdlAudioSink::getActualSpec() const {
    return actual_spec_;
}

void SdlAudioSink::sdlCallbackThunk(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<SdlAudioSink*>(userdata);
    if (!self || !self->is_running_.load(std::memory_order_acquire)) {
        std::memset(stream, 0, len);
        return;
    }

    auto* float_stream = reinterpret_cast<float*>(stream);
    size_t frame_count = len / (self->actual_spec_.channels * sizeof(float));

    if (self->callback_) {
        self->callback_(float_stream, frame_count);
    } else {
        std::memset(stream, 0, len);
    }
}

} // namespace audio_engine
