#include "audio_engine/AudioEngine.hpp"
#include "audio_engine/decoders/FlacDecoder.hpp"
#include "audio_engine/decoders/WavDecoder.hpp"
#include "audio_engine/decoders/Mp3Decoder.hpp"
#include "audio_engine/DsfParser.hpp"

#if defined(__APPLE__)
#include "audio_engine/decoders/AppleAudioDecoder.hpp"
#include "audio_engine/decoders/CoreAudioPcmDecoder.hpp"
#include <TargetConditionals.h>
#endif

#if defined(__ANDROID__)
#include "audio_engine/sinks/AAudioSink.hpp"
#elif defined(__APPLE__)
#include "audio_engine/sinks/AudioQueueSink.hpp"
#if TARGET_OS_IPHONE
#include "audio_engine/sinks/RemoteIOSink.hpp"

// 由 iPad 宿主实现。返回 true 表示该设置已作为 AVAudioSession 的
// 硬件 I/O 缓冲偏好处理，不能再改写已经分配好的 AudioQueue 缓冲区。
extern "C" bool HifiPadSetPreferredIOBufferFrames(uint32_t frames) __attribute__((weak_import));
extern "C" uint32_t HifiPadGetBluetoothOutputSampleRate() __attribute__((weak_import));
#endif
#else
#include "audio_engine/sinks/SdlAudioSink.hpp"
#include "audio_engine/sinks/AlsaAudioSink.hpp"
#endif

#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <random>

#if defined(__APPLE__)
#if TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
#include "audio_engine/MacAudioOutput.hpp"
#endif
#include <AudioToolbox/AudioToolbox.h>
#include <unistd.h>

#if TARGET_OS_OSX
static OSStatus onDefaultDeviceChangedThunk(AudioObjectID /*inObjectID*/, UInt32 /*inNumberAddresses*/,
                                           const AudioObjectPropertyAddress /*inAddresses*/[], void* inClientData) {
    auto* self = static_cast<audio_engine::AudioEngine*>(inClientData);
    if (self) {
        self->notifyOutputDeviceChanged();
    }
    return noErr;
}
#endif
#endif

