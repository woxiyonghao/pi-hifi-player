#pragma once

#include "ScanMusicWidget.hpp"
#include "EQConfigView.hpp"
#include "SystemSettingsView.hpp"
#include "ThemeSettingView.hpp"
#include "AllMusicPlaylistView.hpp"
#include "CustomPlaylistView.hpp"
#include "views/DACSettingView.hpp"
#include "views/MagicTuningView.hpp"
#include "views/TerminalView.hpp"
#include "views/WifiTransferView.hpp"
#include "imgui.h"
#include "public/UIConfig.hpp"
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include <cstdint>
#include <functional>
#include <vector>

// ==============================================================================
// 主舞台核心中央视图 (MainStageView)
// 掌管屏幕右上方 794×520 主显示区域，根据侧边栏 Tab 智能路由各功能页面
// ==============================================================================
class MainStageView {
  public:
    using NavigateTabCallback = std::function<void(SidebarTab)>;
    using SelectPlaylistCallback = std::function<void(SidebarTab, uint64_t)>;

    MainStageView();
    ~MainStageView() = default;

    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = std::move(cb); }
    void setOnSelectPlaylist(SelectPlaylistCallback cb) { on_select_playlist_ = std::move(cb); }
    void setOnIdleFullscreenChanged(SystemSettingsView::IdleFullscreenCallback cb) {
        settings_view_.setOnIdleFullscreenChanged(std::move(cb));
    }
    float getIdleFullscreenSeconds() const { return settings_view_.getIdleFullscreenSeconds(); }
    const DACSettingView& getDacView() const { return dac_view_; }
    DACSettingView& getDacView() { return dac_view_; }

    // 渲染主舞台视图
    void render(SidebarTab current_tab, uint64_t selected_playlist_id, std::vector<Playlist>& playlists,
                float stage_x = 230.0f, float stage_y = 0.0f, float stage_w = 794.0f, float stage_h = 600.0f);

  private:
    // 各选项卡子面板
    void renderScanMusicView(float x, float y, float w, float h, std::vector<Playlist>& playlists);
    void renderEqualizerView(float x, float y, float w, float h);

    // 辅助背景绘制
    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);

  private:
    ScanMusicWidget scan_widget_;
    EQConfigView eq_view_;
    MagicTuningView magic_tuning_view_;
    DACSettingView dac_view_;
    SystemSettingsView settings_view_;
    ThemeSettingView theme_setting_view_;
    AllMusicPlaylistView all_music_view_;
    CustomPlaylistView custom_playlist_view_;
    TerminalView terminal_view_;
    WifiTransferView wifi_transfer_view_;

    NavigateTabCallback on_navigate_tab_;
    SelectPlaylistCallback on_select_playlist_;
};
