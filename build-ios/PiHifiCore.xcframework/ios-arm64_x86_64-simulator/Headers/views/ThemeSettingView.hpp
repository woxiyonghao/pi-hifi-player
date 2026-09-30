#pragma once

#include "imgui.h"

// ==============================================================================
// 纯音数播发烧视觉主题设置面板视图 (ThemeSettingView)
// 管控 4 大经典名机预设、全景背景律动模式切换与发烧级自由调色中枢
// ==============================================================================
class ThemeSettingView {
public:
    ThemeSettingView() = default;
    ~ThemeSettingView() = default;

    // 渲染主题设置主面板
    void render(float x, float y, float w, float h);
};
