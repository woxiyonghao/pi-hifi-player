#pragma once

#include "ScanMusicWidget.hpp"
#include "EQConfigView.hpp"
#include "imgui.h"
#include "public/UIConfig.hpp"
#include "tools/MusicScanManager.hpp"
#include "tools/PlayerAdmin.hpp"
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ==============================================================================
// 主舞台核心中央视图 (MainStageView)
// 掌管屏幕右上方 794×520 主显示区域，根据侧边栏 Tab 智能路由各功能页面
// ==============================================================================
class MainStageView {
  public:
    using NavigateTabCallback = std::function<void(SidebarTab)>;

    MainStageView();
    ~MainStageView() = default;

    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = std::move(cb); }

    // 渲染主舞台视图
    void render(SidebarTab current_tab, uint64_t selected_playlist_id, std::vector<Playlist>& playlists,
                float stage_x = 230.0f, float stage_y = 0.0f, float stage_w = 794.0f, float stage_h = 600.0f);

  private:
    // 各选项卡子面板
    void renderScanMusicView(float x, float y, float w, float h, std::vector<Playlist>& playlists);
    void renderAllMusicView(float x, float y, float w, float h, const std::vector<Playlist>& playlists);
    void renderPlaylistView(uint64_t pid, std::vector<Playlist>& playlists, float x, float y, float w, float h);
    void renderEqualizerView(float x, float y, float w, float h);
    void renderDACSettingsView(float x, float y, float w, float h);
    void renderThemeSettingsView(float x, float y, float w, float h);
    void renderSystemSettingsView(float x, float y, float w, float h);

    // 辅助背景绘制
    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);

  private:
    ScanMusicWidget scan_widget_;
    EQConfigView eq_view_;
    NavigateTabCallback on_navigate_tab_;

    // 所有音乐树列表视图配置与折叠展开状态追踪
    int all_music_view_mode_ = 0; // 0: 按音频格式分类, 1: 按艺术家/专辑, 2: 按存储目录
    std::unordered_map<std::string, bool> tree_expanded_;
    std::unordered_map<std::string, float> tree_anim_t_;
};