namespace audio_engine {

#if defined(__APPLE__) && TARGET_OS_IPHONE
static std::atomic<uint64_t> g_iPadSinkUnderflowCount{0};
#endif

AudioEngine& AudioEngine::getInstance() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::AudioEngine() : ring_buffer_(1048576) {
#if defined(__ANDROID__)
    // Android 原生高保真 AAudio 独占/低延迟推流驱动
    sink_ = std::make_unique<AAudioSink>();
#elif defined(__APPLE__)
    // Apple 平台 (macOS / iOS / iPadOS) 统一使用原生高保真 AudioQueueSink
    // 原生支持 AirPods Pro / 蓝牙无线耳机热插拔、立体声/空间音频、自动采样率自适应
    sink_ = std::make_unique<AudioQueueSink>();
#if TARGET_OS_OSX
    last_active_device_id_ = getPlaybackOutputDeviceID();
#endif
#else
    // Linux / 树莓派平台：使用低延迟 SdlAudioSink
    sink_ = std::make_unique<SdlAudioSink>();
#endif
    registerDeviceListener();
}

AudioEngine::~AudioEngine() {
    unregisterDeviceListener();
    stop();
#if defined(__APPLE__) && TARGET_OS_OSX
    releaseHogMode();
#endif
}

bool AudioEngine::init() {
    return true;
}

std::unique_ptr<IAudioDecoder> AudioEngine::createDecoderForFile(const std::string& filepath) {
    std::filesystem::path p(filepath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

#if defined(__APPLE__) && TARGET_OS_IPHONE
    // AirPods 实际工作在 AVAudioSession 的输出采样率（通常 48 kHz）。先在
    // 解码线程用 ExtAudioFile 完成系统级重采样，再以同采样率打开 AudioQueue，
    // 避免 AudioQueue/A2DP 在实时输出线程中隐式转换 44.1/192 kHz 音源。
    if (HifiPadGetBluetoothOutputSampleRate) {
        uint32_t bluetooth_rate = HifiPadGetBluetoothOutputSampleRate();
        if (bluetooth_rate > 0) {
            std::cout << "[AudioEngine] iPad Bluetooth output: decode/resample to "
                      << bluetooth_rate << " Hz" << std::endl;
            return std::make_unique<AppleAudioDecoder>(bluetooth_rate);
        }
    }
#endif

    if (ext == ".flac") {
        return std::make_unique<FlacDecoder>();
    } else if (ext == ".wav") {
        return std::make_unique<WavDecoder>();
    } else if (ext == ".mp3") {
        return std::make_unique<Mp3Decoder>();
    } else if (ext == ".dsf" || ext == ".dff") {
        return std::make_unique<DsfParser>();
#if defined(__APPLE__)
    } else if (ext == ".m4a" || ext == ".alac" || ext == ".aac" || ext == ".aiff" || ext == ".aif" || ext == ".caf") {
        return std::make_unique<AppleAudioDecoder>();
#endif
    }
#if defined(__APPLE__)
    // 在 Apple 平台（macOS / iPadOS / iOS）上，对未能匹配扩展名的音频使用系统级 ExtAudioFile 兜底
    return std::make_unique<AppleAudioDecoder>();
#else
    return nullptr;
#endif
}

bool AudioEngine::openAndPlay(const std::string& filepath) {
    return openAndPlaySection(filepath, 0.0, 0.0);
}

bool AudioEngine::openAndPlaySection(const std::string& filepath, double start_sec, double end_sec) {
    std::lock_guard lock_dev(device_mutex_);

    start_sec = std::max(0.0, start_sec);
    bool is_section = (end_sec > start_sec);

    // 检查是否已经在播放相同文件且解码器正常工作 (同张 CUE 专辑内分轨极速切歌优化)
    bool is_same_file = false;
    {
        std::lock_guard lock(decoder_mutex_);
        if (decoder_ && current_filepath_ == filepath && sink_ && sink_->isOpen()) {
            is_same_file = true;
        }
    }

    if (is_same_file) {
        sink_->stop();
        // 极速同文件切歌：无需重新开辟硬件声卡设备
        is_playing_.store(false, std::memory_order_release);
        is_decoding_.store(false, std::memory_order_release);

        if (decode_thread_.joinable()) {
            decode_thread_.request_stop();
            decode_thread_.join();
        }

        {
            std::lock_guard lock(decoder_mutex_);
            section_start_sec_.store(start_sec, std::memory_order_release);
            section_end_sec_.store(end_sec, std::memory_order_release);
            is_section_active_.store(is_section, std::memory_order_release);

            if (is_section) {
                duration_sec_ = end_sec - start_sec;
            } else {
                duration_sec_ = std::max(0.0, decoder_->getDuration() - start_sec);
            }

            decoder_->seek(start_sec);
            equalizer_.reset();
            ring_buffer_.reset();
            frames_consumed_by_sink_.store(0, std::memory_order_relaxed);
            seek_base_time_.store(0.0, std::memory_order_relaxed);
            is_eof_.store(false, std::memory_order_relaxed);
            eof_reported_.store(false, std::memory_order_relaxed);
            resetFade();

            uint32_t rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100;
            if (is_section) {
                section_total_frames_.store(static_cast<uint64_t>(duration_sec_ * rate), std::memory_order_release);
            } else {
                section_total_frames_.store(0, std::memory_order_release);
            }
            section_frames_read_.store(0, std::memory_order_release);

            constexpr size_t PREBUFFER_FRAMES = 65536;
            size_t to_req = PREBUFFER_FRAMES;
            if (is_section && section_total_frames_.load() < to_req) {
                to_req = static_cast<size_t>(section_total_frames_.load());
            }
            if (to_req > 0) {
                std::vector<float> prebuf(to_req * current_spec_.channels);
                uint64_t pre_read = decoder_->readFrames(prebuf.data(), to_req);
                if (pre_read > 0) {
                    section_frames_read_.store(pre_read, std::memory_order_release);
                    ring_buffer_.write(prebuf.data(), pre_read * current_spec_.channels);
                }
            }
        }

        is_playing_.store(true, std::memory_order_release);
        is_paused_.store(false, std::memory_order_release);
        is_decoding_.store(true, std::memory_order_release);

        decode_thread_ = std::jthread([this](std::stop_token st) { decodeWorker(st); });
        sink_->start();
        return !sink_->hasError();
    }

    stop();

#if defined(__APPLE__) && TARGET_OS_IPHONE
    g_iPadSinkUnderflowCount.store(0, std::memory_order_relaxed);
#endif

    std::unique_ptr<IAudioDecoder> new_decoder = createDecoderForFile(filepath);
    if (!new_decoder) {
        std::cerr << "[AudioEngine] 不支持或无法识别的音频扩展名: " << filepath << std::endl;
        return false;
    }

    if (!new_decoder->open(filepath)) {
#if defined(__APPLE__)
        auto apple_decoder = std::make_unique<AppleAudioDecoder>();
        if (apple_decoder->open(filepath)) {
            new_decoder = std::move(apple_decoder);
        } else {
            std::cerr << "[AudioEngine] 解码器打开音频文件失败: " << filepath << std::endl;
            return false;
        }
#else
        std::cerr << "[AudioEngine] 解码器打开音频文件失败: " << filepath << std::endl;
        return false;
#endif
    }

#if defined(__APPLE__) && TARGET_OS_IPHONE
    bool should_use_remote_io = HifiPadGetBluetoothOutputSampleRate
        && HifiPadGetBluetoothOutputSampleRate() > 0;
    if (should_use_remote_io != using_ipad_remote_io_) {
        if (sink_) {
            sink_->close();
        }
        sink_ = should_use_remote_io
            ? std::unique_ptr<IAudioSink>(std::make_unique<RemoteIOSink>())
            : std::unique_ptr<IAudioSink>(std::make_unique<AudioQueueSink>());
        using_ipad_remote_io_ = should_use_remote_io;
        std::cout << "[AudioEngine] iPad output backend: "
                  << (using_ipad_remote_io_ ? "RemoteIO" : "AudioQueue") << std::endl;
    }
#endif

#if defined(__APPLE__) && TARGET_OS_OSX
    // Feed AudioQueue the current device's native PCM rate. In particular,
    // AirPods must not convert 192/384 kHz on their real-time output thread.
    releaseHogMode();
    const auto source_spec = new_decoder->getSpec();
    auto output_spec = source_spec;
    output_spec.sample_rate = getActiveHardwareSampleRate();
    output_spec.channels = std::min<uint32_t>(source_spec.channels, 2);
    AudioObjectPropertyAddress channels_address{kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
    UInt32 channels_size = 0;
    const auto device = getPlaybackOutputDeviceID();
    if (AudioObjectGetPropertyDataSize(device, &channels_address, 0, nullptr, &channels_size) == noErr &&
        channels_size >= sizeof(AudioBufferList)) {
        std::vector<uint8_t> storage(channels_size);
        auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
        if (AudioObjectGetPropertyData(device, &channels_address, 0, nullptr, &channels_size, list) == noErr) {
            uint32_t count = 0;
            for (UInt32 i = 0; i < list->mNumberBuffers; ++i) count += list->mBuffers[i].mNumberChannels;
            if (count > 0) output_spec.channels = std::min(output_spec.channels, count);
        }
    }
    const bool conversion = source_spec.sample_rate != output_spec.sample_rate ||
                            source_spec.channels != output_spec.channels;
    is_resampling_.store(conversion);
    if (conversion) {
        auto pcm = std::make_unique<CoreAudioPcmDecoder>(std::move(new_decoder), output_spec);
        if (!pcm->configure()) return false;
        new_decoder = std::move(pcm);
        std::cout << "[AudioEngine] Device-compatible PCM: " << source_spec.sample_rate
                  << " -> " << output_spec.sample_rate << " Hz, " << output_spec.channels << " channels" << std::endl;
    }
#endif

    {
        std::lock_guard lock(decoder_mutex_);
        decoder_ = std::move(new_decoder);
        current_filepath_ = filepath;
        current_spec_ = decoder_->getSpec();

        section_start_sec_.store(start_sec, std::memory_order_release);
        section_end_sec_.store(end_sec, std::memory_order_release);
        is_section_active_.store(is_section, std::memory_order_release);

        if (is_section) {
            duration_sec_ = end_sec - start_sec;
        } else {
            duration_sec_ = std::max(0.0, decoder_->getDuration() - start_sec);
        }

        if (start_sec > 0.001) {
            decoder_->seek(start_sec);
        }

        ring_buffer_.reset();
        equalizer_.reset();
        frames_consumed_by_sink_.store(0, std::memory_order_relaxed);
        seek_base_time_.store(0.0, std::memory_order_relaxed);
        is_eof_.store(false, std::memory_order_relaxed);
        eof_reported_.store(false, std::memory_order_relaxed);
        resetFade();

        uint32_t rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100;
        if (is_section) {
            section_total_frames_.store(static_cast<uint64_t>(duration_sec_ * rate), std::memory_order_release);
        } else {
            section_total_frames_.store(0, std::memory_order_release);
        }
        section_frames_read_.store(0, std::memory_order_release);
    }

    // AudioQueue uses the shared system route; taking a process Hog lock can
    // silence its output service. Hardware preferences must not block playback.
#if defined(__APPLE__) && TARGET_OS_OSX
    last_active_device_id_ = getPlaybackOutputDeviceID();
    releaseHogMode();
#endif

    // 2. 打开硬件输出设备 (根据音频文件的真实采样率和声道动态协商)
    if (!sink_->isOpen() || sink_->getActualSpec().sample_rate != current_spec_.sample_rate || sink_->getActualSpec().channels != current_spec_.channels) {
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

    constexpr size_t PREBUFFER_FRAMES = 65536;
    size_t to_req = PREBUFFER_FRAMES;
    if (is_section && section_total_frames_.load() < to_req) {
        to_req = static_cast<size_t>(section_total_frames_.load());
    }
    if (to_req > 0) {
        std::vector<float> prebuf(to_req * current_spec_.channels);
        uint64_t pre_read = 0;
        {
            std::lock_guard lock(decoder_mutex_);
            if (decoder_) {
                pre_read = decoder_->readFrames(prebuf.data(), to_req);
            }
        }
        if (pre_read > 0) {
            section_frames_read_.store(pre_read, std::memory_order_release);
            ring_buffer_.write(prebuf.data(), pre_read * current_spec_.channels);
        }
    }

    decode_thread_ = std::jthread([this](std::stop_token st) { decodeWorker(st); });

    size_t safe_watermark = std::min(static_cast<size_t>(32768 * current_spec_.channels),
                                     static_cast<size_t>(duration_sec_ * current_spec_.sample_rate * current_spec_.channels));
    int wait_cycles = 0;
    while (ring_buffer_.available_read() < safe_watermark && !is_eof_.load(std::memory_order_acquire) && wait_cycles < 30) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        wait_cycles++;
    }

    sink_->start();
    if (sink_->hasError()) {
        stop();
        return false;
    }
    return true;
}

void AudioEngine::play() {
    std::lock_guard device_lock(device_mutex_);
    if (is_playing_.load(std::memory_order_acquire)) {
        return;
    }
    if (is_paused_.load(std::memory_order_acquire)) {
        is_paused_.store(false, std::memory_order_release);
        is_playing_.store(true, std::memory_order_release);
        if (sink_) sink_->start();
        return;
    }
    // 若当前处于空闲但解码器与音频接收端依然就绪（如自然播完后重播或 Seek 后恢复）
    std::lock_guard lock(decoder_mutex_);
    if (decoder_ && sink_ && sink_->isOpen()) {
        is_playing_.store(true, std::memory_order_release);
        is_paused_.store(false, std::memory_order_release);
        sink_->start();
    }
}

void AudioEngine::pause() {
    std::lock_guard lock(device_mutex_);
    is_paused_.store(true, std::memory_order_release);
    is_playing_.store(false, std::memory_order_release);
    if (sink_) sink_->pause();
}

void AudioEngine::togglePlayPause() {
    if (is_paused_.load(std::memory_order_acquire)) {
        play();
    } else {
        pause();
    }
}

void AudioEngine::stop() {
    std::lock_guard device_lock(device_mutex_);
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
#if defined(__APPLE__) && TARGET_OS_OSX
    releaseHogMode();
#endif

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
        section_start_sec_.store(0.0, std::memory_order_relaxed);
        section_end_sec_.store(0.0, std::memory_order_relaxed);
        is_section_active_.store(false, std::memory_order_relaxed);
        section_frames_read_.store(0, std::memory_order_relaxed);
        section_total_frames_.store(0, std::memory_order_relaxed);
        is_eof_.store(false, std::memory_order_relaxed);
        eof_reported_.store(false, std::memory_order_relaxed);
        resetFade();
    }
}

void AudioEngine::seek(double target_seconds) {
    std::lock_guard device_lock(device_mutex_);
    const bool was_playing = is_playing_.load();
    is_decoding_.store(false);
    if (decode_thread_.joinable()) {
        decode_thread_.request_stop();
        decode_thread_.join();
    }
    if (sink_) sink_->stop();
    {
    std::lock_guard lock(decoder_mutex_);
    if (!decoder_) return;

    double clamped = std::clamp(target_seconds, 0.0, duration_sec_);
    double abs_target = section_start_sec_.load(std::memory_order_relaxed) + clamped;

    if (decoder_->seek(abs_target)) {
        equalizer_.reset();
        ring_buffer_.reset();
        constexpr size_t SEEK_PREBUFFER = 32768;
        uint32_t rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100;
        uint64_t new_read_frames = static_cast<uint64_t>(clamped * rate);
        section_frames_read_.store(new_read_frames, std::memory_order_release);

        size_t to_req = SEEK_PREBUFFER;
        if (is_section_active_.load(std::memory_order_relaxed)) {
            uint64_t total = section_total_frames_.load(std::memory_order_relaxed);
            if (new_read_frames < total) {
                to_req = static_cast<size_t>(std::min<uint64_t>(SEEK_PREBUFFER, total - new_read_frames));
            } else {
                to_req = 0;
            }
        }

        if (to_req > 0) {
            std::vector<float> prebuf(to_req * current_spec_.channels);
            uint64_t pre_read = decoder_->readFrames(prebuf.data(), to_req);
            if (pre_read > 0) {
                section_frames_read_.fetch_add(pre_read, std::memory_order_release);
                ring_buffer_.write(prebuf.data(), pre_read * current_spec_.channels);
            }
        }
        frames_consumed_by_sink_.store(0, std::memory_order_release);
        seek_base_time_.store(clamped, std::memory_order_release);
        is_eof_.store(false, std::memory_order_release);
        eof_reported_.store(false, std::memory_order_release);
    }
    }
    is_decoding_.store(true);
    decode_thread_ = std::jthread([this](std::stop_token st) { decodeWorker(st); });
    if (was_playing && sink_) sink_->start();
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
    bool soft_clean = !is_muted_.load(std::memory_order_acquire) &&
                      (volume_.load(std::memory_order_acquire) >= 0.9999f) &&
                      !is_eq_enabled_.load(std::memory_order_acquire);
#if defined(__APPLE__) && TARGET_OS_OSX
    return soft_clean && !is_resampling_.load() && !isCurrentDeviceBluetooth();
#endif
    return soft_clean;
}

void AudioEngine::setExclusiveMode(bool exclusive) {
    std::lock_guard lock(device_mutex_);
    is_exclusive_mode_.store(exclusive, std::memory_order_release);
#if defined(__APPLE__) && TARGET_OS_OSX
    releaseHogMode();
#endif
}

bool AudioEngine::isExclusiveMode() const {
    return is_exclusive_mode_.load(std::memory_order_acquire);
}

bool AudioEngine::isHogModeActive() const {
    return isRealHogActive();
}

AudioEngine::DeviceTransportType AudioEngine::getCurrentDeviceTransport() const {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioDeviceID dev = getPlaybackOutputDeviceID();
    if (dev == kAudioObjectUnknown) return DeviceTransportType::Unknown;

    UInt32 transport = 0;
    UInt32 size = sizeof(transport);
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    if (AudioObjectGetPropertyData(dev, &addr, 0, nullptr, &size, &transport) == noErr) {
        if (transport == kAudioDeviceTransportTypeBluetooth || transport == kAudioDeviceTransportTypeBluetoothLE) {
            return DeviceTransportType::Bluetooth;
        } else if (transport == kAudioDeviceTransportTypeUSB) {
            return DeviceTransportType::USB;
        } else if (transport == kAudioDeviceTransportTypeBuiltIn) {
            return DeviceTransportType::BuiltIn;
        } else if (transport == kAudioDeviceTransportTypeHDMI) {
            return DeviceTransportType::HDMI;
        }
    }

    // 关键特征名称保底：如果 transport 汇报为 AutoAggregate 或 0，但设备名称带有蓝牙特征 (AirPods, Bluetooth, Beats 等)，判定为 Bluetooth
    std::string name = getActiveHardwareDeviceName();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    if (lower.find("airpod") != std::string::npos ||
        lower.find("bluetooth") != std::string::npos ||
        lower.find("beats") != std::string::npos ||
        lower.find("wireless") != std::string::npos ||
        lower.find("buds") != std::string::npos ||
        lower.find("headphone") != std::string::npos ||
        name.find("蓝牙") != std::string::npos ||
        name.find("耳机") != std::string::npos) {
        return DeviceTransportType::Bluetooth;
    }

    return DeviceTransportType::Other;
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return DeviceTransportType::BuiltIn;
#else
    return DeviceTransportType::Other;
#endif
}

bool AudioEngine::isCurrentDeviceBluetooth() const {
    return getCurrentDeviceTransport() == DeviceTransportType::Bluetooth;
}

bool AudioEngine::isRealHogActive() const {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (isCurrentDeviceBluetooth()) return false;
    return is_hog_active_.load(std::memory_order_acquire);
#else
    return false;
#endif
}

void AudioEngine::registerDeviceListener() {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    OSStatus err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr, onDefaultDeviceChangedThunk, this);
    if (err == noErr) {
        std::cout << "[AudioEngine] 已注册 CoreAudio 默认输出设备热插拔监听器" << std::endl;
    }
    device_thread_ = std::jthread([this](std::stop_token token) {
        while (!token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Also catches a Bluetooth profile/rate change with the same ID.
            device_change_pending_.exchange(false);
            handleDefaultDeviceChanged();
        }
    });
#endif
}

void AudioEngine::unregisterDeviceListener() {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (device_thread_.joinable()) {
        device_thread_.request_stop();
        device_thread_.join();
    }
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr, onDefaultDeviceChangedThunk, this);
#endif
}

