#pragma once

#include "imgui.h"
#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ==============================================================================
// 用户自定义专属发烧歌单视图 (CustomPlaylistView)
// 管控发烧歌单曲目列表呈现、曲目添加/移除、在播高亮与歌单删除二次确认交互
// ==============================================================================
class CustomPlaylistView {
public:
    using SelectPlaylistCallback = std::function<void(SidebarTab, uint64_t)>;
    using NavigateTabCallback = std::function<void(SidebarTab)>;

    CustomPlaylistView() = default;
    ~CustomPlaylistView() = default;

    void setOnSelectPlaylist(SelectPlaylistCallback cb) { on_select_playlist_ = std::move(cb); }
    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = std::move(cb); }

    // 渲染自定义歌单主面板
    void render(uint64_t pid, std::vector<Playlist>& playlists, float x, float y, float w, float h);

private:
    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);
    void renderAddMusicToPlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists);
    void renderDeletePlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists);

private:
    SelectPlaylistCallback on_select_playlist_;
    NavigateTabCallback on_navigate_tab_;

    // 歌单曲目添加选择模态对话框
    bool show_add_music_modal_ = false;
    char add_music_search_buf_[128] = "";
    std::vector<uint64_t> selected_track_ids_to_add_;

    // 歌单删除二次确认模态对话框
    bool show_delete_playlist_modal_ = false;
};
