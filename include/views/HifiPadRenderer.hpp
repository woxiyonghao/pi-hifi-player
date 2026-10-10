#pragma once

#include <vector>
#include <chrono>
#include <string>
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include "views/SidebarView.hpp"
#include "views/MainStageView.hpp"
#include "views/BottomBarView.hpp"
#include "themes/VUMeterRenderer.hpp"
#include "themes/AccuphaseMeterRenderer.hpp"
#include "themes/TapeReelRenderer.hpp"
#include "themes/SiriWaveformRenderer.hpp"
#include "themes/SiriOrbRenderer.hpp"
#include "themes/AudioBubblesRenderer.hpp"
#include "themes/NeonWaveformRenderer.hpp"
#include "themes/CyberGridRenderer.hpp"
#include "themes/GlassClockRenderer.hpp"
#include "tools/PlayerAdmin.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "themes/ThemeManager.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"

class HifiPadRenderer {
public:
    HifiPadRenderer();
    ~HifiPadRenderer() = default;

    void init();
    void render(float screen_w, float screen_h);

    void setSafeArea(float left, float top, float right, float bottom) {
        safe_left_ = left;
        safe_top_ = top;
        safe_right_ = right;
        safe_bottom_ = bottom;
    }

    void resetIdle() {
        idle_timer_ = 0.0f;
        is_fullscreen_idle_ = false;
    }

private:
    void renderBackground(float screen_w, float screen_h);
    void renderCreatePlaylistModal(float screen_w, float screen_h);

    std::vector<Playlist> playlists_;
    bool show_create_playlist_modal_ = false;
    bool create_playlist_focus_needed_ = false;
    char new_playlist_name_buf_[64] = "";

    SidebarView sidebar_;
    MainStageView main_stage_;
    BottomBarView bottom_bar_;
    VUMeterRenderer vu_renderer_;
    AccuphaseMeterRenderer accuphase_renderer_;
    TapeReelRenderer tape_renderer_;
    SiriWaveformRenderer siri_wave_renderer_;
    SiriOrbRenderer siri_orb_renderer_;
    AudioBubblesRenderer bubbles_renderer_;
    NeonWaveformRenderer neon_wave_renderer_;
    CyberGridRenderer cyber_grid_renderer_;
    GlassClockRenderer glass_clock_renderer_;

    SidebarTab last_saved_tab_ = SidebarTab::AllMusic;
    uint64_t last_saved_playlist_id_ = 0;

    float idle_timer_ = 0.0f;
    bool is_fullscreen_idle_ = false;
    float anim_progress_ = 0.0f;
    std::chrono::steady_clock::time_point last_frame_time_;

    float safe_left_ = 0.0f;
    float safe_top_ = 0.0f;
    float safe_right_ = 0.0f;
    float safe_bottom_ = 0.0f;
};
