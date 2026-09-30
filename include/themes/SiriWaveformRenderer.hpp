#pragma once

#include "imgui.h"

// ==============================================================================
// Siri 流光声波纯代码矢量渲染引擎 (SiriWaveformRenderer)
// 基于 Apple Siri 经典流体声波 (多层半透明平滑正弦波交叠，高斯渐隐包络与高光核心)
// ==============================================================================
class SiriWaveformRenderer {
public:
    SiriWaveformRenderer();
    ~SiriWaveformRenderer() = default;

    // 渲染全屏 Siri 多层流体声波动态背景
    void render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    void drawWaveRibbon(ImDrawList* dl, float center_x, float center_y, float width,
                        float amp, float freq, float phase, float speed_t,
                        ImU32 col_top, ImU32 col_bot, float max_half_h);

private:
    float anim_time_ = 0.0f;
    float smooth_energy_ = 0.0f;
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    float last_time_ = 0.0f;
};
