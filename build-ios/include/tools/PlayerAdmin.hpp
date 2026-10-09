
#pragma once
#include "types/MusicModel.hpp"
#include "audio_engine/AudioEngine.hpp"
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <atomic>

// ==============================================================================
// 1. 播放引擎核心状态枚举
// ==============================================================================
enum class PlaybackState {
    Idle,       // 空闲/停止：无曲目载入或播放停止
    Loading,    // 缓冲/解封装中：切换大型 DSD/母带或抓轨预加载
    Playing,    // 正在播放中
    Paused,     // 已暂停播放
    Error       // 异常状态：文件缺失或解码失败
};
// ==============================================================================
// 2. 播放循环模式枚举
// ==============================================================================
enum class PlayMode {
    LoopList,   // 列表循环 (默认)
    LoopSingle, // 单曲循环
    Shuffle,    // 随机播放
    Sequence    // 顺序播放 (播完列表即停止)
};

// ==============================================================================
// 3. 播放核心管理中枢类 (PlayerAdmin)
// ==============================================================================
class PlayerAdmin {
public:

    // 1. 提供全局唯一访问入口 (C++11/20 保证静态局部变量线程安全)
    static PlayerAdmin& getInstance() {
        static PlayerAdmin instance;
        return instance;
    }
    // 2. 严格杜绝拷贝与赋值 (单例防克隆)
    PlayerAdmin(const PlayerAdmin&) = delete;
    PlayerAdmin& operator=(const PlayerAdmin&) = delete;
    PlayerAdmin(PlayerAdmin&&) = delete;
    PlayerAdmin& operator=(PlayerAdmin&&) = delete;

    ~PlayerAdmin() = default;
    // -------------------------------------------------------------------------
    // [基础播放控制指令]
    // -------------------------------------------------------------------------
    void play();
    void pause();
    void togglePlayPause(); // 播放/暂停一键翻转 (对应键盘空格键或 EC11 旋钮按压)
    void stop();
    // -------------------------------------------------------------------------
    // [曲目与队列管理]
    // -------------------------------------------------------------------------
    // 播放指定单曲
    void playTrack(const Track& track);
    // 载入歌单并从指定索引开始播放 (默认从第 0 首开始)
    void playPlaylist(const Playlist& playlist, size_t start_index = 0);
    // 载入歌曲列表并从指定索引开始播放
    void playTracks(const std::vector<Track>& tracks, size_t start_index = 0);
    // 获取当前播放队列
    const std::vector<Track>& getPlaybackQueue() const { return playback_queue_; }
    size_t getCurrentTrackIndex() const { return current_track_index_; }
    void playQueueIndex(size_t index);
    void addToQueue(const Track& track);
    void removeTrackFromQueue(size_t index);
    void clearQueue();
    // 切歌控制 (根据当前的 PlayMode 决定下一首逻辑)
    void next();
    void previous();
    void replayCurrentTrack();
    // -------------------------------------------------------------------------
    // [时间与进度控制]
    // -------------------------------------------------------------------------
    // 跳转至指定秒数 (Seek)
    void seek(double target_sec);
    // 帧驱动心跳更新 (在 60fps 主循环中推进模拟播放进度)
    void update(double delta_time);
    double getCurrentTimeSec() const;
    double getDurationSec() const;
    // 获取当前播放百分比 [0.0f, 1.0f]，直接供 UI 进度条绘制
    float getProgress() const {
        double d = getDurationSec();
        return (d > 0.0) ? static_cast<float>(getCurrentTimeSec() / d) : 0.0f;
    }
    // -------------------------------------------------------------------------
    // [音量与发烧硬件控制 (0.0f ~ 1.0f)]
    // -------------------------------------------------------------------------
    void setVolume(float volume);
    float getVolume() const;
    float getRawVolume() const { return volume_; } // 获取静音前的原始音量
    void toggleMute();
    bool isMuted() const;
    bool isBitPerfectDirect() const; // 源码直通状态 (100% 音量且未静音)
    // -------------------------------------------------------------------------
    // [播放模式切换]
    // -------------------------------------------------------------------------
    PlayMode getPlayMode() const { return play_mode_; }
    void setPlayMode(PlayMode mode);
    void cyclePlayMode(); // 循环切换模式: 列表循环 -> 单曲循环 -> 随机 -> 顺序
    // -------------------------------------------------------------------------
    // [状态与当前曲目只读查询 (供 BottomBar / VU表头 / 侧边栏读取)]
    // -------------------------------------------------------------------------
    PlaybackState getState() const { return state_; }
    bool isPlaying() const { return state_ == PlaybackState::Playing; }
    bool isPaused() const { return state_ == PlaybackState::Paused; }
    // 使用 C++17/20 std::optional 优雅表达“可能有歌曲，也可能为空”
    const std::optional<Track>& getCurrentTrack() const { return current_track_; }

    // -------------------------------------------------------------------------
    // [10段图形均衡器控制 (RBJ Audio EQ 二阶 IIR 滤波)]
    // -------------------------------------------------------------------------
    void setEqEnabled(bool enabled);
    bool isEqEnabled() const;
    void setEqBands(const std::array<float, 10>& gains_db);
    std::array<float, 10> getEqBands() const;

    // 获取实时音频 12 频段振幅包络 (0.0f ~ 1.0f)
    void getSpectrumLevels(float* out_levels, size_t count = 12) const;

    // -------------------------------------------------------------------------
    // [切歌平滑过渡控制 (淡入淡出)]
    // -------------------------------------------------------------------------
    void setFadeDuration(float sec);
    float getFadeDuration() const { return fade_duration_sec_; }
    bool isTransitioning() const { return is_transitioning_; }

private:
    // 核心状态
    PlaybackState state_ = PlaybackState::Idle;
    PlayMode play_mode_  = PlayMode::LoopList;
    // 当前在播曲目与歌单队列引用
    std::optional<Track> current_track_ = std::nullopt;
    const Playlist* current_playlist_   = nullptr;
    std::vector<Track> playback_queue_;
    size_t current_track_index_         = 0;
    // 时间轴 (单位: 秒)
    double current_time_sec_ = 0.0;
    double duration_sec_     = 0.0;
    // 音量状态
    float volume_   = 0.8f; // 默认 80% 舒适音量
    bool is_muted_  = false;

    // 切歌平滑淡入淡出过渡状态
    float fade_duration_sec_ = 0.5f; // 默认 0.5 秒发烧平滑过渡
    bool is_transitioning_ = false;
    double transition_elapsed_ = 0.0;
    std::optional<Track> pending_track_ = std::nullopt;
    std::atomic<bool> eof_pending_{false};

    void switchTrack(const Track& track);
    void executeTrackSwitch(const Track& track);

    PlayerAdmin();

    void saveConfig();
    void loadConfig();
};