#include "themes/ThemeManager.hpp"
#include "public/UIConfig.hpp"
#include "tools/MusicDatabase.hpp"
#include <algorithm>
#include <iostream>

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    // 1. 初始化 4 大经典名机发烧视觉预设
    presets_.push_back({
        ThemeId::ModernCrimson,
        "现代深空玫红",
        "Modern Crimson",
        "Cupertino · 现代超高动态纯音极简美学，纯黑深空与标志性高保真玫红",
        "高解析流行 · 极简通透",
        IM_COL32(250, 45, 72, 255),  // #FA2D48 纯正玫红
        IM_COL32(250, 45, 72, 60),
        IM_COL32(250, 45, 72, 140),
        IM_COL32(255, 120, 140, 230),
        IM_COL32(255, 255, 255, 5),
        IM_COL32(18, 22, 28, 240),
        IM_COL32(10, 14, 20, 255),
        ImVec4(0.015f, 0.03f, 0.06f, 1.0f),
        MeterThemeType::ModernCrimson,
        BackgroundVisualMode::LEDSpectrum
    });

    presets_.push_back({
        ThemeId::McIntosh,
        "麦景图湖蓝",
        "McIntosh Blue Eyes",
        "Binghamton, New York · 75周年传世经典蓝眼睛双表头，发光透镜与深海纯黑",
        "大动态交响 · 磅礴大气",
        IM_COL32(0, 180, 240, 255),  // #00B4D8 湖蓝冰青
        IM_COL32(0, 180, 240, 60),
        IM_COL32(0, 180, 240, 150),
        IM_COL32(239, 68, 68, 240),  // 经典过载警戒红峰值顶
        IM_COL32(0, 40, 80, 25),
        IM_COL32(10, 18, 30, 240),
        IM_COL32(6, 12, 22, 255),
        ImVec4(0.012f, 0.025f, 0.05f, 1.0f),
        MeterThemeType::McIntosh,
        BackgroundVisualMode::VUMeter
    });

    presets_.push_back({
        ThemeId::Accuphase,
        "金嗓子香槟金",
        "Accuphase Champagne",
        "Yokohama, Japan · 日本发烧机皇精密香槟拉丝面板，E-260 液晶双表头温润生动",
        "甜美人声 · 温暖弦乐",
        IM_COL32(226, 199, 146, 255),  // #E2C792 原机香槟暖金
        IM_COL32(226, 199, 146, 60),
        IM_COL32(226, 199, 146, 150),
        IM_COL32(249, 115, 22, 240), // 暖橙金峰值顶
        IM_COL32(60, 45, 10, 25),
        IM_COL32(24, 20, 14, 240),
        IM_COL32(16, 13, 8, 255),
        ImVec4(0.035f, 0.028f, 0.015f, 1.0f),
        MeterThemeType::Accuphase,
        BackgroundVisualMode::Accuphase
    });

    presets_.push_back({
        ThemeId::RetroTape,
        "复古琥珀卡座",
        "Vintage Amber Tape",
        "Revox / Studer · 70年代模拟黄金年代开盘机，钨丝暖光与纯模拟温润磁带味",
        "模拟黑胶 · 温润模拟味",
        IM_COL32(249, 115, 22, 255),  // #F97316 模拟暖橙
        IM_COL32(249, 115, 22, 60),
        IM_COL32(249, 115, 22, 150),
        IM_COL32(239, 68, 68, 240),  // 磁带饱和红峰值顶
        IM_COL32(60, 25, 10, 25),
        IM_COL32(26, 18, 14, 240),
        IM_COL32(18, 11, 8, 255),
        ImVec4(0.035f, 0.02f, 0.01f, 1.0f),
        MeterThemeType::RetroTape,
        BackgroundVisualMode::VUMeter
    });
}

void ThemeManager::init() {
    // 1. 读取持久化的主题 ID
    std::string saved_theme_str = MusicDatabase::getInstance().getSetting("theme_id", "0");
    if (!saved_theme_str.empty()) {
        try {
            int tid = std::stoi(saved_theme_str);
            if (tid >= 0 && tid <= 4) {
                current_theme_ = static_cast<ThemeId>(tid);
            }
        } catch (...) {
            current_theme_ = ThemeId::ModernCrimson;
        }
    }

    // 2. 读取持久化的背景动效模式
    std::string saved_bg_str = MusicDatabase::getInstance().getSetting("bg_visual_mode", "0");
    if (!saved_bg_str.empty()) {
        try {
            int bg_val = std::stoi(saved_bg_str);
            if (bg_val >= 0 && bg_val <= 3) {
                bg_mode_ = static_cast<BackgroundVisualMode>(bg_val);
            }
        } catch (...) {
            bg_mode_ = BackgroundVisualMode::LEDSpectrum;
        }
    }

    // 3. 读取自定义颜色值
    std::string cr = MusicDatabase::getInstance().getSetting("theme_custom_r", "");
    std::string cg = MusicDatabase::getInstance().getSetting("theme_custom_g", "");
    std::string cb = MusicDatabase::getInstance().getSetting("theme_custom_b", "");
    if (!cr.empty() && !cg.empty() && !cb.empty()) {
        try {
            custom_color_.x = std::clamp(std::stof(cr), 0.0f, 1.0f);
            custom_color_.y = std::clamp(std::stof(cg), 0.0f, 1.0f);
            custom_color_.z = std::clamp(std::stof(cb), 0.0f, 1.0f);
            custom_color_.w = 1.0f;
        } catch (...) {}
    }

    // 应用配置到全局视觉系统
    applyCurrentTheme();
    std::cout << "[ThemeManager] 视觉主题中枢初始化就绪，当前主题: " << static_cast<int>(current_theme_)
              << "，背景模式: " << static_cast<int>(bg_mode_) << std::endl;
}

