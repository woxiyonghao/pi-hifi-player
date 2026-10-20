#pragma once

#include "audio_engine/AudioFormatSpec.hpp"
#include "audio_engine/LockFreeRingBuffer.hpp"
#include "audio_engine/IAudioDecoder.hpp"
#include "audio_engine/IAudioSink.hpp"
#include "audio_engine/GraphicEqualizer.hpp"

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include "audio_engine/DacDeviceCatalog.hpp"
#include <mutex>
#include <vector>

namespace audio_engine {

/**
 * @brief 发烧级音频引擎顶层调度中心 (AudioEngine)
 * 协调解码工作线程、SPSC 无锁环形缓冲区、声卡推流回调、声卡晶振级时间戳及 Bit-Perfect 音量控制
 */
class AudioEngine {
public:
    static AudioEngine& getInstance();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /**
     * @brief 初始化音频引擎 (创建硬件 Sink)
     */
    bool init();

    /**
     * @brief 打开并开始播放指定路径的音频文件
     */
    bool openAndPlay(const std::string& filepath);

    /**
     * @brief 打开并播放指定音频文件的时间区间 (用于 CUE 虚拟分轨母带播放)
     */
    bool openAndPlaySection(const std::string& filepath, double start_sec, double end_sec);

    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void seek(double target_seconds);

    // 音量与位完美控制 (0.0f ~ 1.0f)
    void setVolume(float volume);
    float getVolume() const;
    void setMuted(bool muted);
    bool isMuted() const;
    bool isBitPerfectDirect() const; // 当前是否处于 100% 源码直通状态

    enum class DeviceTransportType {
        Unknown,
        BuiltIn,
        USB,
        Bluetooth,
        HDMI,
        Other
    };

    // 硬件独占流与 Hog Mode (macOS CoreAudio 原生直通)
    void setExclusiveMode(bool exclusive);
    bool isExclusiveMode() const;
    bool isHogModeActive() const;
    bool isRealHogActive() const;
    DeviceTransportType getCurrentDeviceTransport() const;
    bool isCurrentDeviceBluetooth() const;
    void handleDefaultDeviceChanged();
    void notifyOutputDeviceChanged();
    void applyHardwareSampleRate(uint32_t sample_rate);
    std::string getActiveHardwareDeviceName() const;
    uint32_t getActiveHardwareSampleRate() const;

    // 动态外设识别与硬件 DAC 画像
    std::vector<AudioDeviceInfo> getConnectedAudioDevices();
    std::string getConnectedDevicesJson();
    bool selectOutputDevice(const std::string& device_id);
    void setExternalDeviceMock(bool enable);
    bool isExternalDeviceMock() const;
    std::string getDiagnosticsJson() const;

    // 状态查询
    bool isPlaying() const;
    bool isPaused() const;
    bool isIdle() const;

    // 声卡晶振驱动的精确时间戳 (秒)
    double getCurrentTimeSec() const;
    double getDurationSec() const;
    float getProgress() const;

    // 10段图形均衡器控制 (RBJ Audio EQ 二阶 IIR 滤波)
    void setEqEnabled(bool enabled);
    bool isEqEnabled() const;
    void setEqBands(const std::array<float, 10>& gains_db);
    std::array<float, 10> getEqBands() const;

    AudioFormatSpec getCurrentSpec() const;
    std::string getCurrentFilePath() const;

    // 播放完毕回调 (EOF)
    void setEofCallback(std::function<void()> callback);

    // 实时 12 频段音频频谱振幅分析 (0.0f ~ 1.0f)
    void getSpectrumLevels(float* out_levels, size_t count);

    // 播放淡入淡出核心控制 (平滑 Hann 升余弦滤波)
    enum class FadeState {
        None,
        FadeIn,
        FadeOut
    };
    void startFadeIn(float duration_sec = 0.5f);
    void startFadeOut(float duration_sec = 0.5f);
    bool isFadeOutCompleted() const;
    void resetFade();
    bool isEof() const { return is_eof_.load(std::memory_order_acquire); }
    // Decoder EOF alone may still leave buffered music. Only report completion
    // after the sink has consumed that buffer, never on stop / output rebuild.
    bool hasPlaybackCompleted() const { return eof_reported_.load(std::memory_order_acquire); }

