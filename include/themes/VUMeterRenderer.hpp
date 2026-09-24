#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

// ==============================================================================
// 主题扩展接口 (TODO: 基础功能闭环后，在此处扩展金嗓子 Accuphase、复古卡座等名机视觉)
// ==============================================================================

enum class MeterThemeType {
    McIntosh,   // 当前基准：经典麦景图湖蓝深空表头
    Accuphase,  // 规划扩展：旗舰金嗓子香槟金白炽暖光表头
    RetroTape   // 规划扩展：复古琥珀色磁带机表头
};

class VUMeterRenderer{
public:
    VUMeterRenderer();
    ~VUMeterRenderer() = default;

    // 渲染双通道动圈表头 (满屏覆盖 1024x600)
    void render(float screen_w, float screen_h, float raw_level_l, float raw_level_r);

    // 主题切换接口 (已为后续主题预留)
    void setTheme(MeterThemeType theme) {current_theme_ = theme;}
    MeterThemeType getTheme() const { return current_theme_; }

private:
    // 动圈物理模拟：非对称阻尼计算 (Attack 迅猛 ~10ms，Decay 惯性下坠 ~300ms)
    void updateBallistics(float target_l,float target_r);

    // 绘制单个独立声道的麦景图表盘
    void drawSingleMeter(ImDrawList *dl,ImVec2 p_min,ImVec2 p_max,float angle,const char *channel_label);

    // 绘制麦景图经典刻度线、dB 刻度字与高光弧线
    void drawScaleAndTicks(ImDrawList *dl, ImVec2 center, float radius, float start_angle, float total_sweep);

    // 绘制流线型动圈金属细针与旋转轴心金属盖
    void drawNeedle(ImDrawList *dl,ImVec2 center,float radius,float angle);

private:   
    MeterThemeType current_theme_ = MeterThemeType::McIntosh;

    // 动圈指针物理状态 (弧度制)
    float needle_angle_l_ = -2.356f; // 当前左针弧度 (静止位置约 -135°)
    float needle_angle_r_ = -2.356f; // 当前右针弧度

    // 表头弧度常数 constexpr 编译时确认，减少单片机内存
    static constexpr float kMinAngle = -2.356f; // -135度
    static constexpr float kMaxAngle = -0.785f; // -45度
    static constexpr float kSweepAngle = 1.571f; // 摆幅 90度 
};