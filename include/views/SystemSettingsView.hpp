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
    using IdleFullscreenCallback = std::function<void(float)>;

    SystemSettingsView();
    ~SystemSettingsView() = default;

    // 渲染系统设置面板主入口 (位于主舞台中央)
    void render(float x, float y, float w, float h);

    // 导航回调 (支持从设置快捷跳转到扫描音乐等功能)
    void setOnNavigateTab(NavigateTabCallback cb) { on_navigate_tab_ = cb; }

    // 空余时间全屏沉浸超时变动回调
    void setOnIdleFullscreenChanged(IdleFullscreenCallback cb) { on_idle_fullscreen_changed_ = std::move(cb); }

    // 获取空余时间全屏秒数 (0 代表从不)
    float getIdleFullscreenSeconds() const {
        switch (idle_fullscreen_mode_) {
            case 0: return 15.0f;  // 15 秒 (默认)
            case 1: return 30.0f;  // 30 秒
            case 2: return 60.0f;  // 1 分钟
            case 3: return 300.0f; // 5 分钟
            case 4: default: return 0.0f; // 永不
        }
    }
    int getIdleFullscreenMode() const { return idle_fullscreen_mode_; }

private:
    void loadSettings();
    void saveSettings();

    enum class ConfirmAction {
        None,
        Reboot,
        Shutdown
    };

    // 内部模块卡片渲染闭包
    void renderAudioSection(ImDrawList* dl, float x0, float y0, float w);
    void renderHardwareSection(ImDrawList* dl, float x0, float y0, float w);
    float renderUpdateSection(ImDrawList* dl, float x0, float y0, float w);
    float renderPowerSection(ImDrawList* dl, float x0, float y0, float w);
    void renderPowerConfirmModal();

    static void applyHardwareBufferSize(int mode);
    static void applyFadeDuration(int mode);
    static void applyCpuGovernor(int mode);
    static void applyScreenBrightness(float brightness);

private:
    // 音频核心配置
    int sample_rate_mode_ = 0; // 0: Bit-Perfect 源码直出, 1: 升频 192kHz, 2: 极频 384kHz
    int dsd_mode_ = 0;         // 0: DoP (DSD over PCM), 1: Native 原生直通, 2: DSD 转 PCM
    int buffer_size_mode_ = 1; // 0: 64帧 (极低延迟), 1: 256帧 (标准平稳), 2: 512帧 (抗抖动)
    int fade_duration_mode_ = 2; // 0: 关闭, 1: 0.3秒, 2: 0.5秒 (默认发烧标准), 3: 1.0秒

    // 硬件与系统配置
    int cpu_governor_ = 0;     // 0: Performance (纯音锁频), 1: Schedutil (动态平衡)
    float screen_brightness_ = 0.85f; // 10% ~ 100%
    int screen_timeout_mode_ = 0;     // 0: 从不, 1: 5分钟, 2: 15分钟, 3: 30分钟
    int idle_fullscreen_mode_ = 0;    // 0: 15秒 (默认), 1: 30秒, 2: 1分钟, 3: 5分钟, 4: 永不

    NavigateTabCallback on_navigate_tab_;
    IdleFullscreenCallback on_idle_fullscreen_changed_;

    ConfirmAction confirm_action_ = ConfirmAction::None;
    std::string power_status_msg_;
    float network_refresh_feedback_timer_ = 0.0f;
};
