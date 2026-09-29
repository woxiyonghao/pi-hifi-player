#pragma once

#include "imgui.h"
#include "types/DACProfile.hpp"
#include <functional>
#include <string>

// ==============================================================================
// 动态多架构硬件 DAC 解码前级设置视图 (DACSettingView)
// 支持根据硬件/选定芯片动态分发专属设置项（ES9038PRO / AK4499EX / CS43198 / 通用I2S）
// ==============================================================================
class DACSettingView {
public:
    using OnDacChangedCallback = std::function<void(DACType, const std::string&)>;

    DACSettingView();
    ~DACSettingView() = default;

    void render(float x, float y, float w, float h);

    void setOnDacChanged(OnDacChangedCallback cb) { on_dac_changed_ = std::move(cb); }
    [[nodiscard]] DACType getCurrentDacType() const { return current_dac_type_; }
    [[nodiscard]] std::string getCurrentDacName() const { return getDACProfileInfo(current_dac_type_).model_name; }
    void setCurrentDacType(DACType type);

private:
    void loadSettings();
    void saveSettings();

    // 硬件型号切换胶囊栏
    void renderHardwareSelector(ImDrawList* dl, float x0, float y0, float w);

    // 针对不同芯片架构的专属渲染入口
    void renderES9038View(ImDrawList* dl, float x0, float y0, float w);
    void renderAK4499EXView(ImDrawList* dl, float x0, float y0, float w);
    void renderCS43198View(ImDrawList* dl, float x0, float y0, float w);
    void renderGenericI2SView(ImDrawList* dl, float x0, float y0, float w);

    // ES9038 专有板块
    void renderPCMFilterSection(ImDrawList* dl, float x0, float y0, float w);
    void renderDSDFilterSection(ImDrawList* dl, float x0, float y0, float w);
    void renderDPLLSection(ImDrawList* dl, float x0, float y0, float w);
    void renderHarmonicsAndOutputSection(ImDrawList* dl, float x0, float y0, float w);

    // AK4499EX 专有板块
    void renderAKMVelvetFilterSection(ImDrawList* dl, float x0, float y0, float w);
    void renderAKMSoundColorSection(ImDrawList* dl, float x0, float y0, float w);
    void renderAKMDSDAndOutputSection(ImDrawList* dl, float x0, float y0, float w);

    // CS43198 专有板块
    void renderCSFiltersAndNOSSection(ImDrawList* dl, float x0, float y0, float w);
    void renderCSGainAndOutputSection(ImDrawList* dl, float x0, float y0, float w);

    // 通用 I2S 专有板块
    void renderGenericAudioSection(ImDrawList* dl, float x0, float y0, float w);

private:
    DACType current_dac_type_ = DACType::ES9038PRO;
    OnDacChangedCallback on_dac_changed_;

    // --- 1. ESS ES9038PRO 专属参数 ---
    int pcm_filter_mode_ = 2; // 0~6: 快速最小相位等
    int dsd_bypass_mode_ = 0; // 0: Direct 1-Bit, 1: FIR
    int dsd_filter_cutoff_ = 0; // 0: 47.7kHz, 1: 50kHz, 2: 60kHz, 3: 70kHz
    int pcm_dpll_band_ = 0; // 0: 极窄带, 1: 平衡, 2: 宽带
    int dsd_dpll_band_ = 0;
    int thd_comp_mode_ = 0; // 0: 超低失真, 1: 二次谐波(胆味), 2: 关闭
    int channel_mode_ = 0; // 0: Dual Mono 8ch, 1: 全平衡立体声
    int output_level_mode_ = 0; // 0: Line-Out (4.2V), 1: Pre-Out
    int phase_invert_ = 0; // 0: 正相, 1: 反相

    // --- 2. AKM AK4499EX 专属参数 ---
    int ak_filter_mode_ = 2; // 0~5: 0=Sharp, 1=Slow, 2=Short Delay Sharp, 3=Short Delay Slow, 4=Super Slow, 5=Low Dispersion
    int ak_sound_color_ = 0; // 0: Sound 1 (Natural), 1: Sound 2 (Warm), 2: Sound 3 (Dynamic), 3: Sound 4 (Analogue)
    int ak_dsd_bypass_mode_ = 0; // 0: DSD Direct, 1: Normal FIR
    int ak_dsd_filter_cutoff_ = 0; // 0: 39kHz, 1: 76kHz, 2: 150kHz
    int ak_output_mode_ = 0; // 0: Line-Out, 1: Pre-Out
    int ak_phase_invert_ = 0;

    // --- 3. Cirrus CS43198 专属参数 ---
    int cs_filter_mode_ = 4; // 0~4: 0=Fast Linear, 1=Slow Linear, 2=Fast Min, 3=Slow Min, 4=NOS(无过采样)
    int cs_dsd_mode_ = 0; // 0: Direct DSD, 1: Filtered DSD
    int cs_headphone_gain_ = 0; // 0: 自适应阻抗检测, 1: 低阻高敏, 2: 高阻高增益
    int cs_output_mode_ = 0;

    // --- 4. 标准通用 I2S / 平台声卡专属参数 ---
    int gen_bit_perfect_ = 0; // 0: 开启 Bit-Perfect 独占直通, 1: 系统共享混音
    int gen_bit_depth_ = 0; // 0: Auto Native, 1: 16-bit, 2: 24-bit, 3: 32-bit Float
    int gen_output_mode_ = 0; // 0: Line-Out, 1: Pre-Out
    int gen_phase_invert_ = 0;
};
