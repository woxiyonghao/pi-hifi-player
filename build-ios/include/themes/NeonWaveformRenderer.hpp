#pragma once

#include "imgui.h"

// ==============================================================================
// 电光霓虹音频波浪渲染引擎 (NeonWaveformRenderer)
// 还原高光霓虹电光波形与背景多频段阴影光柱，光晕深度与振幅随音乐节拍激荡
// ==============================================================================
class NeonWaveformRenderer {
public:
    NeonWaveformRenderer();
    ~NeonWaveformRenderer() = default;

    void render(float screen_w, float screen_h, bool is_playing, const float* spectrum_levels, int num_levels);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    float anim_time_ = 0.0f;
    float smooth_levels_[16] = {0.0f};
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    float last_time_ = 0.0f;
};
