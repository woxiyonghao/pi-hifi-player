#include "PlayerAdmin.hpp"
#include "tools/MusicDatabase.hpp"
#include "imgui.h"
#include "public/AppConfig.hpp"
#include <random> // 提供随机数引擎供 Shuffle 模式使用
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

PlayerAdmin::PlayerAdmin() {
    loadConfig();

    // 注册音频底层 EOF 事件：标记 eof_pending_，交由主线程 update() 驱动切歌，防止音频回调死锁
    audio_engine::AudioEngine::getInstance().setEofCallback([this]() {
        this->eof_pending_.store(true, std::memory_order_release);
    });
}

// ==============================================================================
// 1. 基础播放控制实现
// ==============================================================================
void PlayerAdmin::play() {
    if (!current_track_.has_value()) {
        if (!playback_queue_.empty()) {
            playTrack(playback_queue_[current_track_index_]);
        }
        return;
    }
    state_ = PlaybackState::Playing;
    auto& engine = audio_engine::AudioEngine::getInstance();
    if (engine.isPaused()) {
        engine.play();
    } else if (engine.isIdle()) {
        if (!current_track_->file_path.empty()) {
            current_track_->file_path = MusicDatabase::resolveTrackPath(current_track_->file_path);
            engine.openAndPlay(current_track_->file_path);
            if (current_time_sec_ > 0.0) {
                engine.seek(current_time_sec_);
            }
        }
    }
}

void PlayerAdmin::pause() {
    if (is_transitioning_) {
        is_transitioning_ = false;
        pending_track_ = std::nullopt;
    }
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
    is_transitioning_ = false;
    pending_track_ = std::nullopt;
    state_ = PlaybackState::Idle;
    current_time_sec_ = 0.0;
    audio_engine::AudioEngine::getInstance().stop();
}

// ==============================================================================
// 2. 曲目与播放队列管理实现
// ==============================================================================
void PlayerAdmin::playTrack(const Track& track) {
    // 如果正是当前正在播放的曲目，不重复重启声卡
    if (current_track_.has_value() && current_track_->id == track.id && state_ == PlaybackState::Playing) {
        return;
    }

    // 维护当前队列一致性
    if (playback_queue_.empty()) {
        playback_queue_.push_back(track);
        current_track_index_ = 0;
    } else {
        auto it = std::find_if(playback_queue_.begin(), playback_queue_.end(), [&](const Track& t) {
            return t.id == track.id;
        });
        if (it != playback_queue_.end()) {
            current_track_index_ = static_cast<size_t>(std::distance(playback_queue_.begin(), it));
        } else {
            playback_queue_.push_back(track);
            current_track_index_ = playback_queue_.size() - 1;
        }
    }

    // 手动选曲与切歌：立即中止淡出过渡，即时起播新曲，体验零延迟
    is_transitioning_ = false;
    pending_track_ = std::nullopt;
    executeTrackSwitch(track);
}

void PlayerAdmin::switchTrack(const Track& track) {
    auto& engine = audio_engine::AudioEngine::getInstance();

    // 如果开启了平滑淡入淡出 (时长 > 0.05s) 并且当前正在正常播放且未处于末尾 EOF
    if (fade_duration_sec_ > 0.05f && engine.isPlaying() && !engine.isEof() && current_track_.has_value() && current_track_->id != track.id) {
        is_transitioning_ = true;
        transition_elapsed_ = 0.0;
        pending_track_ = track;

        // UI 立即同步呈现新曲目（歌名、封面、艺术家等），人机体验零延迟滞后
        current_track_ = track;
        duration_sec_ = static_cast<double>(track.duration_sec);
        current_time_sec_ = 0.0;

        // 音频底层平滑淡出 (Hann 曲线衰减)
        engine.startFadeOut(fade_duration_sec_);
    } else {
        // 直接执行切换并在启播时平滑淡入
        executeTrackSwitch(track);
    }
}

void PlayerAdmin::executeTrackSwitch(const Track& track) {
    current_track_ = track;
    current_track_->file_path = MusicDatabase::resolveTrackPath(track.file_path);
    duration_sec_ = static_cast<double>(track.duration_sec);
    current_time_sec_ = 0.0;

    if (!current_track_->file_path.empty()) {
        auto& engine = audio_engine::AudioEngine::getInstance();
        bool ok = engine.openAndPlay(current_track_->file_path);
        if (ok) {
            state_ = PlaybackState::Playing;
            double engine_dur = engine.getDurationSec();
            if (engine_dur > 0.0) {
                duration_sec_ = engine_dur;
            }
            if (fade_duration_sec_ > 0.05f) {
                engine.startFadeIn(fade_duration_sec_);
            }
        } else {
            std::cerr << "[PlayerAdmin] 播放失败: 无法打开文件 " << track.file_path << std::endl;
            state_ = PlaybackState::Paused;
        }
    } else {
        state_ = PlaybackState::Playing;
    }
}

