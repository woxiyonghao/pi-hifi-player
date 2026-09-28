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

    AudioFormatSpec getCurrentSpec() const;
    std::string getCurrentFilePath() const;

    // 播放完毕回调 (EOF)
    void setEofCallback(std::function<void()> callback);

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

    void decodeWorker(std::stop_token stop_token);
    void onSinkDataNeeded(float* output, size_t frame_count);
    std::unique_ptr<IAudioDecoder> createDecoderForFile(const std::string& filepath);
};

} // namespace audio_engine
