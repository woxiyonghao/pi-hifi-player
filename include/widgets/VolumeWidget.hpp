#pragma once

#include "PlayerAdmin.hpp"
#include "imgui.h"

class VolumeWidget {
public:
    VolumeWidget() = default;
    ~VolumeWidget() = default;

    // 渲染右区发烧级音量调节模块 (小喇叭 + 拖拽滑块条 + 百分比)
    // @param dl 绘制列表
    // @param right_limit 右侧几何贴边 X 坐标
    // @param center_y 垂直中心 Y 坐标
    void render(ImDrawList* dl, float right_limit, float center_y);

    // 纯矢量小喇叭绘制 (纯几何抗锯齿，支持小音量单弧、高音量双弧、静音叉号)
    static void drawSpeaker(ImDrawList* dl, ImVec2 center, float vol, bool is_muted, ImU32 color);
};