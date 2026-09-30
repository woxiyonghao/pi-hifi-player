#pragma once

#include "imgui.h"

// ==============================================================================
// 玻璃质感渲染引擎 (Glass Card Renderer)
// 提供发烧级【毛玻璃 Frosted Glass】与【液态玻璃 Liquid Glass】渲染
// ==============================================================================
enum class GlassStyle {
    FrostedGlass = 0, // 全局毛玻璃：高密度漫射磨砂深空底、柔化底层动态频谱、纯正哑光抗眩光
    LiquidGlass       // 全局液态玻璃：曲面高光流动渐变、顶部晶莹高光条、双层微光折射边框
};

class GlassCardRenderer {
public:
    // 获取当前全局风格 (默认 FrostedGlass 毛玻璃)
    static GlassStyle getStyle();
    // 设置全局风格
    static void setStyle(GlassStyle style);

    // 绘制纯正【毛玻璃 (Frosted Glass)】
    static void drawFrosted(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding);

    // 绘制纯正【液态玻璃 (Liquid Glass)】
    static void drawLiquid(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding);

    // 统一绘制入口 (默认毛玻璃)
    static void drawCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, const char* panel_role = nullptr);
};
