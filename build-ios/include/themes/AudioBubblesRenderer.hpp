#pragma once

#include "imgui.h"
#include <vector>

// ==============================================================================
// 音乐律动微光气泡纯代码矢量渲染引擎 (AudioBubblesRenderer)
// 全屏浮动半透明液态玻璃气泡，随低频重音膨胀呼吸，随中高频升腾与微光闪烁
// ==============================================================================
class AudioBubblesRenderer {
public:
    AudioBubblesRenderer();
    ~AudioBubblesRenderer() = default;

    // 渲染全屏音乐微光气泡动效
    void render(float screen_w, float screen_h, bool is_playing, const float* spectrum_12);

    void setTheme(int theme_id) { theme_id_ = theme_id; }
    void setCustomColor(ImVec4 color) { custom_color_ = color; }

private:
    struct Bubble {
        float x = 0.0f;
        float y = 0.0f;
        float base_radius = 20.0f;
        float current_radius = 20.0f;
        float vy = 25.0f;          // 浮升速度 (像素/秒)
        float vx_phase = 0.0f;     // 横向游动相位
        float vx_speed = 1.0f;     // 横向摇摆角速度
        float vx_amp = 18.0f;      // 横向漂移振幅
        float alpha = 0.6f;        // 基础半透明度
        int freq_band = 0;         // 频段亲和度 (0:超重低音, 1:中低频, 2:中频, 3:高频)
    };

    void initBubbles(float screen_w, float screen_h);
    void updatePhysics(float dt, float screen_w, float screen_h, bool is_playing, const float* spectrum_12);

private:
    std::vector<Bubble> bubbles_;
    bool initialized_ = false;
    int theme_id_ = 0;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    float last_time_ = 0.0f;
};
