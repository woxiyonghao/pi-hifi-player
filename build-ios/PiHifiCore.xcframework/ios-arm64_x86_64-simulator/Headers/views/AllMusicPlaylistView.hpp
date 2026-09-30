#pragma once

#include "imgui.h"
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ==============================================================================
// 所有音乐树列表全景视图 (AllMusicPlaylistView)
// 支持按音频格式分类、按艺术家/专辑、按存储目录树状层级浏览与播放控制
// ==============================================================================
class AllMusicPlaylistView {
public:
    using NavigateTabCallback = std::function<void(SidebarTab)>;

    AllMusicPlaylistView() = default;
    ~AllMusicPlaylistView() = default;

    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = std::move(cb); }

    // 渲染所有音乐视图
    void render(float x, float y, float w, float h, const std::vector<Playlist>& playlists);

private:
    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);

private:
    NavigateTabCallback on_navigate_tab_;

    // 所有音乐树列表视图配置与折叠展开状态追踪
    int all_music_view_mode_ = 0; // 0: 按音频格式分类, 1: 按艺术家/专辑, 2: 按存储目录
    std::unordered_map<std::string, bool> tree_expanded_;
    std::unordered_map<std::string, float> tree_anim_t_;
};