void AudioEngine::notifyOutputDeviceChanged() {
    device_change_pending_.store(true);
}

void AudioEngine::handleDefaultDeviceChanged() {
#if defined(__APPLE__) && TARGET_OS_OSX
    std::lock_guard lock(device_mutex_);

    AudioDeviceID current_dev = getPlaybackOutputDeviceID();
    if (current_dev == kAudioObjectUnknown) return;
    const auto system_device = getSystemDefaultOutputDeviceID();
    if (system_device != last_system_output_device_id_ || current_dev != last_active_device_id_) {
        last_system_output_device_id_ = system_device;
        const auto status = synchronizeSystemOutputWithPlaybackDevice(system_device, current_dev);
        if (status != noErr)
            std::cerr << "[AudioEngine] 无法同步系统默认音频输出: " << status << std::endl;
    }
    if (current_dev == last_active_device_id_ &&
        (!decoder_ || (current_spec_.sample_rate == getActiveHardwareSampleRate() && !sink_->hasError()))) {
        return; // 设备 ID 没变，忽略瞬态重复通知
    }
    last_active_device_id_ = current_dev;

    std::string dev_name = getActiveHardwareDeviceName();
    bool is_bt = isCurrentDeviceBluetooth();
    std::cout << "[AudioEngine] >>> 默认音频输出设备切换为: " << dev_name
              << (is_bt ? " [蓝牙无线设备]" : " [物理硬件设备]") << std::endl;

    if (sink_ && sink_->isOpen() && decoder_) {
        bool was_playing = is_playing_.load(std::memory_order_acquire);
        const auto file = current_filepath_;
        const double position = getCurrentTimeSec();
        const double section_start = section_start_sec_.load();
        const double section_end = section_end_sec_.load();
        stop();
        sink_->close();
        if (openAndPlaySection(file, section_start, section_end)) {
            seek(position);
            if (!was_playing) pause();
            std::cout << "[AudioEngine] 已重新将推流管道绑定至新设备: " << dev_name << std::endl;
        }
    } else {
        releaseHogMode();
        if (sink_) sink_->close();
    }
#endif
}

