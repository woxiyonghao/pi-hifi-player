
#pragma once

#include "tools/PlayerAdmin.hpp"
#include "public/UIConfig.hpp"
#include "imgui.h"
#include "widgets/PlayModeWidget.hpp"
#include "widgets/PrevTrackWidget.hpp"
#include "widgets/PlayPauseWidget.hpp"
#include "widgets/NextTrackWidget.hpp"
#include "widgets/VolumeWidget.hpp"
class BottomBarView {
  public:
    BottomBarView();
    ~BottomBarView() = default;

    // 渲染底部播放控制栏 (支持四角屏保动画偏移)
    void render(float screen_w = 1024.0f, float screen_h = 600.0f, float offset_x = 0.0f, float offset_y = 0.0f);

    float getHeight() const { return height_; }
    void setHeight(float h) { height_ = h; }

  private:
    float height_ = 48.0f; // 胶囊高度

    // 渲染底色
    void drawCapsuleBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding);
    // 中区：渲染优雅音频进度条与时间轴 (支持触控与鼠标拖拽 Seek)
    void renderProgressBar(ImDrawList* dl, float left_bound, float right_bound, float center_y);
    // 左区：播放模式 -> 上一曲 -> 播放/暂停 -> 下一曲
    void renderLeftControls(ImDrawList* dl, float start_x, float center_y);

    // 播放模式
    PlayModeWidget play_mode_widget_;
    // 上一曲
    PrevTrackWidget prev_widget_;
    // 播放/暂停
    PlayPauseWidget play_pause_widget_;
    // 下一曲
    NextTrackWidget next_widget_;
    // 音量按钮
    VolumeWidget volume_widget_;
};