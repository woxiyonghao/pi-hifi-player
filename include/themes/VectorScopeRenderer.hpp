#pragma once

#include "imgui.h"

// ==============================================================================
// 立体声相位矢量示波器纯代码渲染引擎 (VectorScopeRenderer)
// 专业母带级 Lissajous / Goniometer 45度极坐标立体声场图与相位相关性读数
// ==============================================================================
class VectorScopeRenderer {
public:
    VectorScopeRenderer();
    ~VectorScopeRenderer() = default;

    // 渲染全屏立体声相位示波器动态背景
    void render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    float phase_angle_ = 0.0f;
    float smooth_l_ = 0.0f;
    float smooth_r_ = 0.0f;
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    float last_time_ = 0.0f;
};
