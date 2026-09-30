#pragma once

#include "tools/PlayerAdmin.hpp"
#include "imgui.h"
#include <functional>

class PlayPauseWidget {
public:
    using Callback = std::function<void()>;

    PlayPauseWidget() = default;
    ~PlayPauseWidget() = default;

    // 渲染播放/暂停按键 (播放中呈现双胶囊竖条 ⏸，暂停时呈现饱满圆润大三角 ▶)
    bool render(ImDrawList* dl, ImVec2 center, ImVec2 size = ImVec2(36.0f, 36.0f), Callback on_click = nullptr);

    static void drawIcon(ImDrawList* dl, ImVec2 center, bool is_playing, ImU32 color);
};