void PlayerAdmin::playTracks(const std::vector<Track>& tracks, size_t start_index) {
    playback_queue_ = tracks;
    if (playback_queue_.empty()) {
        stop();
        return;
    }
    current_track_index_ = std::min(start_index, playback_queue_.size() - 1);
    playTrack(playback_queue_[current_track_index_]);
}

void PlayerAdmin::playPlaylist(const Playlist& playlist, size_t start_index) {
    current_playlist_ = &playlist;
    playTracks(playlist.getTracks(), start_index);
}

void PlayerAdmin::next() {
    std::cout << "[PlayerAdmin] next() called! queue_size=" << playback_queue_.size()
              << " cur_idx=" << current_track_index_
              << " play_mode=" << static_cast<int>(play_mode_) << std::endl;
    if (playback_queue_.empty()) {
        return;
    }

    const size_t total_tracks = playback_queue_.size();
    if (total_tracks == 1) {
        std::cout << "[PlayerAdmin] only 1 track in queue, rewinding to 0.0" << std::endl;
        seek(0.0);
        play();
        return;
    }

    switch (play_mode_) {
        case PlayMode::LoopSingle: {
            // 单曲循环：回到本首开头重播
            seek(0.0);
            play();
            return;
        }
        case PlayMode::Shuffle: {
            // 随机播放：随机选取一首不同曲目
            size_t rand_idx = current_track_index_;
            while (rand_idx == current_track_index_) {
                rand_idx = static_cast<size_t>(rand()) % total_tracks;
            }
            current_track_index_ = rand_idx;
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

    playTrack(playback_queue_[current_track_index_]);
}

void PlayerAdmin::previous() {
    if (playback_queue_.empty()) {
        return;
    }

    // 专业播放器人机体验：如果当前歌曲已播放超过 3 秒，按上一首优先回到歌曲开头
    if (getCurrentTimeSec() > 3.0) {
        seek(0.0);
        return;
    }

    const size_t total_tracks = playback_queue_.size();
    if (total_tracks == 1) {
        seek(0.0);
        play();
        return;
    }

    // 回退到上一首（若在第 0 首则回绕到最后一首）
    if (current_track_index_ == 0) {
        current_track_index_ = total_tracks - 1;
    } else {
        current_track_index_--;
    }

    playTrack(playback_queue_[current_track_index_]);
}

// ==============================================================================
// 3. 时间轴与进度步进实现
// ==============================================================================
void PlayerAdmin::seek(double target_sec) {
    std::cout << "[PlayerAdmin] seek called: target_sec=" << target_sec << " (cur_dur=" << getDurationSec() << ")" << std::endl;
    current_time_sec_ = std::clamp(target_sec, 0.0, getDurationSec());
    audio_engine::AudioEngine::getInstance().seek(current_time_sec_);
}

void PlayerAdmin::update(double delta_time) {
    auto& engine = audio_engine::AudioEngine::getInstance();

    // 异步安全执行 EOF 切歌逻辑 (脱离 CoreAudio 音频回调线程，防止死锁)
    if (eof_pending_.exchange(false, std::memory_order_acq_rel)) {
        this->next();
    }

    // 处理切歌淡出完毕后的新曲载入与平滑淡入接力
    if (is_transitioning_) {
        transition_elapsed_ += delta_time;
        if (engine.isFadeOutCompleted() || transition_elapsed_ >= (fade_duration_sec_ + 0.08)) {
            is_transitioning_ = false;
            if (pending_track_.has_value()) {
                Track next_t = pending_track_.value();
                pending_track_ = std::nullopt;
                executeTrackSwitch(next_t);
            }
        }
    }

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
    saveConfig();
}

float PlayerAdmin::getVolume() const {
    return is_muted_ ? 0.0f : volume_;
}

void PlayerAdmin::toggleMute() {
    is_muted_ = !is_muted_;
    audio_engine::AudioEngine::getInstance().setMuted(is_muted_);
    saveConfig();
}

bool PlayerAdmin::isMuted() const {
    return is_muted_;
}

bool PlayerAdmin::isBitPerfectDirect() const {
    return audio_engine::AudioEngine::getInstance().isBitPerfectDirect();
}

void PlayerAdmin::setEqEnabled(bool enabled) {
    audio_engine::AudioEngine::getInstance().setEqEnabled(enabled);
}

bool PlayerAdmin::isEqEnabled() const {
    return audio_engine::AudioEngine::getInstance().isEqEnabled();
}

void PlayerAdmin::setEqBands(const std::array<float, 10>& gains_db) {
    audio_engine::AudioEngine::getInstance().setEqBands(gains_db);
}

std::array<float, 10> PlayerAdmin::getEqBands() const {
    return audio_engine::AudioEngine::getInstance().getEqBands();
}

// ==============================================================================
// 5. 循环模式切换实现
// ==============================================================================
void PlayerAdmin::setPlayMode(PlayMode mode) {
    play_mode_ = mode;
    saveConfig();
}

void PlayerAdmin::cyclePlayMode() {
    switch (play_mode_) {
        case PlayMode::LoopList:   play_mode_ = PlayMode::LoopSingle; break;
        case PlayMode::LoopSingle: play_mode_ = PlayMode::Shuffle;    break;
        case PlayMode::Shuffle:    play_mode_ = PlayMode::Sequence;   break;
        case PlayMode::Sequence:   play_mode_ = PlayMode::LoopList;   break;
    }
    saveConfig();
}

void PlayerAdmin::setFadeDuration(float sec) {
    fade_duration_sec_ = std::clamp(sec, 0.0f, 3.0f);
    saveConfig();
}

void PlayerAdmin::saveConfig() {
    std::string config_dir = AppConfig::Path::getConfigDir();
    std::error_code ec;
    if (!std::filesystem::exists(config_dir, ec)) {
        std::filesystem::create_directories(config_dir, ec);
    }
    std::string file_path = config_dir + "/player_settings.ini";
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) return;

    ofs << "[Player]\n";
    ofs << "play_mode=" << static_cast<int>(play_mode_) << "\n";
    ofs << "volume=" << volume_ << "\n";
    ofs << "muted=" << (is_muted_ ? 1 : 0) << "\n";
    ofs << "fade_duration=" << fade_duration_sec_ << "\n";
}

void PlayerAdmin::loadConfig() {
    std::string file_path = AppConfig::Path::getConfigDir() + "/player_settings.ini";
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        if (key == "play_mode") {
            try {
                int mode_val = std::stoi(val);
                if (mode_val >= 0 && mode_val <= 3) {
                    play_mode_ = static_cast<PlayMode>(mode_val);
                }
            } catch (...) {}
        } else if (key == "volume") {
            try {
                float vol = std::stof(val);
                volume_ = std::clamp(vol, 0.0f, 1.0f);
                audio_engine::AudioEngine::getInstance().setVolume(volume_);
            } catch (...) {}
        } else if (key == "muted") {
            try {
                is_muted_ = (std::stoi(val) != 0);
                audio_engine::AudioEngine::getInstance().setMuted(is_muted_);
            } catch (...) {}
        } else if (key == "fade_duration") {
            try {
                fade_duration_sec_ = std::clamp(std::stof(val), 0.0f, 3.0f);
            } catch (...) {}
        }
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
        1.15f, 1.05f, 1.10f, 0.95f, 1.05f, 0.90f,
        0.95f, 1.05f, 0.90f, 1.00f, 0.85f, 0.95f
    };
    static float s_smooth_levels[12] = {0.0f};
    float t = static_cast<float>(ImGui::GetTime());
    for (size_t i = 0; i < count; ++i) {
        float freq = 4.2f + (i % 3) * 1.8f;
        float wave = std::abs(std::sin(t * freq + i * 0.9f)) * 0.65f + 
                     std::abs(std::cos(t * (freq * 0.6f) - i * 1.3f)) * 0.35f;
        float w = (i < 12) ? base_weights[i] : 0.8f;
        float target = std::clamp(wave * w * 1.15f, 0.05f, 1.0f);
        if (i < 12) {
            s_smooth_levels[i] = s_smooth_levels[i] * 0.75f + target * 0.25f;
            out_levels[i] = s_smooth_levels[i];
        } else {
            out_levels[i] = target;
        }
    }
}