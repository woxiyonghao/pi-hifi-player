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
// 1.5 渲染中间发烧级音频进度条与时间轴 (支持触控与拖拽 Seek)
// ==============================================================================
void BottomBarView::renderProgressBar(ImDrawList* dl, float left_bound, float right_bound, float center_y) {
    auto& player = PlayerAdmin::getInstance();
    double cur_time = player.getCurrentTimeSec();
    double dur_time = player.getDurationSec();
    float progress = std::clamp(player.getProgress(), 0.0f, 1.0f);

    int cur_sec = static_cast<int>(std::max(0.0, cur_time));
    int dur_sec = static_cast<int>(std::max(0.0, dur_time));

    char cur_buf[16];
    char dur_buf[16];
    std::snprintf(cur_buf, sizeof(cur_buf), "%02d:%02d", cur_sec / 60, cur_sec % 60);
    if (dur_sec > 0) {
        std::snprintf(dur_buf, sizeof(dur_buf), "%02d:%02d", dur_sec / 60, dur_sec % 60);
    } else {
        std::snprintf(dur_buf, sizeof(dur_buf), "--:--");
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 cur_sz = ImGui::CalcTextSize(cur_buf);
    ImVec2 dur_sz = ImGui::CalcTextSize(dur_buf);

    float time_pad = 8.0f;
    float track_x0 = left_bound + cur_sz.x + time_pad;
    float track_x1 = right_bound - dur_sz.x - time_pad;
    float track_w = track_x1 - track_x0;

    if (track_w < 50.0f) {
        if (Fonts::Small) ImGui::PopFont();
        return;
    }

    // 交互响应热区
    ImGui::SetCursorScreenPos(ImVec2(track_x0 - 4.0f, center_y - 14.0f));
    ImGui::InvisibleButton("##BottomBarTrackSeekBtn", ImVec2(track_w + 8.0f, 28.0f));
    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();

    if (is_active && dur_time > 0.0) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float new_progress = std::clamp((mouse_x - track_x0) / track_w, 0.0f, 1.0f);
        player.seek(static_cast<double>(new_progress) * dur_time);
        progress = new_progress;
    }

    // 1. 轨道底槽
    const float track_h = 4.0f;
    const float track_y = center_y - track_h * 0.5f;
    dl->AddRectFilled(ImVec2(track_x0, track_y), ImVec2(track_x1, track_y + track_h), 
                      IM_COL32(255, 255, 255, 30), 2.0f);

    // 2. 激活进度填充
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float fill_x = track_x0 + track_w * progress;
    if (fill_x > track_x0 + 1.0f) {
        // 微光晕
        if (UIConfig::Animation::EnableGlow && (is_hovered || is_active || player.isPlaying())) {
            float glow_int = UIConfig::Animation::GlowIntensity;
            int a_glow = std::clamp(static_cast<int>(35.0f * glow_int), 0, 255);
            dl->AddRectFilled(ImVec2(track_x0 - 1.0f, track_y - 1.5f), 
                              ImVec2(fill_x + 1.0f, track_y + track_h + 1.5f), 
                              IM_COL32(r, g, b, a_glow), 3.0f);
        }
        dl->AddRectFilled(ImVec2(track_x0, track_y), ImVec2(fill_x, track_y + track_h), 
                          accent, 2.0f);
    }

    // 3. 拖拽游标 / 播放指针
    float thumb_r = (is_hovered || is_active) ? 6.5f : 5.0f;
    ImVec2 thumb_pos(fill_x, center_y);
    // 阴影
    dl->AddCircleFilled(ImVec2(thumb_pos.x, thumb_pos.y + 1.0f), thumb_r + 1.5f, IM_COL32(0, 0, 0, 80));
    // 聚焦光环
    if (is_hovered || is_active) {
        dl->AddCircle(thumb_pos, thumb_r + 2.5f, accent, 24, 1.4f);
    }
    // 白色抛光实体
    dl->AddCircleFilled(thumb_pos, thumb_r, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(thumb_pos, thumb_r, IM_COL32(200, 215, 235, 180), 24, 1.0f);

    // 4. 两侧时间标签
    ImU32 time_col = (is_hovered || is_active) ? UIConfig::Color::TextNormal : UIConfig::Color::TextMuted;
    dl->AddText(ImVec2(left_bound, center_y - cur_sz.y * 0.5f), time_col, cur_buf);
    dl->AddText(ImVec2(right_bound - dur_sz.x, center_y - dur_sz.y * 0.5f), time_col, dur_buf);

    if (Fonts::Small) ImGui::PopFont();
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

    // 2. 左区：播放操作控制块 (最高交互优先级：模式、上一曲、播放/暂停、下一曲)
    renderLeftControls(dl, left_x + rounding + 4.0f, center_y);

    // 3. 中区：发烧音频进度条与时间轴 (位于左区按键与右区音量条之间的中央区域)
    float prog_left = left_x + 168.0f;
    float prog_right = right_x - 200.0f;
    if (prog_right > prog_left) {
        renderProgressBar(dl, prog_left, prog_right, center_y);
    }

    // 4. 右区：发烧音量调节组件 (最高交互优先级：小喇叭静音、音量拖拽滑块条)
    float right_limit = right_x - rounding - 4.0f;
    volume_widget_.render(dl, right_limit, center_y);

    ImGui::End();
    ImGui::PopStyleVar();
}