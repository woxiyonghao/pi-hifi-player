#pragma once

#include "imgui.h"
#include "MusicModel.hpp"
#include "UIConfig.hpp"
#include <vector>
#include <functional>

// 侧边栏页面与功能枚举
enum class SidebarTab {
    ScanMusic,      // 扫描音乐
    Equalizer,      // 10段图形 EQ
    DACSettings,    // DAC 硬件滤波与输出
    ThemeSettings,  // 主题风格与调色盘
    SystemSettings, // 系统与硬件配置
    AllMusic,       // 所有音乐
    CustomPlaylist  // 自定义歌单
};

// 侧边栏矢量图标枚举 (纯代码绘制，支持动态变色)
enum class NavIcon {
    Search,         // 放大镜 (扫描音乐)
    Equalizer,      // 调音滑块 (EQ)
    DAC,            // 芯片底座 (DAC)
    Theme,          // 调色板 (主题)
    System,         // 齿轮滑块 (系统)
    Music,          // 音符 (所有音乐)
    Playlist,       // 列表横线 (播放列表)
    Add             // 加号 (添加播放列表)
};

class SidebarView {
public:
    using CreatePlaylistCallback = std::function<void()>;

    SidebarView();
    ~SidebarView() = default;

    // 顶层渲染入口 (默认使用 UIConfig 中的标准尺寸)
    void render(const std::vector<Playlist>& playlists, 
                float width = UIConfig::Layout::SidebarWidth, 
                float height = UIConfig::Layout::ScreenHeight);

    SidebarTab getCurrentTab() const { return current_tab_; }
    void setCurrentTab(SidebarTab tab) { current_tab_ = tab; }

    uint64_t getSelectedPlaylistId() const { return selected_playlist_id_; }
    void setSelectedPlaylistId(uint64_t id) {
        selected_playlist_id_ = id;
        current_tab_ = SidebarTab::CustomPlaylist;
    }

    void setOnCreatePlaylist(CreatePlaylistCallback cb) { on_create_playlist_ = cb; }

    // DAC 硬件连接状态
    void setDacConnected(bool connected, const std::string& name = "ES9038PRO Balanced") {
        dac_connected_ = connected;
        dac_name_ = name;
    }
    bool isDacConnected() const { return dac_connected_; }

    // 主题色获取与设置
    void setAccentColor(ImU32 col) { UIConfig::Color::Accent = col; }
    ImU32 getAccentColor() const { return UIConfig::Color::Accent; }

private:
    // 上部分菜单与歌单独立滚动视图
    void renderTopNav(const std::vector<Playlist>& playlists, float width, float height);

    // 下部分固定 DAC 状态视图
    void renderBottomDac(float width, float y, float height);

    // 基础排版单元
    void drawSectionHeader(const char* title);
    bool drawNavItem(NavIcon icon, const char* label, bool is_selected);

private:
    SidebarTab current_tab_ = SidebarTab::AllMusic;
    uint64_t selected_playlist_id_ = 0;
    CreatePlaylistCallback on_create_playlist_;

    bool dac_connected_ = false;
    std::string dac_name_ = "ES9038PRO Balanced";

    // 动效与滑块位置追踪状态
    float indicator_y_ = -1.0f;
    float target_indicator_y_ = -1.0f;
    float indicator_x_ = 0.0f;
    float indicator_w_ = 0.0f;
    float indicator_h_ = 34.0f;
};