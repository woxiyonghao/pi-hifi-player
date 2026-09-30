#pragma once

#include "imgui.h"

// ==============================================================================
// 复古开盘磁带机纯代码矢量渲染引擎 (TapeReelRenderer)
// 致敬 Studer A820 / Revox B77 旗舰开盘机，双金属镂空大开盘轮实时走带旋转动效
// ==============================================================================
class TapeReelRenderer {
public:
    TapeReelRenderer();
    ~TapeReelRenderer() = default;

    // 渲染全屏复古开盘机动态背景
    void render(float screen_w, float screen_h, bool is_playing, float elapsed_sec);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    void drawReel(ImDrawList* dl, ImVec2 center, float radius, float tape_radius, float angle, bool is_left);
    void drawHeadBlock(ImDrawList* dl, ImVec2 center, float width, float height);

private:
    float reel_angle_ = 0.0f;
    float current_speed_ = 0.0f;
    int theme_id_ = 3;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.45f, 0.09f, 1.0f);
    float last_time_ = 0.0f;
};
