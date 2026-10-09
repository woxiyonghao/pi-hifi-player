#pragma once

#include <vector>
#include <string>
#include <functional>
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include "views/EQConfigView.hpp"
#include "views/MagicTuningView.hpp"
#include "views/DACSettingView.hpp"
#include "themes/AccuphaseMeterRenderer.hpp"
#include "themes/TapeReelRenderer.hpp"
#include "themes/VUMeterRenderer.hpp"
#include "themes/SiriWaveformRenderer.hpp"
#include "themes/ThemeManager.hpp"
#include "tools/PlayerAdmin.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "services/WebService.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"

// ==============================================================================
// 移动端专用 Tab 枚举 (适应手机单列竖屏单手触控)
// ==============================================================================
enum class PhoneTab {
    Library,   // 曲库 (全部音乐 / 自定义歌单 / 扫描)
    Tuning,    // 调音 (10段图形EQ / MSEB调音魔棒)
    Hardware,  // 硬件与服务 (DAC芯片 / Web服务 / 模式切换)
    Visual     // 发烧大屏动效 (金嗓子 / 开盘机 / VU表头)
};

class HifiPhoneRenderer {
public:
    HifiPhoneRenderer();
    ~HifiPhoneRenderer() = default;

    void init();
    void render(float screen_w, float screen_h);

    // 模式切换回调 (当用户在手机端点击“切换为数播”时通知控制器)
    void setOnSwitchToStreamer(std::function<void()> cb) {
        on_switch_to_streamer_ = std::move(cb);
    }

    void resetIdle() {
        idle_timer_ = 0.0f;
    }

private:
    void renderHeader(float screen_w, float top_inset);
    void renderMainContent(float screen_w, float content_y, float content_h);
    void renderLibraryTab(float x, float y, float w, float h);
    void renderTuningTab(float x, float y, float w, float h);
    void renderHardwareTab(float x, float y, float w, float h);
    void renderVisualTab(float x, float y, float w, float h);
    void renderMiniPlayer(float screen_w, float bottom_y);
    void renderBottomTabBar(float screen_w, float tabbar_y, float tabbar_h);
    void renderNowPlayingOverlay(float screen_w, float screen_h);

    PhoneTab current_tab_ = PhoneTab::Library;
    int tuning_subtab_ = 0; // 0: 10段EQ, 1: MSEB魔棒
    bool show_now_playing_ = false; // 是否展开全屏沉浸播放大页
    float idle_timer_ = 0.0f;

    std::vector<Track> cached_tracks_;
    std::vector<Playlist> cached_playlists_;
    bool data_loaded_ = false;

    std::function<void()> on_switch_to_streamer_;

    // 发烧表头组件复用
    AccuphaseMeterRenderer accuphase_renderer_;
    TapeReelRenderer tape_renderer_;
    VUMeterRenderer vu_renderer_;
    SiriWaveformRenderer siri_renderer_;

    // 调音与硬件组件
    EQConfigView eq_view_;
    MagicTuningView magic_tuning_view_;
    DACSettingView dac_view_;
};
