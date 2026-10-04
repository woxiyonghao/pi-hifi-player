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

    void drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle = nullptr);

    // 缓存分组数据结构
    struct FormatGroup {
        std::string key;
        std::string badge;
        std::string name;
        std::vector<Track> group_tracks;
    };

    struct AlbumGroup {
        std::string album_name;
        std::vector<Track> tracks;
    };

    struct ArtistGroup {
        std::string artist_name;
        std::vector<Track> all_tracks;
        std::vector<AlbumGroup> albums;
    };

    struct DirGroup {
        std::string dir_name;
        std::vector<Track> tracks;
    };

    void rebuildCache(const std::vector<Track>& tracks);

private:
    NavigateTabCallback on_navigate_tab_;

    // 所有音乐树列表视图配置与折叠展开状态追踪
    int all_music_view_mode_ = 0; // 0: 按音频格式分类, 1: 按艺术家/专辑, 2: 按存储目录
    std::unordered_map<std::string, bool> tree_expanded_;
    std::unordered_map<std::string, float> tree_anim_t_;

    // 树状分组缓存：大幅降低每帧 CPU 堆分配与深拷贝开销
    size_t last_tracks_count_ = static_cast<size_t>(-1);
    uint64_t last_first_track_id_ = 0;
    std::vector<FormatGroup> cached_format_groups_;
    std::vector<ArtistGroup> cached_artist_groups_;
    std::vector<DirGroup> cached_dir_groups_;
};
