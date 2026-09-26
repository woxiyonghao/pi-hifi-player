#pragma once

#include "types/MusicModel.hpp"
#include "types/SidebarTypes.hpp"
#include "public/UIConfig.hpp"
#include "imgui.h"
#include "widgets/SidebarDacWidget.hpp"
#include "widgets/SidebarFeatureWidget.hpp"
#include "widgets/SidebarPlaylistWidget.hpp"
#include <functional>
#include <vector>


class SidebarView {
  public:
    using CreatePlaylistCallback = std::function<void()>;

    SidebarView();
    ~SidebarView() = default;

    // 顶层渲染入口 (默认使用 UIConfig 中的标准尺寸)
    void render(const std::vector<Playlist>& playlists, float width = UIConfig::Layout::SidebarWidth,
                float height = UIConfig::Layout::ScreenHeight);

    SidebarTab getCurrentTab() const { return current_tab_; }
    void setCurrentTab(SidebarTab tab) { current_tab_ = tab; }

    uint64_t getSelectedPlaylistId() const { return selected_playlist_id_; }
    void setSelectedPlaylistId(uint64_t id) {
        selected_playlist_id_ = id;
        current_tab_ = SidebarTab::CustomPlaylist;
    }

    void setOnCreatePlaylist(CreatePlaylistCallback cb) { on_create_playlist_ = cb; }

    // 主题色获取与设置
    void setAccentColor(ImU32 col) { UIConfig::Color::Accent = col; }
    ImU32 getAccentColor() const { return UIConfig::Color::Accent; }

     // DAC 硬件连接状态
    void setDacConnected(bool connected, const std::string& name = "AK4499EX") {
        dac_widget_.setConnected(connected, name);
    }
    bool isDacConnected() const {
        return dac_widget_.isConnected();
    }

  private:
    // 上部分菜单与歌单独立滚动视图
    void renderTopNav(const std::vector<Playlist>& playlists, float width, float height);


  private:
    SidebarTab current_tab_ = SidebarTab::AllMusic;
    uint64_t selected_playlist_id_ = 0;
    CreatePlaylistCallback on_create_playlist_;

    // 动效与滑块位置追踪状态
    float indicator_y_ = -1.0f;
    float target_indicator_y_ = -1.0f;
    float indicator_x_ = 0.0f;
    float indicator_w_ = 0.0f;
    float indicator_h_ = 34.0f;

    // feature 功能区域
    SidebarFeatureWidget feature_widget_;
    // 播放列表
    SidebarPlaylistWidget playlist_widget_;
    // DAC
    SidebarDacWidget dac_widget_;
};