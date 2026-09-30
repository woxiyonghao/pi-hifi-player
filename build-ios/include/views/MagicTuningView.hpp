#pragma once

#include "imgui.h"
#include <array>
#include <string>

// ==============================================================================
// 调音魔棒 · 主观听感导向高阶音效微调引擎 (MagicTuningView)
// 借鉴海贝 MSEB (Mage Sound Eight-Ball) 心理声学调音算法
// 将深奥的 PEQ 物理频率参数转化为发烧友直观易懂的主观听感滑块
// ==============================================================================
class MagicTuningView {
public:
    static constexpr size_t NUM_PARAMS = 10;

    struct ParamDef {
        const char* name_zh;
        const char* name_en;
        const char* description;
        const char* left_label;
        const char* right_label;
    };

    static const std::array<ParamDef, NUM_PARAMS>& getParamDefs();

    MagicTuningView();
    ~MagicTuningView() = default;

    void render(float x, float y, float w, float h);

    void setEnabled(bool enabled);
    bool isEnabled() const { return is_enabled_; }

    void setParamValue(size_t index, float val);
    float getParamValue(size_t index) const;

    void resetAll();
    void resetParam(size_t index);

private:
    void loadConfig();
    void saveConfig();
    void syncToAudioEngine();

    // 单组调音卡片渲染 (严格符合 --header -----slider 一个调音一组结构)
    float renderTuningGroupCard(ImDrawList* dl, size_t idx, float x, float y, float w);

    // 类似 EQConfigView 风格的自定义高精度左右水平滑动轨与胶囊旋钮
    bool renderHorizontalSlider(ImDrawList* dl, ImVec2 track_min, ImVec2 track_max,
                                float& val, float min_val, float max_val, const char* str_id);

private:
    bool is_enabled_ = true;
    std::array<float, NUM_PARAMS> params_ = {0.0f};
};