std::string AudioEngine::getActiveHardwareDeviceName() const {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioDeviceID deviceID = getPlaybackOutputDeviceID();
    if (deviceID != kAudioObjectUnknown) {
        CFStringRef cfName = nullptr;
        UInt32 size = sizeof(cfName);
        AudioObjectPropertyAddress nameAddr = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectGetPropertyData(deviceID, &nameAddr, 0, nullptr, &size, &cfName) == noErr && cfName) {
            char buf[256] = {0};
            CFStringGetCString(cfName, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(cfName);
            if (buf[0] != '\0') {
                return std::string(buf);
            }
        }
    }
    return "系统默认音频输出";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return "iPad / iOS 原生音频输出";
#else
    if (sink_ && sink_->isOpen()) {
        return sink_->getDeviceName();
    }
    return "系统默认音频输出";
#endif
}

uint32_t AudioEngine::getActiveHardwareSampleRate() const {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioDeviceID deviceID = getPlaybackOutputDeviceID();
    if (deviceID != kAudioObjectUnknown) {
        Float64 sr = 0;
        UInt32 srSize = sizeof(sr);
        AudioObjectPropertyAddress srAddr = {
            kAudioDevicePropertyNominalSampleRate,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectGetPropertyData(deviceID, &srAddr, 0, nullptr, &srSize, &sr) == noErr && sr > 0) {
            return static_cast<uint32_t>(sr);
        }
    }
#endif
    return current_spec_.sample_rate > 0 ? current_spec_.sample_rate : 44100;
}

static std::atomic<bool> g_mockExternalDevice{false};

void AudioEngine::setExternalDeviceMock(bool enable) {
    g_mockExternalDevice.store(enable, std::memory_order_relaxed);
}

bool AudioEngine::isExternalDeviceMock() const {
    return g_mockExternalDevice.load(std::memory_order_relaxed);
}

std::string AudioEngine::getDiagnosticsJson() const {
    std::ostringstream out;
    out << "{\"underruns\":" << underrun_count_.load()
        << ",\"clipped_samples\":" << clipped_sample_count_.load()
        << ",\"invalid_samples\":" << invalid_sample_count_.load()
        << ",\"render_callbacks\":" << render_callback_count_.load() << "}";
    return out.str();
}

