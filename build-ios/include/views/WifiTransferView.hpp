#pragma once

#include "imgui.h"
#include <string>

// ==============================================================================
// 纯音数播 WiFi 无线传歌主视图 (WifiTransferView)
// 支持在屏幕上掌控 HTTP 传歌服务开关、动态展示文件流式上传进度与曲库自动入库历史
// ==============================================================================
class WifiTransferView {
public:
    WifiTransferView();
    ~WifiTransferView() = default;

    // 渲染无线传歌主界面 (位于主舞台中央)
    void render(float x, float y, float w, float h);

private:
    void renderServerCard(ImDrawList* dl, float x0, float y0, float w);
    void renderLiveProgressCard(ImDrawList* dl, float x0, float y0, float w);
    void renderHistoryCard(ImDrawList* dl, float x0, float y0, float w, float h);

    float anim_pulse_ = 0.0f;
};
