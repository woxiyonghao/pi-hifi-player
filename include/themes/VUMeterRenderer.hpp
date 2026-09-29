#pragma once

#include "imgui.h"
#include <algorithm>
#include <cmath>

// ==============================================================================
// 经典名机动圈表头主题视觉类型
// ==============================================================================
enum class MeterThemeType {
    McIntosh,      // 经典麦景图湖蓝深空表头 (Blue Eyes)
    Accuphase,     // 旗舰金嗓子香槟金白炽暖光表头 (Champagne Gold)
    RetroTape,     // 复古琥珀色磁带机表头 (Vintage Amber Tape)
    ModernCrimson, // 现代深空玫红极简发烧表头 (Modern Studio)
    Custom         // 发烧友自由调色大表头 (Custom Audiophile)
};

// ==============================================================================
// 动圈表头色彩与风格配置
// ==============================================================================
struct MeterPalette {
    ImU32 chassis_bg;
    ImU32 meter_bg_base;
    ImU32 glow_core;
    ImU32 glow_outer;
    ImU32 border;
    ImU32 arc_color;
    ImU32 tick_safe;
    ImU32 tick_text_safe;
    ImU32 overload_red;
    ImU32 needle_color;
    ImU32 needle_glow;
    ImU32 pivot_base;
    ImU32 pivot_ring;
    ImU32 footer_badge;
    const char* sub_label;
    const char* footer_left;
    const char* footer_right;
};

class VUMeterRenderer {
public:
    VUMeterRenderer();
    ~VUMeterRenderer() = default;

    // 渲染双通道动圈表头 (满屏覆盖 1024x600)
    void render(float screen_w, float screen_h, float raw_level_l, float raw_level_r);

    // 主题切换接口
    void setTheme(MeterThemeType theme) { current_theme_ = theme; }
    MeterThemeType getTheme() const { return current_theme_; }

    // 自定义颜色配置接口
    void setCustomColor(ImVec4 color) { custom_color_ = color; }
    ImVec4 getCustomColor() const { return custom_color_; }

private:
    // 动圈物理模拟：非对称阻尼计算 (Attack 迅猛 ~10ms，Decay 惯性下坠 ~300ms)
    void updateBallistics(float target_l, float target_r);

    // 绘制单个独立声道的名机动圈表盘
    void drawSingleMeter(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float angle, const char* channel_label, const MeterPalette& pal);

    // 绘制对应名机经典刻度线、dB 刻度字与高光弧线
    void drawScaleAndTicks(ImDrawList* dl, ImVec2 center, float radius, float start_angle, float total_sweep, const MeterPalette& pal);

    // 绘制流线型动圈金属细针与旋转轴心金属盖
    void drawNeedle(ImDrawList* dl, ImVec2 center, float radius, float angle, const MeterPalette& pal);

private:   
    MeterThemeType current_theme_ = MeterThemeType::McIntosh;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);

    // 动圈指针物理状态 (弧度制)
    float needle_angle_l_ = -2.356f; // 当前左针弧度 (静止位置约 -135°)
    float needle_angle_r_ = -2.356f; // 当前右针弧度

    // 表头弧度常数 constexpr 编译时确认
    static constexpr float kMinAngle = -2.356f;   // -135度
    static constexpr float kMaxAngle = -0.785f;   // -45度
    static constexpr float kSweepAngle = 1.571f;  // 摆幅 90度 
};