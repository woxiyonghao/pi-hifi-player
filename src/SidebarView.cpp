#include "SidebarView.hpp"
#include "Font.hpp"
#include "UIConfig.hpp"
#include <string>
#include <algorithm>

SidebarView::SidebarView() {
}

// ==============================================================================
// 纯矢量单色图标绘制引擎 (Vector Icon Engine)
// 纯原生 GPU 几何绘制，支持随主题色 (UIConfig::Color::Accent) 动态变色，0 依赖、永不缺字
// ==============================================================================
static void DrawNavIcon(ImDrawList* dl, ImVec2 center, NavIcon icon, ImU32 color) {
    switch (icon) {
        case NavIcon::Search: {
            // 放大镜：镜框圆圈 + 45 度手柄
            ImVec2 c(center.x - 2.0f, center.y - 2.0f);
            dl->AddCircle(c, 5.0f, color, 16, 1.5f);
            dl->AddLine(ImVec2(c.x + 3.5f, c.y + 3.5f), ImVec2(c.x + 7.5f, c.y + 7.5f), color, 1.8f);
            break;
        }
        case NavIcon::Equalizer: {
            // 均衡器：三根频率推子轨道 + 可调滑块旋钮
            float xs[3] = { center.x - 5.0f, center.x, center.x + 5.0f };
            float knobs[3] = { center.y - 2.0f, center.y + 3.0f, center.y - 4.0f };
            for (int i = 0; i < 3; ++i) {
                dl->AddLine(ImVec2(xs[i], center.y - 6.0f), ImVec2(xs[i], center.y + 6.0f), color, 1.2f);
                dl->AddCircleFilled(ImVec2(xs[i], knobs[i]), 2.2f, color);
            }
            break;
        }
        case NavIcon::DAC: {
            // DAC 解码器：微型芯片封装底座 + 内部核心点
            ImVec2 m1(center.x - 6.0f, center.y - 5.0f);
            ImVec2 m2(center.x + 6.0f, center.y + 5.0f);
            dl->AddRect(m1, m2, color, 2.0f, 0, 1.4f);
            dl->AddCircleFilled(center, 1.8f, color);
            break;
        }
        case NavIcon::Theme: {
            // 调色盘：经典画盘轮廓 + 两个调色小孔
            dl->AddCircle(center, 6.0f, color, 16, 1.4f);
            dl->AddCircleFilled(ImVec2(center.x - 2.0f, center.y - 2.0f), 1.2f, color);
            dl->AddCircleFilled(ImVec2(center.x + 2.0f, center.y - 1.0f), 1.2f, color);
            break;
        }
        case NavIcon::System: {
            // 系统设置：微型同心环与中心节点
            dl->AddCircle(center, 5.5f, color, 16, 1.4f);
            dl->AddCircleFilled(center, 2.0f, color);
            break;
        }
        case NavIcon::Music: {
            // 发烧音符：双八分连音符 (符头 + 符杆 + 符梁)
            ImVec2 n1(center.x - 3.0f, center.y + 3.0f);
            ImVec2 n2(center.x + 3.0f, center.y + 1.0f);
            dl->AddCircleFilled(n1, 2.2f, color);
            dl->AddCircleFilled(n2, 2.2f, color);
            dl->AddLine(ImVec2(n1.x + 1.5f, n1.y), ImVec2(n1.x + 1.5f, center.y - 5.0f), color, 1.2f);
            dl->AddLine(ImVec2(n2.x + 1.5f, n2.y), ImVec2(n2.x + 1.5f, center.y - 7.0f), color, 1.2f);
            dl->AddLine(ImVec2(n1.x + 1.5f, center.y - 5.0f), ImVec2(n2.x + 1.5f, center.y - 7.0f), color, 1.6f);
            break;
        }
        case NavIcon::Playlist: {
            // 播放列表：三道等宽水平条目线
            for (int i = -1; i <= 1; ++i) {
                float y = center.y + i * 4.0f;
                dl->AddLine(ImVec2(center.x - 6.0f, y), ImVec2(center.x + 6.0f, y), color, 1.4f);
            }
            break;
        }
        case NavIcon::Add: {
            // 加号：等长十字交叉线
            dl->AddLine(ImVec2(center.x - 5.0f, center.y), ImVec2(center.x + 5.0f, center.y), color, 1.5f);
            dl->AddLine(ImVec2(center.x, center.y - 5.0f), ImVec2(center.x, center.y + 5.0f), color, 1.5f);
            break;
        }
    }
}

// ==============================================================================
// 绘制纯净平滑的液态玻璃材质 (Liquid Glass)
// 包含：半透明通透底板 + 1px 环形平滑微光折射边框 (彻底剔除生硬直线)
// ==============================================================================
static void DrawLiquidGlass(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, ImU32 fill_color) {
    // 1. 半透明通透底板
    dl->AddRectFilled(p_min, p_max, fill_color, rounding);

    // 2. 1px 环形平滑微光折射边框 (抗锯齿平滑圆角轮廓)
    dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, rounding, 0, 1.0f);
}

// ==============================================================================
// 基础单元：分组小标题 (使用 Fonts::Small 12px 小字阶)
// ==============================================================================
void SidebarView::drawSectionHeader(const char* title) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextMuted);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImGui::TextUnformatted(title);
    if (Fonts::Small) ImGui::PopFont();

    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

