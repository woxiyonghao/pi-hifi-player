#pragma once

#include "imgui.h"

// ==============================================================================
// Siri 悬浮微光玻璃球渲染引擎 (SiriOrbRenderer)
// 外层毛玻璃球体与边缘菲涅尔高光采用当前主题色，里层保留 Apple 经典多彩流体等离子光团
// ==============================================================================
class SiriOrbRenderer {
public:
    SiriOrbRenderer();
    ~SiriOrbRenderer() = default;

    // 渲染全屏 Siri 3D 玻璃球动态背景
    void render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    float anim_time_ = 0.0f;
    float smooth_energy_ = 0.0f;
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    float last_time_ = 0.0f;
};