    // 硬件缓冲区深度配置 (64, 256, 512, 1024 帧)
    void setHardwareBufferSize(uint32_t frames);
    uint32_t getHardwareBufferSize() const;

private:
    AudioEngine();
    ~AudioEngine();

    std::unique_ptr<IAudioSink> sink_;
    bool using_ipad_remote_io_{false};
    std::unique_ptr<IAudioDecoder> decoder_;
    std::mutex decoder_mutex_; // 保护解码器切换与 Seek 操作

    LockFreeRingBuffer<float> ring_buffer_;

    std::jthread decode_thread_;
    std::atomic<bool> is_decoding_{false};
    std::atomic<bool> is_eof_{false};

    std::atomic<bool> is_playing_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<float> volume_{0.8f};
    std::atomic<bool> is_muted_{false};

    std::string current_filepath_;
    AudioFormatSpec current_spec_;
    double duration_sec_ = 0.0;

    // CUE 分轨区间控制
    std::atomic<double> section_start_sec_{0.0};
    std::atomic<double> section_end_sec_{0.0};
    std::atomic<bool> is_section_active_{false};
    std::atomic<uint64_t> section_frames_read_{0};
    std::atomic<uint64_t> section_total_frames_{0};

    // 声卡已消费总帧数计数器 (用于晶振级毫秒时间计算)
    std::atomic<uint64_t> frames_consumed_by_sink_{0};
    std::atomic<double> seek_base_time_{0.0};

    std::function<void()> eof_callback_;
    std::atomic<bool> eof_reported_{false};

    // 实时频谱分析状态 (无锁原子阵列，彻底杜绝 CoreAudio 实时线程与 UI 线程锁竞争导致的爆音)
    std::array<std::atomic<float>, 12> spectrum_levels_{};

    // 10段图形均衡器状态 (无锁原子阵列)
    std::atomic<bool> is_eq_enabled_{false};
    std::array<std::atomic<float>, 10> eq_gains_{};
    GraphicEqualizer equalizer_;

    // 淡入淡出实时状态
    std::atomic<FadeState> fade_state_{FadeState::None};
    std::atomic<float> fade_duration_sec_{0.5f};
    std::atomic<uint64_t> fade_total_frames_{0};
    std::atomic<uint64_t> fade_current_frame_{0};
    std::atomic<bool> fade_out_completed_{false};

    // macOS CoreAudio 硬件独占与时钟同步状态
    std::atomic<bool> is_exclusive_mode_{false};
    std::atomic<bool> is_hog_active_{false};
    uint32_t last_active_device_id_{0};
    uint32_t last_system_output_device_id_{0};
    uint32_t hogged_device_id_{0};
    std::recursive_mutex device_mutex_;
    std::jthread device_thread_;
    std::atomic<bool> device_change_pending_{false};
    std::atomic<bool> is_resampling_{false};
    std::atomic<uint64_t> underrun_count_{0};
    std::atomic<uint64_t> clipped_sample_count_{0};
    std::atomic<uint64_t> invalid_sample_count_{0};
    std::atomic<uint64_t> render_callback_count_{0};

    void applyHogMode(bool enable);
    void releaseHogMode();
    void registerDeviceListener();
    void unregisterDeviceListener();

    void decodeWorker(std::stop_token stop_token);
    void onSinkDataNeeded(float* output, size_t frame_count);
    void applyEqualizer(float* samples, size_t frame_count);
    void updateSpectrumAnalysis(const float* samples, size_t frame_count);
    std::unique_ptr<IAudioDecoder> createDecoderForFile(const std::string& filepath);
};

} // namespace audio_engine
