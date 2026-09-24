#include "SidebarView.hpp"
#include "Font.hpp"
#include "UIConfig.hpp"
#include <string>
#include <algorithm>

SidebarView::SidebarView() {
}

// ==============================================================================
// 独立模块 1：上部分菜单与歌单滚动视图 (包裹在 Xcode 风格液态玻璃容器中)
// ==============================================================================
void SidebarView::renderTopNav(const std::vector<Playlist>& playlists, float width, float height) {
    ImDrawList* dl_master = ImGui::GetWindowDrawList();

    float left_x = UIConfig::Layout::ContainerMarginX;
    float right_x = width - UIConfig::Layout::ContainerMarginX;
    float top_y = UIConfig::Layout::ContainerMarginY;
    float bot_y = top_y + height;
    float rounding = UIConfig::Layout::ContainerRounding;

    // 1. 绘制上部分主容器的 Xcode 风格液态玻璃底板与平滑边框
    dl_master->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                             UIConfig::Color::ContainerBg, rounding);
    dl_master->AddRect(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                       UIConfig::Color::ContainerBorder, rounding, 0, 1.0f);

    // 2. 内部内容区域 (内边距 6px，支持超长时隐藏滚动条平滑滚动)
    float inner_pad = 6.0f;
    float inner_w = (right_x - left_x) - inner_pad * 2.0f;
    float inner_h = height - inner_pad * 2.0f;

    ImGui::SetCursorScreenPos(ImVec2(left_x + inner_pad, top_y + inner_pad));

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoScrollbar 
                                 | ImGuiWindowFlags_NoBackground;

    ImGui::BeginChild("##TopContainerContent", ImVec2(inner_w, inner_h), false, child_flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 双通道分层：通道 0 绘制滑动发光胶囊，通道 1 绘制文字图标
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    // [1] 五大核心功能项
    auto feature_target = feature_widget_.render(current_tab_, [this](SidebarTab tab) {
        current_tab_ = tab;
    });

     if (feature_target) {
        target_indicator_y_ = feature_target->y;
        indicator_x_ = feature_target->x;
        indicator_w_ = feature_target->width;
        indicator_h_ = feature_target->height;
    }

    // [2] 歌单列表
    auto playlist_target = playlist_widget_.render(playlists, current_tab_, selected_playlist_id_);
    if (playlist_target) {
        target_indicator_y_ = playlist_target->y;
        indicator_x_ = playlist_target->x;
        indicator_w_ = playlist_target->width;
        indicator_h_ = playlist_target->height;
    }

    // 切回背景通道 0：绘制发光 + 平滑滑移的选中指示胶囊
    dl->ChannelsSetCurrent(0);

    if (target_indicator_y_ > 0.0f) {
        float dt = ImGui::GetIO().DeltaTime;
        if (indicator_y_ < 0.0f || !UIConfig::Animation::EnableSliding) {
            indicator_y_ = target_indicator_y_;
        } else {
            indicator_y_ += (target_indicator_y_ - indicator_y_) * std::clamp(dt * UIConfig::Animation::SlideSpeed, 0.0f, 1.0f);
            if (std::abs(target_indicator_y_ - indicator_y_) < 0.3f) {
                indicator_y_ = target_indicator_y_;
            }
        }

        ImVec2 p_min(indicator_x_, indicator_y_);
        ImVec2 p_max(indicator_x_ + indicator_w_, indicator_y_ + indicator_h_);
        float pill_rounding = UIConfig::Layout::NavItemRounding;

        // 1. 发光辉光层 (多层平滑外光晕 / Multi-tier Ambient Glow)
        if (UIConfig::Animation::EnableGlow) {
            ImU32 accent = UIConfig::Color::Accent;
            ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
            ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
            ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;
            float intensity = UIConfig::Animation::GlowIntensity;

            // 远场弥散微光 (扩散 7px)
            int a4 = std::clamp(static_cast<int>(18.0f * intensity), 0, 255);
            dl->AddRectFilled(ImVec2(p_min.x - 7.0f, p_min.y - 7.0f), 
                              ImVec2(p_max.x + 7.0f, p_max.y + 7.0f), 
                              IM_COL32(r, g, b, a4), pill_rounding + 5.0f);

            // 中场柔和环境光 (扩散 4px)
            int a3 = std::clamp(static_cast<int>(36.0f * intensity), 0, 255);
            dl->AddRectFilled(ImVec2(p_min.x - 4.0f, p_min.y - 4.0f), 
                              ImVec2(p_max.x + 4.0f, p_max.y + 4.0f), 
                              IM_COL32(r, g, b, a3), pill_rounding + 3.0f);

            // 近场核心亮光 (扩散 2px)
            int a2 = std::clamp(static_cast<int>(70.0f * intensity), 0, 255);
            dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y - 2.0f), 
                              ImVec2(p_max.x + 2.0f, p_max.y + 2.0f), 
                              IM_COL32(r, g, b, a2), pill_rounding + 1.5f);
        }

        // 2. 绘制主体液态玻璃胶囊 (主题色流体润色底板 + 通透磨砂高光 + 1px 无缝圆角轮廓)
        {
            ImU32 accent = UIConfig::Color::Accent;
            ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
            ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
            ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

            dl->AddRectFilled(p_min, p_max, IM_COL32(r, g, b, 60), pill_rounding);
            dl->AddRectFilled(p_min, p_max, UIConfig::Color::GlassActive, pill_rounding);
            dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, pill_rounding, 0, 1.0f);
        }
    }

    dl->ChannelsMerge();
    ImGui::EndChild();
}

// ==============================================================================
// 顶层主渲染入口：包含上下两个独立 Xcode 风格液态玻璃卡片容器
// ==============================================================================
void SidebarView::render(const std::vector<Playlist>& playlists, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar 
                           | ImGuiWindowFlags_NoResize 
                           | ImGuiWindowFlags_NoMove 
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoBackground
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIConfig::Color::WindowBg);
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("##SidebarMainView", nullptr, flags);

    const float margin_y = UIConfig::Layout::ContainerMarginY;
    const float gap = UIConfig::Layout::ContainerGap;
    const float dac_height = UIConfig::Layout::DacCardHeight;

    const float dac_y = height - margin_y - dac_height;
    const float top_height = dac_y - margin_y - gap;

    // 1. 渲染上部分功能与歌单容器 (左右间隔 16px，上下间隔 16px)
    renderTopNav(playlists, width, top_height);

    // 2. 渲染下部分固定 DAC 容器 (左右间隔 16px，上下间隔 16px)
    dac_widget_.render(width, dac_y, dac_height);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}