// ==============================================================================
// 基础单元：标准胶囊菜单项 (前景色绘制 + 动画坐标记录)
// ==============================================================================
bool SidebarView::drawNavItem(NavIcon icon, const char* label, bool is_selected) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = UIConfig::Layout::NavItemHeight;
    float rounding = UIConfig::Layout::NavItemRounding;

    std::string btn_id = std::string("##nav_") + label;
    bool clicked = ImGui::InvisibleButton(btn_id.c_str(), ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + width, pos.y + height);

    // 1. 记录被选中项的坐标，供背景通道绘制滑动的发光液态玻璃
    if (is_selected) {
        target_indicator_y_ = pos.y;
        indicator_x_ = p_min.x;
        indicator_w_ = width;
        indicator_h_ = height;
    } else if (hovered) {
        // 未选中项在悬停时，绘制浅色临时毛玻璃
        DrawLiquidGlass(dl, p_min, p_max, rounding, UIConfig::Color::GlassHover);
    }

    // 2. 颜色状态联动：激活时使用高亮纯白 (与发光底板对比鲜明)，未激活时使用柔和灰白
    ImU32 icon_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::IconNormal;
    ImU32 text_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal;

    // 3. 绘制居中的矢量图标 (随状态动态变色)
    DrawNavIcon(dl, ImVec2(pos.x + 18.0f, pos.y + height * 0.5f), icon, icon_col);

    // 4. 绘制菜单文字
    dl->AddText(ImVec2(pos.x + 36.0f, pos.y + 8.0f), text_col, label);

    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    return clicked;
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
    drawSectionHeader("功能");
    if (drawNavItem(NavIcon::Search, "扫描音乐", current_tab_ == SidebarTab::ScanMusic)) {
        current_tab_ = SidebarTab::ScanMusic;
    }
    if (drawNavItem(NavIcon::Equalizer, "Equalizer (EQ)", current_tab_ == SidebarTab::Equalizer)) {
        current_tab_ = SidebarTab::Equalizer;
    }
    if (drawNavItem(NavIcon::DAC, "DACSettings", current_tab_ == SidebarTab::DACSettings)) {
        current_tab_ = SidebarTab::DACSettings;
    }
    if (drawNavItem(NavIcon::Theme, "ThemeSettings", current_tab_ == SidebarTab::ThemeSettings)) {
        current_tab_ = SidebarTab::ThemeSettings;
    }
    if (drawNavItem(NavIcon::System, "SystemSettings", current_tab_ == SidebarTab::SystemSettings)) {
        current_tab_ = SidebarTab::SystemSettings;
    }

    // [2] 歌单列表
    drawSectionHeader("播放列表");
    if (drawNavItem(NavIcon::Music, "所有音乐", current_tab_ == SidebarTab::AllMusic)) {
        current_tab_ = SidebarTab::AllMusic;
    }

    for (const auto& playlist : playlists) {
        bool is_sel = (current_tab_ == SidebarTab::CustomPlaylist && selected_playlist_id_ == playlist.getId());
        if (drawNavItem(NavIcon::Playlist, playlist.getName().c_str(), is_sel)) {
            current_tab_ = SidebarTab::CustomPlaylist;
            selected_playlist_id_ = playlist.getId();
        }
    }

    // [3] 添加歌单按钮
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (drawNavItem(NavIcon::Add, "添加播放列表", false)) {
        if (on_create_playlist_) {
            on_create_playlist_();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

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
// 独立模块 2：下部分固定 DAC 状态视图 (包裹在 Xcode 风格液态玻璃容器中)
// ==============================================================================
void SidebarView::renderBottomDac(float width, float y, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float left_x = UIConfig::Layout::ContainerMarginX;
    float right_x = width - UIConfig::Layout::ContainerMarginX;
    float top_y = y;
    float bot_y = y + height;
    float rounding = UIConfig::Layout::ContainerRounding;

    // 交互悬停检测
    ImGui::SetCursorScreenPos(ImVec2(left_x, top_y));
    ImGui::InvisibleButton("##DacContainerButton", ImVec2(right_x - left_x, bot_y - top_y));
    bool hovered = ImGui::IsItemHovered();

    // 1. 绘制下部分 DAC 容器的 Xcode 风格液态玻璃底板与平滑边框
    dl->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                      UIConfig::Color::ContainerBg, rounding);

    if (hovered) {
        dl->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                          UIConfig::Color::GlassHover, rounding);
    }

    dl->AddRect(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                UIConfig::Color::ContainerBorder, rounding, 0, 1.0f);

    // 2. 内部状态指示灯与文字排版
    float center_y = (top_y + bot_y) * 0.5f;
    float led_x = left_x + 18.0f;

    if (dac_connected_) {
        // [已连接]：发光绿灯 + 高亮设备名称
        dl->AddCircleFilled(ImVec2(led_x, center_y), 5.5f, IM_COL32(52, 199, 89, 70));
        dl->AddCircleFilled(ImVec2(led_x, center_y), 3.0f, UIConfig::Color::DacConnected);
        dl->AddText(ImVec2(led_x + 12.0f, center_y - 7.0f), UIConfig::Color::TextNormal, dac_name_.c_str());
    } else {
        // [未连接]：微光灰点 + 次级提示文本
        dl->AddCircleFilled(ImVec2(led_x, center_y), 3.0f, UIConfig::Color::DacDisconnected);
        dl->AddText(ImVec2(led_x + 12.0f, center_y - 7.0f), UIConfig::Color::TextMuted, "DAC: 未连接");
    }
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
                           | ImGuiWindowFlags_NoScrollbar;

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
    renderBottomDac(width, dac_y, dac_height);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}