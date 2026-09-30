#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

// ==============================================================================
// 日本发烧机皇金嗓子 (Accuphase) 双通道动圈大表头纯代码矢量渲染引擎
// 完全基于 ImDrawList 纯代码绘制，对齐原机双层水平梯形色带与对数分度
// ==============================================================================
class AccuphaseMeterRenderer {
public:
    AccuphaseMeterRenderer();
    ~AccuphaseMeterRenderer() = default;

    // 渲染双通道金嗓子大表头 (覆盖 1024x600 屏幕)
    void render(float screen_w, float screen_h, float raw_level_l, float raw_level_r);

    // 主题与色彩配置接口 (支持自由调色与名机预设全景联动)
    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    // 动圈物理模拟：非对称阻尼计算 (Attack ~12ms 迅猛冲顶, Decay ~280ms 惯性平滑回落)
    void updateBallistics(float target_l, float target_r);

    // 绘制单个独立声道金嗓子原机动圈表盘
    void drawSingleMeter(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float needle_val, const char* channel_label);

    // 绘制中央祖母绿 Accuphase 呼吸发光微标与经典数字音量指示
    void drawCenterDisplay(ImDrawList* dl, float center_x, float screen_h);

private:
    int theme_id_ = 2; // 默认金嗓子香槟金
    ImVec4 custom_color_ = ImVec4(0.88f, 0.78f, 0.57f, 1.0f);

    // 指针物理状态 (归一化位置 0.0f ~ 1.0f)
    float needle_val_l_ = 0.0f;
    float needle_val_r_ = 0.0f;
};
