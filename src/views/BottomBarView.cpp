#include "BottomBarView.hpp"
#include "Font.hpp"
#include "PlayerAdmin.hpp"
#include "UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <string>

BottomBarView::BottomBarView() {}

// ==============================================================================
// 1. 绘制外部液态玻璃磨砂胶囊底板与 1px 微光折射边框
// ==============================================================================
void BottomBarView::drawCapsuleBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    GlassCardRenderer::drawCard(dl, p_min, p_max, rounding, "bottom_bar");
}

// ==============================================================================
// 1.5 渲染整个胶囊背景的播放进度 (颜色与 SidebarView 选中的高亮胶囊严格对齐)
// ==============================================================================
void BottomBarView::renderProgressBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    auto& player = PlayerAdmin::getInstance();
    float progress = std::clamp(player.getProgress(), 0.0f, 1.0f);

    if (progress <= 0.001f) {
        return;
    }

    float total_w = p_max.x - p_min.x;
    float fill_x = p_min.x + total_w * progress;

    // 严密圆角裁切：保证进度从左往右推进时，两端半圆与胶囊物理轮廓完美吻合，绝无溢出
    dl->PushClipRect(ImVec2(p_min.x, p_min.y - 4.0f), ImVec2(fill_x, p_max.y + 4.0f), true);

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 1. 外层发光环境层 (Ambient Glow, 扩散 3px，与 SidebarView 选中项发光层完全对齐)
    if (UIConfig::Animation::EnableGlow) {
        float intensity = UIConfig::Animation::GlowIntensity;
        int a_glow = std::clamp(static_cast<int>(28.0f * intensity), 0, 255);
        dl->AddRectFilled(ImVec2(p_min.x - 3.0f, p_min.y - 3.0f), 
                          ImVec2(p_max.x + 3.0f, p_max.y + 3.0f), 
                          IM_COL32(r, g, b, a_glow), rounding + 2.0f);
    }

    // 2. 主体主题色流体润色底板 (与 SidebarView 选中胶囊的 IM_COL32(r, g, b, 60~70) 完全一致)
    dl->AddRectFilled(p_min, p_max, IM_COL32(r, g, b, 70), rounding);

    // 3. 通透磨砂高光层 (UIConfig::Color::GlassActive: IM_COL32(255, 255, 255, 30))
    dl->AddRectFilled(p_min, p_max, UIConfig::Color::GlassActive, rounding);

    // 4. 表面微光渐变 (Surface Specular Sheen)
    dl->AddRectFilledMultiColor(
        p_min, p_max,
        IM_COL32(255, 255, 255, 28), // Top-Left
        IM_COL32(255, 255, 255, 12), // Top-Right
        IM_COL32(255, 255, 255, 0),  // Bottom-Right
        IM_COL32(255, 255, 255, 16)  // Bottom-Left
    );

    // 5. 进度前锋垂直微光棱 (Leading Edge Light Line)
    if (fill_x > p_min.x + 2.0f && fill_x < p_max.x - 2.0f) {
        // 主题色微晕
        dl->AddLine(ImVec2(fill_x, p_min.y + 2.0f), ImVec2(fill_x, p_max.y - 2.0f), 
                    IM_COL32(r, g, b, 220), 2.5f);
        // 白色高光纤细光柱
        dl->AddLine(ImVec2(fill_x, p_min.y + 3.0f), ImVec2(fill_x, p_max.y - 3.0f), 
                    IM_COL32(255, 255, 255, 240), 1.0f);
    }

    // 6. 1px 微光折射边框 (UIConfig::Color::GlassBorder)
    dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, rounding, 0, 1.0f);

    dl->PopClipRect();
}

// ==============================================================================
// 2. 左区：播放模式 -> 上一曲 -> 播放/暂停 -> 下一曲
// ==============================================================================
void BottomBarView::renderLeftControls(ImDrawList* dl, float start_x, float center_y) {
    const float btn_w = 34.0f;
    const float btn_h = height_; // 48.0f 全高触控热区，杜绝上下边缘死区

    // 4个核心按键中心点排列 (保持 32px 舒适人机间隙)
    const float c_mode = start_x + 14.0f;
    const float c_prev = c_mode + 32.0f;
    const float c_play = c_prev + 32.0f;
    const float c_next = c_play + 32.0f;

    // 委托给 4 个独立小组件渲染
    play_mode_widget_.render(dl, ImVec2(c_mode, center_y), ImVec2(btn_w, btn_h));
    prev_widget_.render(dl, ImVec2(c_prev, center_y), ImVec2(btn_w, btn_h));
    play_pause_widget_.render(dl, ImVec2(c_play, center_y), ImVec2(btn_w, btn_h));
    next_widget_.render(dl, ImVec2(c_next, center_y), ImVec2(btn_w, btn_h));
}

void BottomBarView::render(float screen_w, float screen_h) {
    const float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    const float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    const float rounding = height_ * 0.5f;                     // 24.0f (半高半圆)

    // 几何对齐：
    float left_x = UIConfig::Layout::SidebarWidth + margin_x;
    float right_x = screen_w - margin_x;
    float bot_y = screen_h - margin_y;
    float top_y = bot_y - height_;
    float center_y = (top_y + bot_y) * 0.5f;

    // 1. 设置窗口位置与大小
    ImGui::SetNextWindowPos(ImVec2(left_x, top_y));
    ImGui::SetNextWindowSize(ImVec2(right_x - left_x, height_));

    // 2. 窗口标志：无标题栏、无多余边框、无背景
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##BottomBarContainer", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p_min(left_x, top_y);
    ImVec2 p_max(right_x, bot_y);

    // 1. 渲染毛玻璃胶囊基底
    drawCapsuleBackground(dl, p_min, p_max, rounding);

    // 2. 渲染全背景播放进度 (颜色对齐 SidebarView 激活选中胶囊)
    renderProgressBackground(dl, p_min, p_max, rounding);

    // 3. 左区：播放操作控制块 (最高交互优先级：模式、上一曲、播放/暂停、下一曲)
    renderLeftControls(dl, left_x + rounding + 4.0f, center_y);

    // 4. 右区：发烧音量调节组件 (最高交互优先级：小喇叭静音、音量拖拽滑块条)
    float right_limit = right_x - rounding - 4.0f;
    volume_widget_.render(dl, right_limit, center_y);

    // 5. 中间安全区域触控 Seek 响应 (严格限制在左区按键与右区音量条之间的中央区域，绝无任何手势冲突)
    float seek_x0 = left_x + 162.0f;  // 避开左侧所有按键 (+7px 安全缓冲)
    float seek_x1 = right_x - 198.0f; // 避开右侧小喇叭与滑块 (+10px 净间距)
    if (seek_x1 > seek_x0) {
        ImVec2 seek_min(seek_x0, top_y);
        ImVec2 seek_size(seek_x1 - seek_x0, height_);
        ImGui::SetCursorScreenPos(seek_min);
        ImGui::InvisibleButton("##BottomBarCenterSeekArea", seek_size);
        if (ImGui::IsItemActive()) {
            float mouse_x = ImGui::GetIO().MousePos.x;
            float new_progress = std::clamp((mouse_x - left_x) / (right_x - left_x), 0.0f, 1.0f);
            auto& player = PlayerAdmin::getInstance();
            double duration = player.getDurationSec();
            if (duration > 0.0) {
                player.seek(static_cast<double>(new_progress) * duration);
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}