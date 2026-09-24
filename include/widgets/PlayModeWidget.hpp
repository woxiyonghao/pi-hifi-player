#pragma once

#include "PlayerAdmin.hpp"
#include "imgui.h"
#include <functional>

class PlayModeWidget {
public:
    using Callback = std::function<void()>;

    PlayModeWidget() = default;
    ~PlayModeWidget() = default;

    // 渲染播放模式按钮 (点击默认调用 player.cyclePlayMode()，也可传入自定义回调)
    bool render(ImDrawList* dl, ImVec2 center, ImVec2 size = ImVec2(36.0f, 36.0f), Callback on_click = nullptr);

    // 纯矢量模式图标绘制 (列表循环、单曲循环、随机播放、顺序播放)
    static void drawIcon(ImDrawList* dl, ImVec2 center, PlayMode mode, ImU32 color, float scale = 1.25f);
};