#include "audio_engine/AudioEngine.hpp"
#include "audio_engine/decoders/FlacDecoder.hpp"
#include "audio_engine/decoders/WavDecoder.hpp"
#include "audio_engine/decoders/Mp3Decoder.hpp"
#include "audio_engine/DsfParser.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
#include "audio_engine/sinks/SdlAudioSink.hpp"
#include "audio_engine/sinks/AlsaAudioSink.hpp"
#else
#include "audio_engine/sinks/AudioQueueSink.hpp"
#endif

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <random>

#if defined(__APPLE__)
#if TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
#endif
#include <AudioToolbox/AudioToolbox.h>
#include <unistd.h>

#if TARGET_OS_OSX
static AudioDeviceID getDefaultOutputDeviceID() {
    AudioDeviceID dev = kAudioObjectUnknown;
    UInt32 size = sizeof(dev);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &dev) == noErr) {
        return dev;
    }
    return kAudioObjectUnknown;
}

static OSStatus onDefaultDeviceChangedThunk(AudioObjectID /*inObjectID*/, UInt32 /*inNumberAddresses*/,
                                           const AudioObjectPropertyAddress /*inAddresses*/[], void* inClientData) {
    auto* self = static_cast<audio_engine::AudioEngine*>(inClientData);
    if (self) {
        self->handleDefaultDeviceChanged();
    }
    return noErr;
}
#endif
#endif

namespace audio_engine {

AudioEngine& AudioEngine::getInstance() {
    static AudioEngine instance;
    return instance;
}

AudioEngine::AudioEngine() : ring_buffer_(262144) {
#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
    // 默认输出驱动：使用跨平台低延迟 SdlAudioSink
    sink_ = std::make_unique<SdlAudioSink>();
#if defined(__APPLE__) && TARGET_OS_OSX
    last_active_device_id_ = getDefaultOutputDeviceID();
#endif
#else
    // iOS / iPadOS 原生低延迟推流驱动
    sink_ = std::make_unique<AudioQueueSink>();
#endif
    registerDeviceListener();
}

AudioEngine::~AudioEngine() {
    stop();
    unregisterDeviceListener();
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
    std::lock_guard lock_dev(device_mutex_);
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

    // 1. 在打开音频设备前，如果是非蓝牙物理硬件且开启了独占，先同步硬件采样率
#if defined(__APPLE__) && TARGET_OS_OSX
    last_active_device_id_ = getDefaultOutputDeviceID();
    if (is_exclusive_mode_.load(std::memory_order_acquire)) {
        applyHardwareSampleRate(current_spec_.sample_rate);
    }
    // 确保打开前释放所有 Hog 独占锁，防止阻碍 SDL 打开设备 (SDL2 检查 HogMode != -1 会直接拒绝)
    releaseHogMode();
#endif

    // 2. 打开硬件输出设备 (根据音频文件的真实采样率和声道动态协商)
    if (!sink_->isOpen() || sink_->getActualSpec().sample_rate != current_spec_.sample_rate) {
        sink_->close();
        if (!sink_->open(current_spec_, [this](float* output, size_t frame_count) {
                onSinkDataNeeded(output, frame_count);
            })) {
            std::cerr << "[AudioEngine] 无法打开硬件输出设备" << std::endl;
            return false;
        }
    }

    // 3. 打开流之后，如果是物理声卡且开启了独占模式，申请真正的 Hog Mode 硬件锁
#if defined(__APPLE__) && TARGET_OS_OSX
    last_active_device_id_ = getDefaultOutputDeviceID();
    if (is_exclusive_mode_.load(std::memory_order_acquire)) {
        applyHogMode(true);
    }
#endif

    is_playing_.store(true, std::memory_order_release);
    is_paused_.store(false, std::memory_order_release);
    is_decoding_.store(true, std::memory_order_release);

    // 预缓冲：在声卡推流前先预解码填充环形缓冲区，彻底消除起播断流与沙沙杂音
    constexpr size_t PREBUFFER_FRAMES = 8192;
    std::vector<float> prebuf(PREBUFFER_FRAMES * current_spec_.channels);
    uint64_t pre_read = 0;
    {
        std::lock_guard lock(decoder_mutex_);
        if (decoder_) {
            pre_read = decoder_->readFrames(prebuf.data(), PREBUFFER_FRAMES);
        }
    }
    if (pre_read > 0) {
        ring_buffer_.write(prebuf.data(), pre_read * current_spec_.channels);
    }

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
        is_eof_.store(false, std::memory_order_relaxed);
        resetFade();
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
    bool soft_clean = !is_muted_.load(std::memory_order_acquire) && 
                      (volume_.load(std::memory_order_acquire) >= 0.9999f) &&
                      !is_eq_enabled_.load(std::memory_order_acquire);
#if defined(__APPLE__) && TARGET_OS_OSX
    if (is_exclusive_mode_.load(std::memory_order_acquire)) {
        return soft_clean;
    }
#endif
    return soft_clean;
}

void AudioEngine::setExclusiveMode(bool exclusive) {
    is_exclusive_mode_.store(exclusive, std::memory_order_release);
#if defined(__APPLE__) && TARGET_OS_OSX
    if (isCurrentDeviceBluetooth()) {
        releaseHogMode();
        return;
    }

    if (exclusive) {
        if (current_spec_.sample_rate > 0) {
            applyHardwareSampleRate(current_spec_.sample_rate);
        }
        if (sink_ && sink_->isOpen() && is_playing_.load(std::memory_order_acquire)) {
            applyHogMode(true);
        }
    } else {
        releaseHogMode();
    }
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
    AudioDeviceID dev = getDefaultOutputDeviceID();
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
#endif
}

void AudioEngine::unregisterDeviceListener() {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr, onDefaultDeviceChangedThunk, this);
#endif
}

