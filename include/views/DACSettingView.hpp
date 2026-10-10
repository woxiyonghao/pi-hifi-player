#pragma once

#include "imgui.h"
#include "types/DACProfile.hpp"
#include <functional>
#include <string>

// ==============================================================================
// 全球主流旗舰发烧 DAC 芯片与硬件直通设置视图 (DACSettingView)
// 适配 Apple Direct (MacBook Pro M1 Pro 硬件直通)、ESS Sabre (ES9038PRO并联)、
// AKM 旭化成 (AK4499EX Velvet Sound)、Cirrus Logic (CS43198 MasterHIFI)、
// R-2R 纯分立梯形电阻网络 (NOS/OS)、ROHM 罗姆 (MUS-IC BD34301EKV)
// ==============================================================================
class DACSettingView {
public:
    using OnDacChangedCallback = std::function<void(DACType, const std::string&)>;

    static DACSettingView* getInstance();

    DACSettingView();
    ~DACSettingView();

    void render(float x, float y, float w, float h);

    void setOnDacChanged(OnDacChangedCallback cb) { on_dac_changed_ = std::move(cb); }
    [[nodiscard]] int getSelectedChip() const { return selected_chip_; }
    [[nodiscard]] DACType getCurrentDacType() const { return static_cast<DACType>(selected_chip_); }
    [[nodiscard]] std::string getCurrentChipName() const;
    [[nodiscard]] std::string getCurrentDacName() const { return getCurrentChipName(); }
    [[nodiscard]] bool isDacConnected() const;

    void setSelectedChip(int index);
    void setCurrentDacType(DACType type) { setSelectedChip(static_cast<int>(type)); }

    int getParam(const std::string& key) const;
    void setParam(const std::string& key, int val);

private:
    void loadSettings();
    void saveSettings();

    // 辅助按钮组渲染工具 (防字体溢出与自适应排版)
    void renderOptionRow(ImDrawList* dl, float x0, float row_y, float w,
                         const char* label, const char* const options[], int count,
                         int& current_val, const char* id_prefix);

    // 全局硬件输出与独占流状态条 (所有芯片共享)
    void renderHardwareStatusBar(ImDrawList* dl, float x0, float& cur_y, float w);

    // 芯片专属发烧设置子页面
    void renderHardwareDeviceSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderAppleDirectSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderAAudioDirectSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderESSSabreSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderAKMVelvetSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderCirrusSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderR2RSettings(ImDrawList* dl, float x0, float& cur_y, float w);
    void renderROHMSettings(ImDrawList* dl, float x0, float& cur_y, float w);

private:
    // 当前选中的芯片架构索引 (0 ~ 5)
    // 0: Apple Direct / Android AAudio Direct / 树莓派物理声卡
    // 1: ESS Sabre (ES9038PRO / ES9039PRO)
    // 2: AKM 旭化成 (AK4499EX + AK4191EQ)
    // 3: Cirrus Logic (CS43198 / CS43131)
    // 4: R-2R 纯分立电阻网络 (NOS/OS)
    // 5: ROHM 罗姆 (MUS-IC BD34301EKV)
    int selected_chip_ = 0;
    OnDacChangedCallback on_dac_changed_;

    // --------------------------------------------------------------------------
    // 0. Apple Direct (MacBook Pro M1 Pro / macOS CoreAudio 硬件直通)
    // --------------------------------------------------------------------------
    int apple_exclusive_mode_ = 0;   // 0: Bit-Perfect 独占流, 1: 系统混音共享
    int apple_sample_rate_ = 0;      // 0: 原生跟随母带 (44.1k-192k), 1: 固定 96kHz, 2: 固定 192kHz
    int apple_headphone_drive_ = 0;  // 0: 智能阻抗自适应 (<150Ω/150-1kΩ/>1kΩ), 1: 强制高输出 3.0Vrms, 2: 标准输出 1.25Vrms
    int apple_bit_depth_ = 0;        // 0: 32-bit Float 直通, 1: 24-bit 整数定点

    // --------------------------------------------------------------------------
    // 0. Android AAudio (AAudio 硬件独占直通 & USB DAC 架构)
    // --------------------------------------------------------------------------
    int aaudio_exclusive_mode_ = 0;   // 0: Bit-Perfect 独占流, 1: 系统混音低延迟
    int aaudio_sample_rate_ = 0;      // 0: 原生跟随母带 (44.1k-768k/DSD), 1: 固定 48kHz (系统兼容), 2: 锁定 96kHz, 3: 锁定 192kHz
    int aaudio_bit_depth_ = 0;        // 0: 32-bit Float 浮点直通, 1: 24-bit 整数定点
    int aaudio_perf_mode_ = 0;        // 0: Low Latency 极低延迟, 1: Power Saving 均衡

