
#pragma once

#include "PlayerAdmin.hpp"
#include "UIConfig.hpp"
#include "imgui.h"
class BottomBarView {
  public:
    BottomBarView();
    ~BottomBarView() = default;

    // 渲染底部播放控制栏
    void render(float screen_w = 1024.0f, float screen_h = 600.0f);

    float getHeight() const { return height_; }
    void setHeight(float h) { height_ = h; }

  private:
    float height_ = 48.0f; // 胶囊高度

    // 渲染底色
    void drawCapsuleBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding);
    // 左区：播放模式 -> 上一曲 -> 播放/暂停 -> 下一曲
    void renderLeftControls(ImDrawList* dl, float start_x, float center_y);
    // 中区：App Icon (先绘制字母 "A"，点击一键切换全屏)
    void renderCenterIcon(ImDrawList* dl, float center_x, float center_y);
    // 4. 右区：音量调节条 (小喇叭 + 发烧可拖拽滑块 + 百分比)
    void renderRightVolume(ImDrawList* dl, float right_limit, float center_y);

};