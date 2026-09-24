
#pragma once
#include "types/MusicModel.hpp"
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <algorithm>

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
    // 切歌控制 (根据当前的 PlayMode 决定下一首逻辑)
    void next();
    void previous();
    // -------------------------------------------------------------------------
    // [时间与进度控制]
    // -------------------------------------------------------------------------
    // 跳转至指定秒数 (Seek)
    void seek(double target_sec);
    // 帧驱动心跳更新 (在 60fps 主循环中推进模拟播放进度)
    void update(double delta_time);
    double getCurrentTimeSec() const { return current_time_sec_; }
    double getDurationSec() const { return duration_sec_; }
    // 获取当前播放百分比 [0.0f, 1.0f]，直接供 UI 进度条绘制
    float getProgress() const {
        return (duration_sec_ > 0.0) ? static_cast<float>(current_time_sec_ / duration_sec_) : 0.0f;
    }
    // -------------------------------------------------------------------------
    // [音量与发烧硬件控制 (0.0f ~ 1.0f)]
    // -------------------------------------------------------------------------
    void setVolume(float volume) {
        volume_ = std::clamp(volume, 0.0f, 1.0f);
    }
    float getVolume() const { return is_muted_ ? 0.0f : volume_; }
    float getRawVolume() const { return volume_; } // 获取静音前的原始音量
    void toggleMute() { is_muted_ = !is_muted_; }
    bool isMuted() const { return is_muted_; }
    // -------------------------------------------------------------------------
    // [播放模式切换]
    // -------------------------------------------------------------------------
    PlayMode getPlayMode() const { return play_mode_; }
    void setPlayMode(PlayMode mode) { play_mode_ = mode; }
    void cyclePlayMode(); // 循环切换模式: 列表循环 -> 单曲循环 -> 随机 -> 顺序
    // -------------------------------------------------------------------------
    // [状态与当前曲目只读查询 (供 BottomBar / VU表头 / 侧边栏读取)]
    // -------------------------------------------------------------------------
    PlaybackState getState() const { return state_; }
    bool isPlaying() const { return state_ == PlaybackState::Playing; }
    bool isPaused() const { return state_ == PlaybackState::Paused; }
    // 使用 C++17/20 std::optional 优雅表达“可能有歌曲，也可能为空”
    const std::optional<Track>& getCurrentTrack() const { return current_track_; }
private:
    // 核心状态
    PlaybackState state_ = PlaybackState::Idle;
    PlayMode play_mode_  = PlayMode::LoopList;
    // 当前在播曲目与歌单队列引用
    std::optional<Track> current_track_ = std::nullopt;
    const Playlist* current_playlist_   = nullptr;
    size_t current_track_index_         = 0;
    // 时间轴 (单位: 秒)
    double current_time_sec_ = 0.0;
    double duration_sec_     = 0.0;
    // 音量状态
    float volume_   = 0.8f; // 默认 80% 舒适音量
    bool is_muted_  = false;
    PlayerAdmin();
};