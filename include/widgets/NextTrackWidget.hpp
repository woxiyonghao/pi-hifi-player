#pragma once

#include "PlayerAdmin.hpp"
#include "imgui.h"
#include <functional>

class NextTrackWidget {
public:
    using Callback = std::function<void()>;

    NextTrackWidget() = default;
    ~NextTrackWidget() = default;

    // 渲染下一曲按键 (双右向小圆角三角形 ▶▶)
    bool render(ImDrawList* dl, ImVec2 center, ImVec2 size = ImVec2(36.0f, 36.0f), Callback on_click = nullptr);

    static void drawIcon(ImDrawList* dl, ImVec2 center, ImU32 color);
};