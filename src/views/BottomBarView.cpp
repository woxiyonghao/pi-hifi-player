#include "BottomBarView.hpp"
#include "Font.hpp"
#include "PlayerAdmin.hpp"
#include "UIConfig.hpp"
#include <algorithm>
#include <cmath>
#include <string>

BottomBarView::BottomBarView() {}

// ==============================================================================
// 1. 绘制外部液态玻璃磨砂胶囊底板与 1px 微光折射边框
// ==============================================================================
void BottomBarView::drawCapsuleBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    dl->AddRectFilled(p_min, p_max, UIConfig::Color::ContainerBg, rounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::ContainerBorder, rounding, 0, 1.0f);
}

// ==============================================================================
// 2. 左区：播放模式 -> 上一曲 -> 播放/暂停 -> 下一曲
// ==============================================================================
void BottomBarView::renderLeftControls(ImDrawList* dl, float start_x, float center_y) {
    // 视觉尺寸与中心点计算 (保持 12px 严格净间隙)
    const float c_mode = start_x + 10.0f;
    const float c_prev = c_mode + 27.2f;
    const float c_play = c_prev + 23.8f;
    const float c_next = c_play + 25.0f;
    const float x0 = c_mode - 14.0f;
    const float x1 = (c_mode + c_prev) * 0.5f;
    const float x2 = (c_prev + c_play) * 0.5f;
    const float x3 = (c_play + c_next) * 0.5f;
    const float x4 = c_next + 14.0f;
    // 直接委托给 4 个独立小组件渲染
    play_mode_widget_.render(dl, ImVec2(c_mode, center_y), ImVec2(x1 - x0, 36.0f));
    prev_widget_.render(dl, ImVec2(c_prev, center_y), ImVec2(x2 - x1, 36.0f));
    play_pause_widget_.render(dl, ImVec2(c_play, center_y), ImVec2(x3 - x2, 36.0f));
    next_widget_.render(dl, ImVec2(c_next, center_y), ImVec2(x4 - x3, 36.0f));
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
    float center_x = (left_x + right_x) * 0.5f;

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

    // 渲染液态玻璃的底层
    drawCapsuleBackground(dl, ImVec2(left_x, top_y), ImVec2(right_x, bot_y), rounding);

    // 左区：播放操作控制块
    renderLeftControls(dl, left_x + rounding + 4.0f, center_y);

    // 中区：LED 律动频谱仪 (截图红框位置)
    spectrum_widget_.render(dl, ImVec2(center_x, center_y));

    // 右区：发烧音量调节组件 (小喇叭 + 可拖拽滑块条 + 百分比)
    float right_limit = right_x - rounding - 4.0f;
    volume_widget_.render(dl, right_limit, center_y);

    ImGui::End();
    ImGui::PopStyleVar();
}