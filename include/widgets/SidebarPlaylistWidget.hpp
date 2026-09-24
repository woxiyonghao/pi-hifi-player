#pragma once

#include "MusicModel.hpp"
#include "imgui.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include "SidebarTypes.hpp"

// 供父容器绘制发光背景胶囊使用的坐标目标
struct PlaylistIndicatorTarget {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// 歌单区域专属图标类型
enum class PlaylistIcon {
    Music,      // 发烧音符 (所有音乐)
    Playlist,   // 三道横线 (实体歌单)
    Add         // 十字加号 (新建歌单)
};

class SidebarPlaylistWidget {
public:
    using CreatePlaylistCallback = std::function<void()>;

    SidebarPlaylistWidget() = default;
    ~SidebarPlaylistWidget() = default;

    // 渲染歌单区域
    // 直接传入 current_tab 与 selected_playlist_id 的引用，点击项内部直接更新，无需手写 Lambda
    std::optional<PlaylistIndicatorTarget> render(
        const std::vector<Playlist>& playlists,
        SidebarTab& current_tab,
        uint64_t& selected_playlist_id
    );

    // 设置点击“+ 添加播放列表”时的回调
    void setOnCreatePlaylist(CreatePlaylistCallback cb) {
        on_create_playlist_ = cb;
    }

private:
    void drawHeader(const char* title);
    bool drawPlaylistItem(PlaylistIcon icon, const char* label, bool is_selected, PlaylistIndicatorTarget& out_target);
    void drawPlaylistIcon(ImDrawList* dl, ImVec2 center, PlaylistIcon icon, ImU32 color);

private:
    CreatePlaylistCallback on_create_playlist_;
};