#include "PlayerAdmin.hpp"
#include <random> // 提供随机数引擎供 Shuffle 模式使用

PlayerAdmin::PlayerAdmin() {
    // 构造时保持默认空闲态
}

// ==============================================================================
// 1. 基础播放控制实现
// ==============================================================================
void PlayerAdmin::play() {
    // 必须有在播曲目才能启动播放
    if (current_track_.has_value()) {
        state_ = PlaybackState::Playing;
    }
}

void PlayerAdmin::pause() {
    if (state_ == PlaybackState::Playing) {
        state_ = PlaybackState::Paused;
    }
}

void PlayerAdmin::togglePlayPause() {
    if (state_ == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void PlayerAdmin::stop() {
    state_ = PlaybackState::Idle;
    current_time_sec_ = 0.0;
}

// ==============================================================================
// 2. 曲目与播放队列管理实现
// ==============================================================================
void PlayerAdmin::playTrack(const Track& track) {
    current_track_ = track;
    duration_sec_ = static_cast<double>(track.duration_sec);
    current_time_sec_ = 0.0;
    state_ = PlaybackState::Playing;
}

void PlayerAdmin::playPlaylist(const Playlist& playlist, size_t start_index) {
    current_playlist_ = &playlist;
    const auto& tracks = current_playlist_->getTracks();

    if (tracks.empty()) {
        stop();
        return;
    }

    // 防止越界，截断到有效范围
    current_track_index_ = std::min(start_index, tracks.size() - 1);
    playTrack(tracks[current_track_index_]);
}

void PlayerAdmin::next() {
    if (!current_playlist_ || current_playlist_->getTracks().empty()) {
        return;
    }

    const auto& tracks = current_playlist_->getTracks();
    const size_t total_tracks = tracks.size();

    switch (play_mode_) {
        case PlayMode::LoopSingle: {
            // 单曲循环：回到本首开头重播
            seek(0.0);
            play();
            return;
        }
        case PlayMode::Shuffle: {
            // 随机播放：随机选取一首
            if (total_tracks > 1) {
                size_t rand_idx = current_track_index_;
                while (rand_idx == current_track_index_) {
                    rand_idx = static_cast<size_t>(rand()) % total_tracks;
                }
                current_track_index_ = rand_idx;
            }
            break;
        }
        case PlayMode::Sequence: {
            // 顺序播放：如果已经是最后一首则停止
            if (current_track_index_ + 1 < total_tracks) {
                current_track_index_++;
            } else {
                stop();
                return;
            }
            break;
        }
        case PlayMode::LoopList:
        default: {
            // 列表循环：取模回绕到首曲
            current_track_index_ = (current_track_index_ + 1) % total_tracks;
            break;
        }
    }

    playTrack(tracks[current_track_index_]);
}

void PlayerAdmin::previous() {
    if (!current_playlist_ || current_playlist_->getTracks().empty()) {
        return;
    }

    // 专业播放器人机体验：如果当前歌曲已播放超过 3 秒，按上一首优先回到歌曲开头
    if (current_time_sec_ > 3.0) {
        seek(0.0);
        return;
    }

    const auto& tracks = current_playlist_->getTracks();
    const size_t total_tracks = tracks.size();

    // 回退到上一首（若在第 0 首则回绕到最后一首）
    if (current_track_index_ == 0) {
        current_track_index_ = total_tracks - 1;
    } else {
        current_track_index_--;
    }

    playTrack(tracks[current_track_index_]);
}

// ==============================================================================
// 3. 时间轴与进度步进实现
// ==============================================================================
void PlayerAdmin::seek(double target_sec) {
    current_time_sec_ = std::clamp(target_sec, 0.0, duration_sec_);
}

void PlayerAdmin::update(double delta_time) {
    // 只有处于正在播放状态才向前推进时间
    if (state_ == PlaybackState::Playing) {
        current_time_sec_ += delta_time;

        // 播放完毕检测：自动触发切到下一首
        if (duration_sec_ > 0.0 && current_time_sec_ >= duration_sec_) {
            next();
        }
    }
}

// ==============================================================================
// 4. 循环模式切换实现
// ==============================================================================
void PlayerAdmin::cyclePlayMode() {
    switch (play_mode_) {
        case PlayMode::LoopList:   play_mode_ = PlayMode::LoopSingle; break;
        case PlayMode::LoopSingle: play_mode_ = PlayMode::Shuffle;    break;
        case PlayMode::Shuffle:    play_mode_ = PlayMode::Sequence;   break;
        case PlayMode::Sequence:   play_mode_ = PlayMode::LoopList;   break;
    }
}