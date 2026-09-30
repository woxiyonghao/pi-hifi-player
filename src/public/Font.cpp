#include "Font.hpp"
#include "UIConfig.hpp"
#include <iostream>
#include <vector>
#include <string>

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>

static std::string queryAppleFontPath(const char* font_name) {
    if (!font_name) return "";
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, font_name, kCFStringEncodingUTF8);
    if (!name) return "";
    CTFontRef font = CTFontCreateWithName(name, 16.0, nullptr);
    CFRelease(name);
    if (!font) return "";
    CFURLRef url = (CFURLRef)CTFontCopyAttribute(font, kCTFontURLAttribute);
    CFRelease(font);
    if (!url) return "";
    char path[1024] = {0};
    Boolean ok = CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, sizeof(path));
    CFRelease(url);
    if (ok) {
        return std::string(path);
    }
    return "";
}
#endif

namespace Fonts {

void initialize(ImGuiIO& io) {
    // 跨平台字体路径备选池 (支持 Apple CoreText 动态寻址与 Linux 路径)
    const std::vector<std::string> candidate_paths = {
#if defined(__APPLE__)
        queryAppleFontPath("PingFangSC-Regular"),
        queryAppleFontPath("HiraginoSansGB-W3"),
        queryAppleFontPath("STHeitiSC-Light"),
#endif
        "/System/Library/Fonts/LanguageSupport/PingFang.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",            // macOS 原生冬青黑体
        "/System/Library/Fonts/STHeiti Light.ttc",               // macOS 华文黑体
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",        // 树莓派文泉驿微米黑
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" // 树莓派 Noto Sans
    };

    std::string matched_path;
    for (const auto& path : candidate_paths) {
        if (path.empty()) continue;
        if (FILE* f = fopen(path.c_str(), "r")) {
            fclose(f);
            matched_path = path;
            break;
        }
    }

    if (matched_path.empty()) {
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
    Small   = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), UIConfig::FontSize::Small,   &cfg, glyph_ranges);
    Regular = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), UIConfig::FontSize::Regular, &cfg, glyph_ranges);
    Medium  = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), UIConfig::FontSize::Medium,  &cfg, glyph_ranges);
    Large   = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), UIConfig::FontSize::Large,   &cfg, glyph_ranges);

    // 专属烘焙 iOS StandBy 液态玻璃超大时钟字模 (仅 0-9 与冒号，极速烘焙，零纹理压力)
    // 优先选用粗圆体 (Arial Rounded Bold / Bold)，完美对齐 Apple iOS StandBy 胖圆粗字风格
    const std::vector<std::string> clock_font_candidates = {
#if defined(__APPLE__)
        queryAppleFontPath("ArialRoundedMTBold"),
        queryAppleFontPath("HelveticaNeue-Bold"),
#endif
        "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf", // macOS 原生圆角特粗体
        "/System/Library/Fonts/SFNSDisplay-Bold.otf",
        "/System/Library/Fonts/Supplemental/Futura.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf"
    };
    std::string clock_font_path;
    for (const auto& path : clock_font_candidates) {
        if (path.empty()) continue;
        if (FILE* f = fopen(path.c_str(), "r")) {
            fclose(f);
            clock_font_path = path;
            break;
        }
    }
    if (clock_font_path.empty()) clock_font_path = matched_path;

    static const ImWchar clock_ranges[] = {
        '0', '9',
        ':', ':',
        0
    };
    // 用户指定：字号再大 24px (原 145px + 24px = 169.0f)
    GiantClock = io.Fonts->AddFontFromFileTTF(clock_font_path.c_str(), 169.0f, &cfg, clock_ranges);

    // 设置默认全局字体
    io.FontDefault = Regular;
}

} // namespace Fonts