void ThemeManager::setTheme(ThemeId id, bool save) {
    current_theme_ = id;
    applyCurrentTheme();

    if (id != ThemeId::Custom) {
        setBackgroundVisualMode(getCurrentPreset().default_bg_mode, save);
    }

    if (save) {
        MusicDatabase::getInstance().setSetting("theme_id", std::to_string(static_cast<int>(id)));
    }
}

void ThemeManager::setBackgroundVisualMode(BackgroundVisualMode mode, bool save) {
    bg_mode_ = mode;
    if (save) {
        MusicDatabase::getInstance().setSetting("bg_visual_mode", std::to_string(static_cast<int>(mode)));
    }
}

void ThemeManager::setCustomColor(ImVec4 color, bool save) {
    custom_color_ = color;
    current_theme_ = ThemeId::Custom;
    applyCurrentTheme();

    if (save) {
        MusicDatabase::getInstance().setSetting("theme_id", std::to_string(static_cast<int>(ThemeId::Custom)));
        MusicDatabase::getInstance().setSetting("theme_custom_r", std::to_string(custom_color_.x));
        MusicDatabase::getInstance().setSetting("theme_custom_g", std::to_string(custom_color_.y));
        MusicDatabase::getInstance().setSetting("theme_custom_b", std::to_string(custom_color_.z));
    }
}

const ThemePresetInfo& ThemeManager::getCurrentPreset() const {
    for (const auto& preset : presets_) {
        if (preset.id == current_theme_) {
            return preset;
        }
    }
    return presets_[0];
}

void ThemeManager::applyCurrentTheme() {
    uint32_t r = 0, g = 0, b = 0;
    if (current_theme_ == ThemeId::Custom) {
        r = static_cast<uint32_t>(std::clamp(custom_color_.x * 255.0f, 0.0f, 255.0f));
        g = static_cast<uint32_t>(std::clamp(custom_color_.y * 255.0f, 0.0f, 255.0f));
        b = static_cast<uint32_t>(std::clamp(custom_color_.z * 255.0f, 0.0f, 255.0f));

        UIConfig::Color::Accent = IM_COL32(r, g, b, 255);
        UIConfig::Color::GlassActiveTint = IM_COL32(r, g, b, 60);
        UIConfig::Color::WindowBg = IM_COL32(16, 20, 26, 240);
        UIConfig::Color::MainStageBg = IM_COL32(10, 13, 18, 255);
    } else {
        const auto& preset = getCurrentPreset();
        UIConfig::Color::Accent = preset.accent_color;
        UIConfig::Color::GlassActiveTint = preset.glass_active_tint;
        UIConfig::Color::WindowBg = preset.window_bg;
        UIConfig::Color::MainStageBg = preset.main_stage_bg;
    }
}

ImU32 ThemeManager::getAccentColor() const {
    if (current_theme_ == ThemeId::Custom) {
        uint32_t r = static_cast<uint32_t>(std::clamp(custom_color_.x * 255.0f, 0.0f, 255.0f));
        uint32_t g = static_cast<uint32_t>(std::clamp(custom_color_.y * 255.0f, 0.0f, 255.0f));
        uint32_t b = static_cast<uint32_t>(std::clamp(custom_color_.z * 255.0f, 0.0f, 255.0f));
        return IM_COL32(r, g, b, 255);
    }
    return getCurrentPreset().accent_color;
}

ImU32 ThemeManager::getSpectrumLitColor() const {
    if (current_theme_ == ThemeId::Custom) {
        uint32_t r = static_cast<uint32_t>(std::clamp(custom_color_.x * 255.0f, 0.0f, 255.0f));
        uint32_t g = static_cast<uint32_t>(std::clamp(custom_color_.y * 255.0f, 0.0f, 255.0f));
        uint32_t b = static_cast<uint32_t>(std::clamp(custom_color_.z * 255.0f, 0.0f, 255.0f));
        return IM_COL32(r, g, b, 150);
    }
    return getCurrentPreset().lit_color;
}

ImU32 ThemeManager::getSpectrumPeakColor() const {
    if (current_theme_ == ThemeId::Custom) {
        uint32_t r = static_cast<uint32_t>(std::clamp(custom_color_.x * 255.0f, 0.0f, 255.0f));
        uint32_t g = static_cast<uint32_t>(std::clamp(custom_color_.y * 255.0f, 0.0f, 255.0f));
        uint32_t b = static_cast<uint32_t>(std::clamp(custom_color_.z * 255.0f, 0.0f, 255.0f));
        return IM_COL32(std::min(255u, r + 40u), std::min(255u, g + 40u), std::min(255u, b + 40u), 240);
    }
    return getCurrentPreset().peak_color;
}

ImU32 ThemeManager::getSpectrumUnlitColor() const {
    if (current_theme_ == ThemeId::Custom) {
        return IM_COL32(255, 255, 255, 6);
    }
    return getCurrentPreset().unlit_color;
}

ImVec4 ThemeManager::getClearColor() const {
    if (current_theme_ == ThemeId::Custom) {
        return ImVec4(0.02f, 0.025f, 0.035f, 1.0f);
    }
    return getCurrentPreset().clear_color;
}

MeterThemeType ThemeManager::getMeterTheme() const {
    if (current_theme_ == ThemeId::Custom) {
        return MeterThemeType::Custom;
    }
    return getCurrentPreset().meter_theme;
}
