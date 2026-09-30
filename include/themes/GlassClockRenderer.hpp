#pragma once

#include "imgui.h"

// ==============================================================================
// iPhone 待机液态玻璃大时钟渲染引擎 (GlassClockRenderer)
// 还原 iOS StandBy 悬浮通透液态玻璃时钟，主题色微光折射倒角，随音乐低频呼吸律动
// ==============================================================================
class GlassClockRenderer {
public:
    GlassClockRenderer();
    ~GlassClockRenderer() = default;

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