std::vector<AudioDeviceInfo> AudioEngine::getConnectedAudioDevices() {
    std::vector<AudioDeviceInfo> devices;

#if defined(__APPLE__) && TARGET_OS_OSX
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) == noErr && size > 0) {
        int deviceCount = size / sizeof(AudioDeviceID);
        std::vector<AudioDeviceID> deviceIDs(deviceCount);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, deviceIDs.data()) == noErr) {
            AudioDeviceID defaultOutID = getPlaybackOutputDeviceID();

            for (AudioDeviceID dev : deviceIDs) {
                // 检查设备是否有实际输出流通道
                AudioObjectPropertyAddress streamsAddr = {
                    kAudioDevicePropertyStreams,
                    kAudioObjectPropertyScopeOutput,
                    kAudioObjectPropertyElementMain
                };
                UInt32 streamSize = 0;
                if (AudioObjectGetPropertyDataSize(dev, &streamsAddr, 0, nullptr, &streamSize) != noErr || streamSize == 0) {
                    continue; // 忽略纯输入麦克风等设备
                }

                // 获取硬件设备商业全名
                AudioObjectPropertyAddress nameAddr = {
                    kAudioObjectPropertyName,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                CFStringRef cfName = nullptr;
                UInt32 cfSize = sizeof(cfName);
                std::string devName = "未知音频设备";
                if (AudioObjectGetPropertyData(dev, &nameAddr, 0, nullptr, &cfSize, &cfName) == noErr && cfName) {
                    char buf[256] = {0};
                    CFStringGetCString(cfName, buf, sizeof(buf), kCFStringEncodingUTF8);
                    CFRelease(cfName);
                    if (buf[0] != '\0') devName = buf;
                }

                // 探测物理传输总线
                AudioObjectPropertyAddress transportAddr = {
                    kAudioDevicePropertyTransportType,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                UInt32 transport = 0;
                UInt32 tSize = sizeof(transport);
                std::string transportStr = "External";
                bool isBuiltIn = false;
                if (AudioObjectGetPropertyData(dev, &transportAddr, 0, nullptr, &tSize, &transport) == noErr) {
                    if (transport == kAudioDeviceTransportTypeBuiltIn) {
                        transportStr = "BuiltIn";
                        isBuiltIn = true;
                    } else if (transport == kAudioDeviceTransportTypeUSB) {
                        transportStr = "USB";
                        isBuiltIn = false;
                    } else if (transport == kAudioDeviceTransportTypeBluetooth || transport == kAudioDeviceTransportTypeBluetoothLE) {
                        transportStr = "Bluetooth";
                        isBuiltIn = false;
                    } else if (transport == kAudioDeviceTransportTypeHDMI || transport == kAudioDeviceTransportTypeDisplayPort) {
                        transportStr = "HDMI / DP";
                        isBuiltIn = false;
                    } else {
                        transportStr = "External";
                        isBuiltIn = false;
                    }
                }

                // Monitor audio is not a player output option, even when macOS
                // advertises it as a valid HDMI / DisplayPort audio endpoint.
                if (isDisplayOutputTransport(transport)) continue;

                // 探测该硬件声卡原生支持的最大采样率
                AudioObjectPropertyAddress srAddr = {
                    kAudioDevicePropertyAvailableNominalSampleRates,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                UInt32 srSize = 0;
                uint32_t maxRate = 48000;
                if (AudioObjectGetPropertyDataSize(dev, &srAddr, 0, nullptr, &srSize) == noErr && srSize > 0) {
                    int rangeCount = srSize / sizeof(AudioValueRange);
                    std::vector<AudioValueRange> ranges(rangeCount);
                    if (AudioObjectGetPropertyData(dev, &srAddr, 0, nullptr, &srSize, ranges.data()) == noErr) {
                        for (const auto& r : ranges) {
                            if (r.mMaximum > maxRate) maxRate = static_cast<uint32_t>(r.mMaximum);
                        }
                    }
                }

                AudioObjectPropertyAddress curSrAddr = {
                    kAudioDevicePropertyNominalSampleRate,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                Float64 curRate = 44100;
                UInt32 curSrSize = sizeof(curRate);
                AudioObjectGetPropertyData(dev, &curSrAddr, 0, nullptr, &curSrSize, &curRate);

                AudioDeviceInfo info;
                info.id = std::to_string(dev);
                info.name = devName;
                info.is_builtin = isBuiltIn;
                info.is_default = (dev == defaultOutID);
                info.transport = transportStr;
                info.current_sample_rate = static_cast<uint32_t>(curRate);
                info.max_sample_rate = maxRate;
                info.max_bit_depth = 32;
                info.profile = DacDeviceCatalog::getInstance().matchProfile(devName, isBuiltIn, transportStr, maxRate);

                devices.push_back(info);
            }
        }
    }
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    // iOS / iPadOS 原生音频总线探测
    AudioDeviceInfo builtin;
    builtin.id = "builtin_ipad";
    builtin.name = "iPad 本机解码器";
    builtin.is_builtin = true;
    builtin.is_default = true;
    builtin.transport = "BuiltIn (AOP)";
    builtin.current_sample_rate = 48000;
    builtin.max_sample_rate = 96000;
    builtin.max_bit_depth = 32;
    builtin.profile = DacDeviceCatalog::getIpadBuiltinProfile(48000);
    devices.push_back(builtin);
#elif defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    // Linux / 树莓派 5: 默认直通
    AudioDeviceInfo builtin;
    builtin.id = "alsa_hw_0_0";
    builtin.name = "树莓派 ALSA 直通声卡";
    builtin.is_builtin = true;
    builtin.is_default = true;
    builtin.transport = "BuiltIn (I2S)";
    builtin.current_sample_rate = 192000;
    builtin.max_sample_rate = 192000;
    builtin.max_bit_depth = 32;
    builtin.profile = DacDeviceCatalog::getAlsaBuiltinProfile(192000);
    devices.push_back(builtin);
#endif

    // 确保至少有 1 个本机解码器
    bool hasBuiltin = false;
    bool hasUsbExternal = false;
    for (const auto& d : devices) {
        if (d.is_builtin) hasBuiltin = true;
        if (!d.is_builtin && (d.transport == "USB" || d.name.find("TOPPING") != std::string::npos || d.name.find("DX") != std::string::npos)) {
            hasUsbExternal = true;
        }
    }

    bool needsBuiltinFallback = !hasBuiltin;
#if defined(__APPLE__) && TARGET_OS_OSX
    // macOS must only list real audio hardware, never a synthetic speaker.
    needsBuiltinFallback = false;
#endif
    if (needsBuiltinFallback) {
        AudioDeviceInfo defBuiltin;
#if defined(__APPLE__) && TARGET_OS_IPHONE
        defBuiltin.id = "builtin_ipad";
        defBuiltin.name = "iPad 本机解码器";
        defBuiltin.transport = "BuiltIn (AOP)";
        defBuiltin.profile = DacDeviceCatalog::getIpadBuiltinProfile(48000);
#elif defined(__APPLE__) && TARGET_OS_OSX
        defBuiltin.id = "builtin_apple";
        defBuiltin.name = "Mac 本机解码器";
        defBuiltin.transport = "BuiltIn";
        defBuiltin.profile = DacDeviceCatalog::getAppleBuiltinProfile(96000);
#else
        defBuiltin.id = "builtin_alsa";
        defBuiltin.name = "树莓派 ALSA 直通声卡";
        defBuiltin.transport = "BuiltIn (I2S)";
        defBuiltin.profile = DacDeviceCatalog::getAlsaBuiltinProfile(192000);
#endif
        defBuiltin.is_builtin = true;
        defBuiltin.is_default = devices.empty();
        defBuiltin.current_sample_rate = 48000;
        defBuiltin.max_sample_rate = 96000;
        defBuiltin.max_bit_depth = 32;
        devices.insert(devices.begin(), defBuiltin);
    }

    // 若当前未插入物理 USB 外接解码器，默认智能连接模拟 TOPPING DX12 (ES9039Q2M)
    // 使得发烧友在购买前即可即时体验与调校全部参数，购买插上后则自动平滑切换为物理机！
    if (!hasUsbExternal && g_mockExternalDevice.load(std::memory_order_relaxed)) {
        AudioDeviceInfo topping;
        topping.id = "usb_topping_dx12";
        topping.name = "TOPPING DX12";
        topping.is_builtin = false;
        topping.is_default = false;
        topping.transport = "USB";
        topping.current_sample_rate = 384000;
        topping.max_sample_rate = 384000;
        topping.max_bit_depth = 32;
        topping.profile = DacDeviceCatalog::getToppingDx12Profile();
        devices.push_back(topping);
    }

    return devices;
}

std::string AudioEngine::getConnectedDevicesJson() {
    auto list = getConnectedAudioDevices();
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        ss << list[i].toJson();
        if (i + 1 < list.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

bool AudioEngine::selectOutputDevice(const std::string& device_id) {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (device_id == "usb_topping_dx12") {
        return true;
    }
    try {
        AudioDeviceID devID = static_cast<AudioDeviceID>(std::stoul(device_id));
        AudioObjectPropertyAddress addr = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 size = sizeof(devID);
        if (AudioObjectSetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, size, &devID) == noErr) {
            handleDefaultDeviceChanged();
            return true;
        }
    } catch (...) {}
#endif
    return false;
}

void AudioEngine::releaseHogMode() {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioObjectPropertyScope scopes[] = {
        kAudioObjectPropertyScopeGlobal,
        kAudioDevicePropertyScopeOutput
    };

    auto clearHog = [&](AudioDeviceID dev) {
        if (dev == kAudioObjectUnknown) return;
        for (auto sc : scopes) {
            AudioObjectPropertyAddress hogAddr = {
                kAudioDevicePropertyHogMode,
                sc,
                kAudioObjectPropertyElementMain
            };
            if (AudioObjectHasProperty(dev, &hogAddr)) {
                pid_t reset_pid = -1;
                AudioObjectSetPropertyData(dev, &hogAddr, 0, nullptr, sizeof(reset_pid), &reset_pid);
            }
        }
    };

    if (hogged_device_id_ != 0) {
        clearHog(static_cast<AudioDeviceID>(hogged_device_id_));
        hogged_device_id_ = 0;
    }
    is_hog_active_.store(false, std::memory_order_release);
#endif
}

void AudioEngine::applyHogMode(bool enable) {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (!enable) {
        releaseHogMode();
        return;
    }

    // 关键防护：绝不可对 HDMI、DisplayPort、蓝牙或内置扬声器申请 Hog Mode！
    // 只有在检测到外接物理 USB DAC 且用户主动开启独占模式时，才允许申请硬件独占
    DeviceTransportType transport = getCurrentDeviceTransport();
    if (transport != DeviceTransportType::USB) {
        releaseHogMode();
        return;
    }

    AudioDeviceID deviceID = getPlaybackOutputDeviceID();
    if (deviceID == kAudioObjectUnknown) {
        releaseHogMode();
        return;
    }

    AudioObjectPropertyScope scopes[] = {
        kAudioObjectPropertyScopeGlobal,
        kAudioDevicePropertyScopeOutput
    };

    bool has_hog = false;
    for (auto sc : scopes) {
        AudioObjectPropertyAddress hogAddr = {
            kAudioDevicePropertyHogMode,
            sc,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectHasProperty(deviceID, &hogAddr)) {
            has_hog = true;
            break;
        }
    }
    if (!has_hog) {
        releaseHogMode();
        return;
    }

    pid_t pid = getpid();
    bool set_success = false;
    for (auto sc : scopes) {
        AudioObjectPropertyAddress hogAddr = {
            kAudioDevicePropertyHogMode,
            sc,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectHasProperty(deviceID, &hogAddr)) {
            if (AudioObjectSetPropertyData(deviceID, &hogAddr, 0, nullptr, sizeof(pid), &pid) == noErr) {
                set_success = true;
            }
        }
    }

    if (set_success) {
        hogged_device_id_ = deviceID;
        is_hog_active_.store(true, std::memory_order_release);
        std::cout << "[AudioEngine] 【Hog Mode 真实硬件独占成功】已锁定声卡 (PID: " + std::to_string(pid) + ")" << std::endl;
    } else {
        std::cerr << "[AudioEngine] 设置 Hog Mode 失败" << std::endl;
        releaseHogMode();
    }
#else
    (void)enable;
    is_hog_active_.store(enable, std::memory_order_release);
#endif
}

void AudioEngine::applyHardwareSampleRate(uint32_t sample_rate) {
    // This is a preference, not permission to re-clock a live shared device.
    // The decoder converts to the device's negotiated rate on open / reroute.
    (void)sample_rate;
}


void AudioEngine::setEqEnabled(bool enabled) {
    is_eq_enabled_.store(enabled, std::memory_order_release);
}

bool AudioEngine::isEqEnabled() const {
    return is_eq_enabled_.load(std::memory_order_acquire);
}

void AudioEngine::setEqBands(const std::array<float, 10>& gains_db) {
    for (size_t i = 0; i < 10; ++i) {
        eq_gains_[i].store(std::isfinite(gains_db[i]) ? std::clamp(gains_db[i], -12.0f, 12.0f) : 0.0f,
                           std::memory_order_relaxed);
    }
}

std::array<float, 10> AudioEngine::getEqBands() const {
    std::array<float, 10> res{};
    for (size_t i = 0; i < 10; ++i) {
        res[i] = eq_gains_[i].load(std::memory_order_relaxed);
    }
    return res;
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

void AudioEngine::startFadeIn(float duration_sec) {
    fade_duration_sec_.store(duration_sec, std::memory_order_release);
    uint32_t rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100;
    uint64_t total_frames = static_cast<uint64_t>(rate * duration_sec);
    if (total_frames == 0) total_frames = 1;
    fade_total_frames_.store(total_frames, std::memory_order_release);
    fade_current_frame_.store(0, std::memory_order_release);
    fade_out_completed_.store(false, std::memory_order_release);
    fade_state_.store(FadeState::FadeIn, std::memory_order_release);
}

void AudioEngine::startFadeOut(float duration_sec) {
    fade_duration_sec_.store(duration_sec, std::memory_order_release);
    uint32_t rate = (current_spec_.sample_rate > 0) ? current_spec_.sample_rate : 44100;
    uint64_t total_frames = static_cast<uint64_t>(rate * duration_sec);
    if (total_frames == 0) total_frames = 1;
    fade_total_frames_.store(total_frames, std::memory_order_release);
    fade_current_frame_.store(0, std::memory_order_release);
    fade_out_completed_.store(false, std::memory_order_release);
    fade_state_.store(FadeState::FadeOut, std::memory_order_release);
}

bool AudioEngine::isFadeOutCompleted() const {
    return fade_out_completed_.load(std::memory_order_acquire);
}

void AudioEngine::resetFade() {
    fade_state_.store(FadeState::None, std::memory_order_release);
    fade_current_frame_.store(0, std::memory_order_release);
    fade_out_completed_.store(false, std::memory_order_release);
}

void AudioEngine::setHardwareBufferSize(uint32_t frames) {
    std::lock_guard lock(device_mutex_);
#if defined(__APPLE__) && TARGET_OS_IPHONE
    // iPadOS 的“硬件缓冲深度”属于 AVAudioSession，而 AudioQueue 自身的
    // 推流缓冲必须保持为 open() 时的分配尺寸。在线改写后者会让回调尺寸
    // 与已分配内存/队列状态不一致，最终造成爆音或整个队列断粮。
    if (HifiPadSetPreferredIOBufferFrames && HifiPadSetPreferredIOBufferFrames(frames)) {
        return;
    }
#endif
    if (sink_) {
        sink_->setBufferSize(frames);
    }
}

uint32_t AudioEngine::getHardwareBufferSize() const {
    return sink_ ? sink_->getBufferSize() : 1024;
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
    constexpr size_t CHUNK_FRAMES = 8192;
    std::vector<float> decode_buf(CHUNK_FRAMES * current_spec_.channels);

#if defined(__APPLE__) && TARGET_OS_IPHONE
    uint64_t reported_underflows = 0;
    auto next_underflow_report = std::chrono::steady_clock::now() + std::chrono::seconds(2);
#endif

    while (!stop_token.stop_requested() && is_decoding_.load(std::memory_order_acquire)) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
        auto now = std::chrono::steady_clock::now();
        if (now >= next_underflow_report) {
            uint64_t count = g_iPadSinkUnderflowCount.load(std::memory_order_relaxed);
            if (count != reported_underflows) {
                std::cerr << "[AudioEngine][iPad] AudioQueue underflow count=" << count
                          << ", new=" << (count - reported_underflows)
                          << ", bufferedSamples=" << ring_buffer_.available_read() << std::endl;
                reported_underflows = count;
            }
            next_underflow_report = now + std::chrono::seconds(2);
        }
#endif

        // 如果无锁环形缓冲区剩余空间不足 1 个 Block，休眠 5 毫秒等待声卡消费，避免突发忙轮询
        if (ring_buffer_.available_write() < (CHUNK_FRAMES * current_spec_.channels)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        size_t frames_to_request = CHUNK_FRAMES;
        if (is_section_active_.load(std::memory_order_relaxed)) {
            uint64_t total = section_total_frames_.load(std::memory_order_relaxed);
            uint64_t already = section_frames_read_.load(std::memory_order_relaxed);
            if (already >= total) {
                if (!is_eof_.load(std::memory_order_relaxed)) {
                    std::cout << "[AudioEngine] CUE 分轨区间解码完成 (EOF), 已解码帧=" << already << std::endl;
                }
                is_eof_.store(true, std::memory_order_release);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            frames_to_request = static_cast<size_t>(std::min<uint64_t>(CHUNK_FRAMES, total - already));
        }

        uint64_t frames_read = 0;
        {
            std::lock_guard lock(decoder_mutex_);
            if (!decoder_) break;
            frames_read = decoder_->readFrames(decode_buf.data(), frames_to_request);
        }

        if (frames_read > 0) {
            section_frames_read_.fetch_add(frames_read, std::memory_order_relaxed);
            size_t samples_to_write = frames_read * current_spec_.channels;
            ring_buffer_.write(decode_buf.data(), samples_to_write);
        } else {
            // 解码完毕 (EOF)
            if (!is_eof_.load(std::memory_order_relaxed)) {
                std::cout << "[AudioEngine] decodeWorker: reached EOF, total frames consumed="
                          << frames_consumed_by_sink_.load() << std::endl;
            }
            is_eof_.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void AudioEngine::onSinkDataNeeded(float* output, size_t frame_count) {
    render_callback_count_.fetch_add(1, std::memory_order_relaxed);
    size_t samples_needed = frame_count * current_spec_.channels;
    size_t samples_read = ring_buffer_.read(output, samples_needed);

    // 若环形缓冲未填满 (或处于末尾)，平滑渐隐防止方波突变引起的爆音/沙沙声
    if (samples_read < samples_needed) {
        if (!is_eof_.load(std::memory_order_relaxed)) underrun_count_.fetch_add(1, std::memory_order_relaxed);
#if defined(__APPLE__) && TARGET_OS_IPHONE
        g_iPadSinkUnderflowCount.fetch_add(1, std::memory_order_relaxed);
#endif
        float last_val = (samples_read > 0) ? output[samples_read - 1] : 0.0f;
        size_t ramp_len = std::min(samples_needed - samples_read, size_t(32));
        for (size_t i = 0; i < ramp_len; ++i) {
            float decay = 1.0f - static_cast<float>(i + 1) / static_cast<float>(ramp_len);
            output[samples_read + i] = last_val * decay;
        }
        if (samples_needed > samples_read + ramp_len) {
            std::memset(output + samples_read + ramp_len, 0, (samples_needed - samples_read - ramp_len) * sizeof(float));
        }
    }

    size_t frames_read = samples_read / current_spec_.channels;
    frames_consumed_by_sink_.fetch_add(frames_read, std::memory_order_relaxed);

    // 音量与 Bit-Perfect 控制处理
    bool muted = is_muted_.load(std::memory_order_acquire);
    float vol = volume_.load(std::memory_order_acquire);

    // 10段发烧级图形均衡器 RBJ Audio EQ 二阶 IIR 滤波处理 (支持硬件 Direct 直通)
    if (!muted) {
        applyEqualizer(output, frame_count);
    }

    FadeState f_state = fade_state_.load(std::memory_order_acquire);
    bool fade_out_done = fade_out_completed_.load(std::memory_order_acquire);

    // 实时频谱分析更新：提取衰减前的原生音频流，并结合等响度音量感知系数
    // 既真实反映曲目本身的爆发力与动态节拍，又能实时跟随音量滑块与静音状态灵敏联动
    if (!muted && !fade_out_done && samples_read > 0) {
        updateSpectrumAnalysis(output, frame_count);
    } else {
        updateSpectrumAnalysis(nullptr, 0);
    }

    if (muted || fade_out_done) {
        std::memset(output, 0, samples_needed * sizeof(float));
    } else if (f_state == FadeState::None && vol >= 0.9999f && !is_eq_enabled_.load(std::memory_order_relaxed)) {
        // 【Bit-Perfect 0dB 源码直出】：不进行任何浮点乘法计算，保持原始数据绝对纯净！
    } else {
        // 【发烧级纯净 64-bit 浮点音量衰减 + 人耳等响度对数电位器曲线 (Audio Taper)】
        // 人耳感知声音强度呈对数特性 (dB)，若采用线性乘法会导致 10% 音量依然极其大声。
        // 引入经典 HiFi 对数电位器曲线 (Cubic Taper: vol^3)，10% 对应约 -58dB 细腻微风，50% 对应约 -18dB 惬意聆听，100% 保持 0dB Bit-Perfect 源码直出。
        double audio_taper_vol = (vol >= 0.999f) ? 1.0 : (static_cast<double>(vol) * static_cast<double>(vol) * static_cast<double>(vol));
        double double_vol = audio_taper_vol;
        uint32_t channels = (current_spec_.channels > 0) ? current_spec_.channels : 2;
        uint64_t f_total = fade_total_frames_.load(std::memory_order_relaxed);
        uint64_t f_curr = fade_current_frame_.load(std::memory_order_relaxed);

        for (size_t f = 0; f < frame_count; ++f) {
            float f_gain = 1.0f;
            if (f_state == FadeState::FadeIn) {
                f_curr++;
                float progress = (f_total > 0) ? (static_cast<float>(f_curr) / static_cast<float>(f_total)) : 1.0f;
                if (progress >= 1.0f) {
                    progress = 1.0f;
                    f_gain = 1.0f;
                    fade_state_.store(FadeState::None, std::memory_order_release);
                    f_state = FadeState::None;
                } else {
                    f_gain = 0.5f * (1.0f - std::cos(progress * 3.141592653589793f));
                }
            } else if (f_state == FadeState::FadeOut) {
                f_curr++;
                float progress = (f_total > 0) ? (static_cast<float>(f_curr) / static_cast<float>(f_total)) : 1.0f;
                if (progress >= 1.0f) {
                    progress = 1.0f;
                    f_gain = 0.0f;
                    fade_state_.store(FadeState::None, std::memory_order_release);
                    fade_out_completed_.store(true, std::memory_order_release);
                    f_state = FadeState::None;
                } else {
                    f_gain = 0.5f * (1.0f + std::cos(progress * 3.141592653589793f));
                }
            }

            float final_vol = static_cast<float>(double_vol * static_cast<double>(f_gain));
            for (size_t c = 0; c < channels; ++c) {
                size_t idx = f * channels + c;
                if (idx < samples_needed) {
                    output[idx] *= final_vol;
                }
            }
        }
        fade_current_frame_.store(f_curr, std::memory_order_relaxed);
    }

    // 强制峰值限幅保护 [-1.0f, 1.0f]，杜绝母带超零真实峰值 (True Peak > 0dBFS) 在 DAC/蓝牙转换时溢出爆音
    if (!muted && !fade_out_done) {
        uint64_t clipped = 0, invalid = 0;
        for (size_t i = 0; i < samples_needed; ++i) {
            if (!std::isfinite(output[i])) {
                output[i] = 0;
                ++invalid;
            } else if (std::abs(output[i]) > 1) {
                ++clipped;
            }
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
        clipped_sample_count_.fetch_add(clipped, std::memory_order_relaxed);
        invalid_sample_count_.fetch_add(invalid, std::memory_order_relaxed);
    }

    // 播放完毕检测 (单曲仅触发一次 EOF 回调，防止高频重复调用)
    if (is_eof_.load(std::memory_order_acquire) && ring_buffer_.available_read() == 0) {
        if (!eof_reported_.exchange(true, std::memory_order_acq_rel)) {
            std::cout << "[AudioEngine] onSinkDataNeeded: EOF and buffer drained, triggering eof_callback" << std::endl;
            is_playing_.store(false, std::memory_order_release);
            if (sink_) {
                sink_->pause();
            }
            if (eof_callback_) {
                eof_callback_();
            }
        }
    }
}

void AudioEngine::applyEqualizer(float* samples, size_t frame_count) {
    std::array<float, 10> gains{};
    if (is_eq_enabled_.load(std::memory_order_relaxed)) {
        for (size_t i = 0; i < gains.size(); ++i)
            gains[i] = eq_gains_[i].load(std::memory_order_relaxed);
    }
    equalizer_.process(samples, frame_count, current_spec_.channels,
                       current_spec_.sample_rate, gains);
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
        for (auto& lvl : spectrum_levels_) {
            float cur = lvl.load(std::memory_order_relaxed);
            lvl.store(cur * 0.85f, std::memory_order_relaxed);
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

    float vol = volume_.load(std::memory_order_relaxed);
    // 真实声学音量正比联动：严格按实际音量大小与真实声学能量比例渲染高度，绝不虚高顶满
    float vol_factor = std::clamp(vol, 0.0f, 1.0f);
    if (vol_factor < 0.001f) {
        for (auto& lvl : spectrum_levels_) {
            float cur = lvl.load(std::memory_order_relaxed);
            lvl.store(cur * 0.85f, std::memory_order_relaxed);
        }
        return;
    }

    for (int b = 0; b < 12; ++b) {
        int b_start = band_bins[b][0];
        int b_end = band_bins[b][1];
        float sum_mag = 0.0f;
        for (int k = b_start; k <= b_end; ++k) {
            sum_mag += std::sqrt(re[k] * re[k] + im[k] * im[k]);
        }
        float avg_mag = sum_mag / (b_end - b_start + 1);
        float target = std::clamp(avg_mag * band_weights[b] * 0.15f * vol_factor, 0.0f, 1.0f);

        // 动效弹道：快速起音 (Attack) + 平滑自然衰减 (Decay)
        float cur = spectrum_levels_[b].load(std::memory_order_relaxed);
        if (target > cur) {
            spectrum_levels_[b].store(target, std::memory_order_relaxed);
        } else {
            spectrum_levels_[b].store(cur * 0.85f + target * 0.15f, std::memory_order_relaxed);
        }
    }
}

void AudioEngine::getSpectrumLevels(float* out_levels, size_t count) {
    if (!out_levels || count == 0) return;

    if (!isPlaying()) {
        std::fill(out_levels, out_levels + count, 0.0f);
        return;
    }

    size_t copy_cnt = std::min(count, spectrum_levels_.size());
    for (size_t i = 0; i < copy_cnt; ++i) {
        out_levels[i] = spectrum_levels_[i].load(std::memory_order_relaxed);
    }
    for (size_t i = copy_cnt; i < count; ++i) {
        out_levels[i] = 0.0f;
    }
}

} // namespace audio_engine
