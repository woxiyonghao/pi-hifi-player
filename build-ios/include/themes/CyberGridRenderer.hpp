#pragma once

#include "imgui.h"

// ==============================================================================
// 3D 赛博粒子网格地形渲染引擎 (CyberGridRenderer)
// 透视投影粒子矩阵地形网格，随多频段音乐动态起伏激荡，深度雾化沉浸空间
// ==============================================================================
class CyberGridRenderer {
public:
    CyberGridRenderer();
    ~CyberGridRenderer() = default;

    void render(float screen_w, float screen_h, bool is_playing, const float* spectrum_levels, int num_levels);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    float anim_time_ = 0.0f;
    float smooth_levels_[16] = {0.0f};
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.0f, 0.71f, 0.94f, 1.0f);
    float last_time_ = 0.0f;
};