    // --------------------------------------------------------------------------
    // 1. ESS Sabre (ES9038PRO 并联架构)
    // --------------------------------------------------------------------------
    int pcm_filter_mode_ = 2;        // 0: 快速最小, 1: 慢速最小, 2: 快速线性, 3: 慢速线性, 4: 变迹, 5: 砖墙, 6: 混合
    int dsd_bypass_mode_ = 0;        // 0: Direct 1-Bit 直通, 1: FIR 模拟滤波
    int dsd_filter_cutoff_ = 0;      // 0: 47.7kHz, 1: 50kHz, 2: 60kHz, 3: 70kHz
    int pcm_dpll_band_ = 0;          // 0: 极窄带, 1: 标准平衡, 2: 宽带
    int dsd_dpll_band_ = 0;          // 0: 极窄带, 1: 标准平衡, 2: 宽带
    int thd_comp_mode_ = 0;          // 0: 超低失真, 1: 二次谐波, 2: 关闭补偿
    int channel_mode_ = 0;           // 0: 双芯片 8-Ch 并联, 1: 双芯片立体声
    int output_level_mode_ = 0;      // 0: 固定后级 (Line-Out 4.2V), 1: 可调前级 (Pre-Out)
    int phase_invert_ = 0;           // 0: 绝对正相 (0°), 1: 极性反转 (180°)

    // --------------------------------------------------------------------------
    // 2. AKM 旭化成 (AK4499EX + AK4191EQ Velvet Sound Verita)
    // --------------------------------------------------------------------------
    int akm_filter_mode_ = 0;        // 0: 短延时锐滚降, 1: 短延时慢滚降, 2: 锐滚降, 3: 慢滚降, 4: 超低群延迟, 5: 低色散
    int akm_sound_color_ = 0;        // 0: 风格 1 自然温润, 1: 风格 2 细腻通透, 2: 风格 3 动感宽厚, 3: 风格 4 极简监听
    int akm_dsd_mode_ = 0;           // 0: Direct 旁路调制, 1: Normal 滤波模式
    int akm_exdf_mode_ = 0;          // 0: 外部超采样直通, 1: 内部 AK4191 处理

    // --------------------------------------------------------------------------
    // 3. Cirrus Logic (CS43198 / CS43131 MasterHIFI)
    // --------------------------------------------------------------------------
    int cs_filter_mode_ = 2;         // 0: 快速最小, 1: 慢速最小, 2: 快速线性, 3: 慢速线性, 4: NOS 无过采样
    int cs_dsd_mode_ = 0;            // 0: Direct DSD 直通, 1: DoP 硬件解调
    int cs_drive_mode_ = 0;          // 0: 高推力伪差分 (2Vrms), 1: 标准单端 (1Vrms)
    int cs_impedance_mode_ = 0;      // 0: 智能自适应 (16Ω~600Ω), 1: 高阻监听优先

    // --------------------------------------------------------------------------
    // 4. R-2R 纯分立电阻网络 (NOS / OS 架构)
    // --------------------------------------------------------------------------
    int r2r_mode_ = 0;               // 0: NOS 纯无过采样, 1: OS 线性相位, 2: OS 最小相位
    int r2r_dsd_mode_ = 0;           // 0: 独立 1-Bit 纯电阻网络, 1: 转为 24-bit 阶梯解码
    int r2r_clock_mode_ = 0;         // 0: 飞秒 FIFO 本地重整, 1: 直接跟随输入时钟
    int r2r_phase_mode_ = 0;         // 0: 绝对正相 (0°), 1: 极性反转 (180°)

    // --------------------------------------------------------------------------
    // 5. ROHM 罗姆 (MUS-IC BD34301EKV 旗舰)
    // --------------------------------------------------------------------------
    int rohm_filter_mode_ = 0;       // 0: Sharp Roll-Off (高解析宏大声场), 1: Slow Roll-Off (浓郁宽松自然感)
    int rohm_modulator_clock_ = 0;   // 0: 智能倍频匹配, 1: 锁定 64x fs, 2: 锁定 128x fs
    int rohm_dsd_path_ = 0;          // 0: Direct Path 纯模拟通道, 1: 标准多级滤波
};
