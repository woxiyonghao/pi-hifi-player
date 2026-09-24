#pragma once

#include "PlayerAdmin.hpp"
#include "imgui.h"
#include <functional>

class LEDSpectrumWidget {
public:
    using Callback = std::function<void()>;

    LEDSpectrumWidget() = default;
    ~LEDSpectrumWidget() = default;

    // 渲染中区 LED 分段律动频谱矩阵
    // @param dl 绘制列表
    // @param center 频谱仪几何中心坐标
    // @param width 频谱仪总横宽 (默认 48.0f)
    // @param height 频谱仪总高度 (默认 20.0f)
    // @param on_click 点击回调 (默认触发 player.togglePlayPause())
    bool render(ImDrawList* dl, ImVec2 center, float width = 48.0f, float height = 20.0f, Callback on_click = nullptr);

    // 纯矢量分段式 LED 点阵绘制 (12列 x 10行)
    static void drawMatrix(ImDrawList* dl, ImVec2 center, float total_w, float total_h,
                           int num_cols, int num_rows, ImU32 lit_color, ImU32 unlit_color,
                           bool is_animating);
};