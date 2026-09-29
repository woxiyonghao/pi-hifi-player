#pragma once

#include "imgui.h"
#include <string>

// ==============================================================================
// 双 ES9038PRO 旗舰并联全平衡解码前级设置视图 (DACSettingView)
// 适配 ESS SABRE 旗舰并联架构：7 种硬件 FIR 滤波、DSD模拟低通、DPLL飞秒锁相环、THD谐波补偿与并联模式
// ==============================================================================
class DACSettingView {
public:
    DACSettingView();
    ~DACSettingView() = default;

    void render(float x, float y, float w, float h);

private:
    void loadSettings();
    void saveSettings();

    // 内部四大发烧设置板块
    void renderPCMFilterSection(ImDrawList* dl, float x0, float y0, float w);
    void renderDSDFilterSection(ImDrawList* dl, float x0, float y0, float w);
    void renderDPLLSection(ImDrawList* dl, float x0, float y0, float w);
    void renderHarmonicsAndOutputSection(ImDrawList* dl, float x0, float y0, float w);

private:
    // 1. PCM 硬件数字滤波器滚降特性 (0 ~ 6)
    // 0: 快速线性 (Fast Linear)
    // 1: 慢速线性 (Slow Linear)
    // 2: 快速最小 (Fast Minimum)
    // 3: 慢速最小 (Slow Minimum)
    // 4: 变迹滤波 (Apodizing)
    // 5: 砖墙滤波 (Brickwall)
    // 6: 混合滤波 (Hybrid)
    int pcm_filter_mode_ = 2; // 默认：快速最小相位 (现代发烧流行人声推荐)

    // 2. DSD 模拟低通滤波与直通
    // dsd_bypass_mode_: 0: Direct 1-Bit 直通 (最高纯度), 1: 正常 FIR 模拟滤波
    int dsd_bypass_mode_ = 0;
    // dsd_filter_cutoff_: 0: 47.7kHz (标准), 1: 50kHz, 2: 60kHz, 3: 70kHz
    int dsd_filter_cutoff_ = 0;

    // 3. ESS 专利 DPLL 飞秒抖动消除器 (Jitter Eliminator)
    // pcm_dpll_band_: 0: 极窄带 (超低抖动), 1: 标准平衡, 2: 宽带 (抗时钟失锁)
    int pcm_dpll_band_ = 0;
    // dsd_dpll_band_: 0: 极窄带 (超低抖动), 1: 标准平衡, 2: 宽带 (防爆音)
    int dsd_dpll_band_ = 0;

    // 4. THD 谐波补偿与双芯片并联架构
    // thd_comp_mode_: 0: 纯净超低失真 (< -122dB), 1: 二次偶次谐波增强 (模拟胆味), 2: 关闭补偿
    int thd_comp_mode_ = 0;
    // channel_mode_: 0: 双芯片 8 通道单声道并联 (Dual Mono 8ch, 140dB SNR), 1: 全平衡立体声
    int channel_mode_ = 0;
    // output_level_mode_: 0: 纯后级固定 Line-Out (4.2V), 1: 可调模拟前级 Pre-Out
    int output_level_mode_ = 0;
    // phase_invert_: 0: 绝对正相 (0°), 1: 极性反转 (180°)
    int phase_invert_ = 0;
};
