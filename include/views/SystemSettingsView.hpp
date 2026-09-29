#pragma once

#include "imgui.h"
#include <string>
#include <functional>

// ==============================================================================
// 纯音数播系统设置主视图 (SystemSettingsView)
// 管控音频输出时钟模式、DSD 策略、缓冲延迟、硬件调频、屏幕背光与曲库维护
// ==============================================================================
class SystemSettingsView {
public:
    using NavigateTabCallback = std::function<void(int tab_id)>;

    SystemSettingsView();
    ~SystemSettingsView() = default;

    // 渲染系统设置面板主入口 (位于主舞台中央)
    void render(float x, float y, float w, float h);

    // 导航回调 (支持从设置快捷跳转到扫描音乐等功能)
    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = cb; }

private:
    void loadSettings();
    void saveSettings();

    // 内部模块卡片渲染闭包
    void renderAudioSection(ImDrawList* dl, float x0, float y0, float w);
    void renderHardwareSection(ImDrawList* dl, float x0, float y0, float w);
    void renderLibrarySection(ImDrawList* dl, float x0, float y0, float w);
    void renderPowerSection(ImDrawList* dl, float x0, float y0, float w);

    // 提示弹窗/Toast
    void showToast(const std::string& msg);
    void renderToast(ImDrawList* dl, float card_x0, float card_y0, float card_w, float card_h);

private:
    // 音频核心配置
    int sample_rate_mode_ = 0; // 0: Bit-Perfect 源码直出, 1: 升频 192kHz, 2: 极频 384kHz
    int dsd_mode_ = 0;         // 0: DoP (DSD over PCM), 1: Native 原生直通, 2: DSD 转 PCM
    int buffer_size_mode_ = 1; // 0: 64帧 (极低延迟), 1: 256帧 (标准平稳), 2: 512帧 (抗抖动)

    // 硬件与系统配置
    int cpu_governor_ = 0;     // 0: Performance (纯音锁频), 1: Schedutil (动态平衡)
    float screen_brightness_ = 0.85f; // 10% ~ 100%
    int screen_timeout_mode_ = 0;     // 0: 从不, 1: 5分钟, 2: 15分钟, 3: 30分钟

    // 状态提示
    std::string toast_msg_;
    float toast_timer_ = 0.0f;

    NavigateTabCallback on_navigate_tab_;
};