void AudioEngine::handleDefaultDeviceChanged() {
#if defined(__APPLE__) && TARGET_OS_OSX
    std::lock_guard lock(device_mutex_);

    AudioDeviceID current_dev = getDefaultOutputDeviceID();
    if (current_dev == kAudioObjectUnknown) return;
    if (current_dev == last_active_device_id_) {
        return; // 设备 ID 没变，忽略瞬态重复通知
    }
    last_active_device_id_ = current_dev;

    std::string dev_name = getActiveHardwareDeviceName();
    bool is_bt = isCurrentDeviceBluetooth();
    std::cout << "[AudioEngine] >>> 默认音频输出设备切换为: " << dev_name 
              << (is_bt ? " [蓝牙无线设备]" : " [物理硬件设备]") << std::endl;

    if (sink_ && sink_->isOpen()) {
        bool was_playing = is_playing_.load(std::memory_order_acquire);
        sink_->stop();
        releaseHogMode();
        sink_->close();
        if (sink_->open(current_spec_, [this](float* output, size_t frame_count) {
                onSinkDataNeeded(output, frame_count);
            })) {
            if (!is_bt && is_exclusive_mode_.load(std::memory_order_acquire)) {
                applyHogMode(true);
            }
            if (was_playing) {
                sink_->start();
            }
            std::cout << "[AudioEngine] 已重新将推流管道绑定至新设备: " << dev_name << std::endl;
        }
    } else {
        releaseHogMode();
    }
#endif
}

std::string AudioEngine::getActiveHardwareDeviceName() const {
#if defined(__APPLE__) && TARGET_OS_OSX
    AudioDeviceID deviceID = getDefaultOutputDeviceID();
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
    AudioDeviceID deviceID = getDefaultOutputDeviceID();
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
    clearHog(getDefaultOutputDeviceID());
    is_hog_active_.store(false, std::memory_order_release);
#endif
}

void AudioEngine::applyHogMode(bool enable) {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (!enable) {
        releaseHogMode();
        return;
    }

    // 蓝牙或 HDMI 绝不可申请物理 Hog Mode (否则会剔除 bluetoothd 导致蓝牙耳机掉线静音)
    DeviceTransportType transport = getCurrentDeviceTransport();
    if (transport == DeviceTransportType::Bluetooth || transport == DeviceTransportType::HDMI) {
        releaseHogMode();
        return;
    }

    AudioDeviceID deviceID = getDefaultOutputDeviceID();
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
#if defined(__APPLE__) && TARGET_OS_OSX
    if (sample_rate == 0) return;

    if (isCurrentDeviceBluetooth()) {
        std::cout << "[AudioEngine] 当前为蓝牙无线设备，跳过硬件时钟切换，保持原生蓝牙协议最佳采样率" << std::endl;
        return;
    }

    AudioDeviceID deviceID = getDefaultOutputDeviceID();
    if (deviceID == kAudioObjectUnknown) return;

    AudioObjectPropertyAddress srRangeAddr = {
        kAudioDevicePropertyAvailableNominalSampleRates,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    if (!AudioObjectHasProperty(deviceID, &srRangeAddr)) return;

    UInt32 rangeSize = 0;
    AudioObjectGetPropertyDataSize(deviceID, &srRangeAddr, 0, nullptr, &rangeSize);
    size_t rangeCount = rangeSize / sizeof(AudioValueRange);
    if (rangeCount == 0) return;

    std::vector<AudioValueRange> ranges(rangeCount);
    if (AudioObjectGetPropertyData(deviceID, &srRangeAddr, 0, nullptr, &rangeSize, ranges.data()) != noErr) return;

    bool supported = false;
    Float64 target_sr = static_cast<Float64>(sample_rate);
    for (const auto& r : ranges) {
        if (target_sr >= r.mMinimum && target_sr <= r.mMaximum) {
            supported = true;
            break;
        }
    }
    if (!supported) {
        std::cout << "[AudioEngine] 当前声卡硬件不支持 " << target_sr << " Hz 硬件切换，保持设备原有最佳采样率" << std::endl;
        return;
    }

    AudioObjectPropertyAddress srAddr = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    Float64 cur_sr = 0;
    UInt32 curSize = sizeof(cur_sr);
    if (AudioObjectGetPropertyData(deviceID, &srAddr, 0, nullptr, &curSize, &cur_sr) == noErr) {
        if (std::abs(cur_sr - target_sr) < 1.0) {
            return;
        }
    }

    bool was_open = sink_ && sink_->isOpen();
    bool was_playing = is_playing_.load(std::memory_order_acquire);
    if (was_open) {
        releaseHogMode();
        sink_->stop();
        sink_->close();
    }

    OSStatus srErr = AudioObjectSetPropertyData(deviceID, &srAddr, 0, nullptr, sizeof(target_sr), &target_sr);
    if (srErr == noErr) {
        std::cout << "[AudioEngine] 成功将硬件 DAC 采样率点对点同步至: " << target_sr << " Hz (Bit-Perfect)" << std::endl;
    }

    if (was_open) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (sink_->open(current_spec_, [this](float* output, size_t frame_count) {
                onSinkDataNeeded(output, frame_count);
            })) {
            if (is_exclusive_mode_.load(std::memory_order_acquire)) {
                applyHogMode(true);
            }
            if (was_playing) {
                sink_->start();
            }
        }
    }
#else
    (void)sample_rate;
#endif
}


