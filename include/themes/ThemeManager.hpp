#pragma once

#include "imgui.h"
#include "themes/VUMeterRenderer.hpp"
#include <string>
#include <vector>

// ==============================================================================
// 经典发烧主题枚举 (向名机皇致敬)
// ==============================================================================
enum class ThemeId : int {
    ModernCrimson = 0, // 现代深空玫红 (Apple Music 纯净基准)
    McIntosh = 1,      // 麦景图深蓝 (McIntosh Blue Eyes)
    Accuphase = 2,     // 金嗓子香槟金 (Accuphase Champagne Gold)
    RetroTape = 3,     // 复古琥珀卡座 (Vintage Amber Tape)
    Custom = 4         // 发烧友自由调色
};

// ==============================================================================
// 全屏全景底层背景律动视觉模式
// ==============================================================================
enum class BackgroundVisualMode : int {
    LEDSpectrum = 0, // 48 列全屏分段 LED 律动矩阵 (全屏贯通)
    VUMeter = 1,     // 发烧双通道机械动圈大表头 (全景对称)
    Accuphase = 2,   // 金嗓子动圈大表头 (Accuphase Precision Power Meter)
    PureBlack = 3    // 极简纯净发烧机架 (0 干扰纯音直通)
};

// ==============================================================================
// 预设主题发烧规格参数结构体
// ==============================================================================
struct ThemePresetInfo {
    ThemeId id;
    std::string name;          // 中文主题名: 如 "麦景图湖蓝"
    std::string en_name;       // 英文名称: 如 "McIntosh Blue Eyes"
    std::string brand_desc;    // 品牌机皇渊源描述
    std::string sound_style;   // 声学风格标签
    ImU32 accent_color;        // UI 核心强调色
    ImU32 glass_active_tint;   // 激活液态玻璃微光润色底
    ImU32 lit_color;           // 48 列全屏全景 LED 频谱点亮色
    ImU32 peak_color;          // 频谱顶峰高亮警示色
    ImU32 unlit_color;         // 频谱暗格微光底色
    ImU32 window_bg;           // 侧边栏发烧底色
    ImU32 main_stage_bg;       // 主舞台基底色
    ImVec4 clear_color;        // OpenGL 清屏底色
    MeterThemeType meter_theme;// 对应动圈表头主题
    BackgroundVisualMode default_bg_mode; // 预设关联默认背景模式
};

// ==============================================================================
// 全局发烧视觉主题管理中枢 (ThemeManager - 单例)
// 管控全局调色板、LED 频谱着色、VU 表头风格与 SQLite 持久化
// ==============================================================================
class ThemeManager {
public:
    static ThemeManager& getInstance();

    // 初始化：从 SQLite 数据库加载已持久化的主题与自定义颜色
    void init();

    ThemeId getCurrentTheme() const { return current_theme_; }
    void setTheme(ThemeId id, bool save = true);

    BackgroundVisualMode getBackgroundVisualMode() const { return bg_mode_; }
    void setBackgroundVisualMode(BackgroundVisualMode mode, bool save = true);

    const ThemePresetInfo& getCurrentPreset() const;
    const std::vector<ThemePresetInfo>& getAllPresets() const { return presets_; }

    // 自定义颜色支持 (RGB 0.0f ~ 1.0f)
    ImVec4 getCustomColor() const { return custom_color_; }
    void setCustomColor(ImVec4 color, bool save = true);

    // 快捷查询接口 (供 UI、全景背景、动圈表头调用)
    ImU32 getAccentColor() const;
    ImU32 getSpectrumLitColor() const;
    ImU32 getSpectrumPeakColor() const;
    ImU32 getSpectrumUnlitColor() const;
    ImVec4 getClearColor() const;
    MeterThemeType getMeterTheme() const;

private:
    ThemeManager();
    ~ThemeManager() = default;

    void applyCurrentTheme();

    ThemeId current_theme_ = ThemeId::ModernCrimson;
    BackgroundVisualMode bg_mode_ = BackgroundVisualMode::LEDSpectrum;
    ImVec4 custom_color_ = ImVec4(0.98f, 0.18f, 0.28f, 1.0f);
    std::vector<ThemePresetInfo> presets_;
};
