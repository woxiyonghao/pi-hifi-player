#include "PlayerAdmin.hpp"
#include "imgui.h"
#include <random> // 提供随机数引擎供 Shuffle 模式使用
#include <cmath>

PlayerAdmin::PlayerAdmin() {
    // 注册音频底层 EOF 事件：曲目硬件推流完毕后自动切下一首
    audio_engine::AudioEngine::getInstance().setEofCallback([this]() {
        this->next();
    });
}

// ==============================================================================
// 1. 基础播放控制实现
// ==============================================================================
void PlayerAdmin::play() {
    if (current_track_.has_value()) {
        state_ = PlaybackState::Playing;
        audio_engine::AudioEngine::getInstance().play();
    }
}

void PlayerAdmin::pause() {
    if (state_ == PlaybackState::Playing) {
        state_ = PlaybackState::Paused;
        audio_engine::AudioEngine::getInstance().pause();
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
    audio_engine::AudioEngine::getInstance().stop();
}

// ==============================================================================
// 2. 曲目与播放队列管理实现
// ==============================================================================
void PlayerAdmin::playTrack(const Track& track) {
    current_track_ = track;
    duration_sec_ = static_cast<double>(track.duration_sec);
    current_time_sec_ = 0.0;
    state_ = PlaybackState::Playing;

    if (!track.file_path.empty()) {
        bool ok = audio_engine::AudioEngine::getInstance().openAndPlay(track.file_path);
        if (ok) {
            double engine_dur = audio_engine::AudioEngine::getInstance().getDurationSec();
            if (engine_dur > 0.0) {
                duration_sec_ = engine_dur;
            }
        }
    }
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
    if (getCurrentTimeSec() > 3.0) {
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
    current_time_sec_ = std::clamp(target_sec, 0.0, getDurationSec());
    audio_engine::AudioEngine::getInstance().seek(current_time_sec_);
}

void PlayerAdmin::update(double delta_time) {
    auto& engine = audio_engine::AudioEngine::getInstance();
    if (engine.isPlaying()) {
        current_time_sec_ = engine.getCurrentTimeSec();
    } else if (state_ == PlaybackState::Playing) {
        // 当播放在线/模拟无物理文件的曲目时，继续作为虚拟走表兜底
        current_time_sec_ += delta_time;
        if (duration_sec_ > 0.0 && current_time_sec_ >= duration_sec_) {
            next();
        }
    }
}

double PlayerAdmin::getCurrentTimeSec() const {
    auto& engine = audio_engine::AudioEngine::getInstance();
    if (engine.isPlaying() || engine.isPaused()) {
        return engine.getCurrentTimeSec();
    }
    return current_time_sec_;
}

double PlayerAdmin::getDurationSec() const {
    auto& engine = audio_engine::AudioEngine::getInstance();
    if ((engine.isPlaying() || engine.isPaused()) && engine.getDurationSec() > 0.0) {
        return engine.getDurationSec();
    }
    return duration_sec_;
}

// ==============================================================================
// 4. 音量与发烧硬件控制实现
// ==============================================================================
void PlayerAdmin::setVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    audio_engine::AudioEngine::getInstance().setVolume(volume_);
}

float PlayerAdmin::getVolume() const {
    return is_muted_ ? 0.0f : volume_;
}

void PlayerAdmin::toggleMute() {
    is_muted_ = !is_muted_;
    audio_engine::AudioEngine::getInstance().setMuted(is_muted_);
}

bool PlayerAdmin::isMuted() const {
    return is_muted_;
}

bool PlayerAdmin::isBitPerfectDirect() const {
    return audio_engine::AudioEngine::getInstance().isBitPerfectDirect();
}

// ==============================================================================
// 5. 循环模式切换实现
// ==============================================================================
void PlayerAdmin::cyclePlayMode() {
    switch (play_mode_) {
        case PlayMode::LoopList:   play_mode_ = PlayMode::LoopSingle; break;
        case PlayMode::LoopSingle: play_mode_ = PlayMode::Shuffle;    break;
        case PlayMode::Shuffle:    play_mode_ = PlayMode::Sequence;   break;
        case PlayMode::Sequence:   play_mode_ = PlayMode::LoopList;   break;
    }
}

// ==============================================================================
// 6. 实时频谱振幅分析获取
// ==============================================================================
void PlayerAdmin::getSpectrumLevels(float* out_levels, size_t count) const {
    if (!out_levels || count == 0) return;

    // 如果未播放，严格清零 (显示 0 个方块)
    if (!isPlaying()) {
        std::fill(out_levels, out_levels + count, 0.0f);
        return;
    }

    auto& engine = audio_engine::AudioEngine::getInstance();
    if (engine.isPlaying()) {
        engine.getSpectrumLevels(out_levels, count);
        return;
    }

    // 虚拟模拟数据环境下的拟真律动频谱 (供未导入实体音乐文件时的 UI 验证)
    static const float base_weights[12] = {
        0.85f, 0.70f, 0.90f, 0.60f, 0.75f, 0.50f,
        0.65f, 0.80f, 0.55f, 0.70f, 0.45f, 0.60f
    };
    static float s_smooth_levels[12] = {0.0f};
    float t = static_cast<float>(ImGui::GetTime());
    for (size_t i = 0; i < count; ++i) {
        float freq = 4.2f + (i % 3) * 1.8f;
        float wave = std::abs(std::sin(t * freq + i * 0.9f)) * 0.65f + 
                     std::abs(std::cos(t * (freq * 0.6f) - i * 1.3f)) * 0.35f;
        float w = (i < 12) ? base_weights[i] : 0.6f;
        float target = std::clamp(wave * w, 0.05f, 0.95f);
        if (i < 12) {
            s_smooth_levels[i] = s_smooth_levels[i] * 0.75f + target * 0.25f;
            out_levels[i] = s_smooth_levels[i];
        } else {
            out_levels[i] = target;
        }
    }
}