void AudioEngine::setEqEnabled(bool enabled) {
    is_eq_enabled_.store(enabled, std::memory_order_release);
}

bool AudioEngine::isEqEnabled() const {
    return is_eq_enabled_.load(std::memory_order_acquire);
}

void AudioEngine::setEqBands(const std::array<float, 10>& gains_db) {
    std::lock_guard lock(eq_mutex_);
    eq_gains_ = gains_db;
}

std::array<float, 10> AudioEngine::getEqBands() const {
    std::lock_guard lock(eq_mutex_);
    return eq_gains_;
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

    // 若环形缓冲未填满 (或处于末尾)，平滑渐隐防止方波突变引起的爆音/沙沙声
    if (samples_read < samples_needed) {
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
    if (is_eq_enabled_.load(std::memory_order_relaxed) && !muted) {
        applyEqualizer(output, frame_count);
    }

    FadeState f_state = fade_state_.load(std::memory_order_acquire);
    bool fade_out_done = fade_out_completed_.load(std::memory_order_acquire);

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

    // 实时频谱分析更新
    updateSpectrumAnalysis(output, frame_count);

    // 播放完毕检测
    if (is_eof_.load(std::memory_order_acquire) && ring_buffer_.available_read() == 0) {
        if (eof_callback_) {
            eof_callback_();
        }
    }
}

struct BiquadCoeffs {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
};

struct BiquadState {
    float s1 = 0.0f;
    float s2 = 0.0f;
};

static constexpr float EQ_FREQS[10] = {
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static BiquadCoeffs calculatePeakingCoeffs(float freq, float sample_rate, float gain_db, float q = 1.414f) {
    if (std::abs(gain_db) < 0.05f) {
        return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }
    float w0 = 2.0f * 3.14159265358979323846f * freq / sample_rate;
    if (w0 >= 3.14159265358979323846f * 0.95f) {
        w0 = 3.14159265358979323846f * 0.95f;
    }
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * q);
    float A = std::pow(10.0f, gain_db / 40.0f);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cos_w0;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cos_w0;
    float a2 = 1.0f - alpha / A;

    float inv_a0 = 1.0f / a0;
    return { b0 * inv_a0, b1 * inv_a0, b2 * inv_a0, a1 * inv_a0, a2 * inv_a0 };
}

void AudioEngine::applyEqualizer(float* samples, size_t frame_count) {
    if (!is_eq_enabled_.load(std::memory_order_relaxed)) return;
    uint32_t channels = current_spec_.channels;
    if (channels == 0 || frame_count == 0) return;

    std::array<float, 10> gains;
    {
        std::lock_guard lock(eq_mutex_);
        gains = eq_gains_;
    }

    bool all_zero = true;
    for (float g : gains) {
        if (std::abs(g) > 0.05f) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) return;

    float sample_rate = (current_spec_.sample_rate > 0) ? static_cast<float>(current_spec_.sample_rate) : 44100.0f;

    BiquadCoeffs coeffs[10];
    bool active[10];
    for (size_t b = 0; b < 10; ++b) {
        if (std::abs(gains[b]) < 0.05f) {
            active[b] = false;
        } else {
            active[b] = true;
            coeffs[b] = calculatePeakingCoeffs(EQ_FREQS[b], sample_rate, gains[b]);
        }
    }

    static thread_local BiquadState eq_states[2][10]{};

    for (size_t f = 0; f < frame_count; ++f) {
        for (uint32_t ch = 0; ch < std::min(channels, 2u); ++ch) {
            float s = samples[f * channels + ch];
            for (size_t b = 0; b < 10; ++b) {
                if (active[b]) {
                    const auto& c = coeffs[b];
                    auto& st = eq_states[ch][b];
                    float out = c.b0 * s + st.s1;
                    st.s1 = c.b1 * s - c.a1 * out + st.s2;
                    st.s2 = c.b2 * s - c.a2 * out;
                    s = out;
                }
            }
            samples[f * channels + ch] = s;
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
