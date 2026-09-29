#pragma once

#include "audio_engine/AudioFormatSpec.hpp"
#include "audio_engine/LockFreeRingBuffer.hpp"
#include "audio_engine/IAudioDecoder.hpp"
#include "audio_engine/IAudioSink.hpp"

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>

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

    // 硬件缓冲区深度配置 (64, 256, 512, 1024 帧)
    void setHardwareBufferSize(uint32_t frames);
    uint32_t getHardwareBufferSize() const;

private:
    AudioEngine();
    ~AudioEngine();

    std::unique_ptr<IAudioSink> sink_;
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

    // 声卡已消费总帧数计数器 (用于晶振级毫秒时间计算)
    std::atomic<uint64_t> frames_consumed_by_sink_{0};
    std::atomic<double> seek_base_time_{0.0};

    std::function<void()> eof_callback_;

    // 实时频谱分析状态
    std::array<float, 12> spectrum_levels_{};
    mutable std::mutex spectrum_mutex_;

    // 10段图形均衡器状态
    std::atomic<bool> is_eq_enabled_{false};
    std::array<float, 10> eq_gains_{};
    mutable std::mutex eq_mutex_;

    // 淡入淡出实时状态
    std::atomic<FadeState> fade_state_{FadeState::None};
    std::atomic<float> fade_duration_sec_{0.5f};
    std::atomic<uint64_t> fade_total_frames_{0};
    std::atomic<uint64_t> fade_current_frame_{0};
    std::atomic<bool> fade_out_completed_{false};

    void decodeWorker(std::stop_token stop_token);
    void onSinkDataNeeded(float* output, size_t frame_count);
    void applyEqualizer(float* samples, size_t frame_count);
    void updateSpectrumAnalysis(const float* samples, size_t frame_count);
    std::unique_ptr<IAudioDecoder> createDecoderForFile(const std::string& filepath);
};

} // namespace audio_engine
