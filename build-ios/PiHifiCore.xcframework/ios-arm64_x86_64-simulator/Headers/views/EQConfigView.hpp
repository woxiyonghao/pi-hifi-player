#pragma once

#include "imgui.h"
#include <array>
#include <string>
#include <vector>
#include <cstddef>

struct EQPreset {
    std::string name;
    std::array<float, 10> gains;
};

class EQConfigView {
public:
    static constexpr size_t NUM_BANDS = 10;
    static const float BAND_FREQUENCIES[NUM_BANDS];
    static const char* const BAND_LABELS[NUM_BANDS];

    EQConfigView();
    ~EQConfigView() = default;

    // 渲染 10 段发烧级图形均衡器中央调音台主面板
    void render(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max);

    // 状态与调音参数控制
    void setEnabled(bool enabled);
    bool isEnabled() const { return is_enabled_; }

    void setBandGain(size_t index, float gain_db);
    float getBandGain(size_t index) const;

    void applyPreset(size_t preset_index);
    int getCurrentPresetIndex() const { return current_preset_idx_; }

    void resetToFlat();

    // 供移动端触控手势判定滑块交互区域与激活状态 (防止误判为纵向列表滚动而释放鼠标按键)
    static bool isSliderTouch(float x, float y);
    static bool isAnySliderActive();
    static void setSliderActive(bool active);

private:
    bool is_enabled_ = true;
    int current_preset_idx_ = 0;
    std::array<float, NUM_BANDS> band_gains_{};

    std::vector<EQPreset> presets_;

    // 模块化子渲染组件
    void renderTopActions(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max);
    void renderPresetChips(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max);
    void renderCurveCanvas(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max);
    void renderSliders(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max);

    void syncToAudioEngine();
    void saveConfig();
    void loadConfig();
};
