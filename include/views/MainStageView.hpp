#pragma once

#include "ScanMusicWidget.hpp"
#include "imgui.h"
#include "public/UIConfig.hpp"
#include "tools/MusicScanManager.hpp"
#include "tools/PlayerAdmin.hpp"
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>
// ==============================================================================
// 主舞台核心中央视图 (MainStageView)
// 掌管屏幕右上方 794×520 主显示区域，根据侧边栏 Tab 智能路由各功能页面
// ==============================================================================
class MainStageView {
  public:
    MainStageView();
    ~MainStageView() = default;

    // 渲染主舞台视图
    void render(SidebarTab current_tab, uint64_t selected_playlist_id, std::vector<Playlist>& playlists,
                float stage_x = 230.0f, float stage_y = 0.0f, float stage_w = 794.0f, float stage_h = 600.0f);

  private:
    // 各选项卡子面板
    void renderScanMusicView(float x, float y, float w, float h, std::vector<Playlist>& playlists);
    void renderPlaylistView(uint64_t pid, std::vector<Playlist>& playlists, float x, float y, float w, float h);
    void renderEqualizerView(float x, float y, float w, float h);
    void renderDACSettingsView(float x, float y, float w, float h);
    void renderThemeSettingsView(float x, float y, float w, float h);
    void renderSystemSettingsView(float x, float y, float w, float h);

    // 辅助背景绘制
    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);

  private:
    char scan_path_buf_[256] = "";
    ScanMusicWidget scan_widget_;
};
