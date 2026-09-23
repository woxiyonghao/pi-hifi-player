#pragma once

#include "imgui.h"
#include "MusicModel.hpp" // 引入模型类
#include <vector>
#include <functional>

// 侧边栏核心分类与功能枚举
enum class SidebarTab {
    // 顶部五大核心功能
    ScanMusic,      // 扫描音乐
    Equalizer,      // Equalizer (10段图形 EQ)
    DACSettings,    // DACSettings (硬件滤波与输出)
    ThemeSettings,  // ThemeSettings (麦景图/金嗓子/自选色)
    SystemSettings, // SystemSettings (屏幕亮度/按键/系统)

    // 播放列表区
    AllMusic,       // 所有音乐
    CustomPlaylist  // 自定义歌单
};

class SidebarView {
public:
    // 回调函数：用户点击“+ 添加播放列表”时触发，通知外部数据层新建歌单
    using CreatePlaylistCallback = std::function<void()>;

    SidebarView();
    ~SidebarView() = default;

    // 纯视图渲染：接收外部的只读歌单模型引用 (解耦 View 与 Model)
    void render(const std::vector<Playlist>& playlists, float width = 230.0f, float height = 600.0f);

    // 状态查询与设置
    SidebarTab getCurrentTab() const { return current_tab_; }
    void setCurrentTab(SidebarTab tab) { current_tab_ = tab; }

    uint64_t getSelectedPlaylistId() const { return selected_playlist_id_; }
    void setSelectedPlaylistId(uint64_t id) {
        selected_playlist_id_ = id;
        current_tab_ = SidebarTab::CustomPlaylist;
    }

    // 注册添加歌单的回调
    void setOnCreatePlaylist(CreatePlaylistCallback cb) { on_create_playlist_ = cb; }

    // 主色调接口 (默认 Apple 玫红)
    void setAccentColor(ImU32 col) { accent_color_ = col; }
    ImU32 getAccentColor() const { return accent_color_; }

private:
    void drawSectionHeader(const char* title);
    bool drawNavItem(const char* icon, const char* label, bool is_selected);

private:
    SidebarTab current_tab_ = SidebarTab::AllMusic;
    uint64_t selected_playlist_id_ = 0;              // 选中歌单的唯一实体 ID
    CreatePlaylistCallback on_create_playlist_;      // 添加歌单事件回调

    ImU32 accent_color_ = IM_COL32(250, 45, 72, 255); // 经典 Apple 玫红 (#FA2D48)
};