#include "Font.hpp"
#include "UIConfig.hpp"
#include <iostream>
#include <vector>

namespace Fonts {

void initialize(ImGuiIO& io) {
    // 跨平台字体路径备选池
    const std::vector<const char*> candidate_paths = {
        "/System/Library/Fonts/Hiragino Sans GB.ttc",            // macOS 原生冬青黑体
        "/System/Library/Fonts/STHeiti Light.ttc",               // macOS 华文黑体
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",        // 树莓派文泉驿微米黑
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" // 树莓派 Noto Sans
    };

    const char* matched_path = nullptr;
    for (const auto& path : candidate_paths) {
        if (FILE* f = fopen(path, "r")) {
            fclose(f);
            matched_path = path;
            break;
        }
    }

    if (!matched_path) {
        std::cout << "[Font] 未检测到系统 CJK 字体，使用内置默认英文字体。" << std::endl;
        return;
    }

    std::cout << "[Font] 成功定位中文字体: " << matched_path << std::endl;

    // 配置抗锯齿采样
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesChineseFull();

    // 根据 UIConfig::FontSize 中的字号配置，依序烘焙不同字阶的矢量字模
    Small   = io.Fonts->AddFontFromFileTTF(matched_path, UIConfig::FontSize::Small,   &cfg, glyph_ranges);
    Regular = io.Fonts->AddFontFromFileTTF(matched_path, UIConfig::FontSize::Regular, &cfg, glyph_ranges);
    Medium  = io.Fonts->AddFontFromFileTTF(matched_path, UIConfig::FontSize::Medium,  &cfg, glyph_ranges);
    Large   = io.Fonts->AddFontFromFileTTF(matched_path, UIConfig::FontSize::Large,   &cfg, glyph_ranges);

    // 设置默认全局字体
    io.FontDefault = Regular;
}

} // namespace Fonts