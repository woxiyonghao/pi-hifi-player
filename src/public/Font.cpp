#include "Font.hpp"
#include "UIConfig.hpp"
#include "public/Platform.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

// 校验字体是否具备 stb_truetype 所需的矢量轮廓字表 (glyf 或 CFF)
// 苹果系统自带的部分中文字体 (如 PingFangUI.ttc) 使用了私有的 cidg/hvgl 压缩表，
// stb_truetype 无法解析；此函数确保只选定格式兼容的高清矢量中文字模。
static bool isFontSupportedByStbTrueType(const char* path) {
    if (!path || !path[0]) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    unsigned char header[12];
    if (fread(header, 1, 12, f) != 12) { fclose(f); return false; }

    uint32_t first_font_offset = 0;
    if (memcmp(header, "ttcf", 4) == 0) {
        unsigned char off_bytes[4];
        if (fread(off_bytes, 1, 4, f) != 4) { fclose(f); return false; }
        first_font_offset = (static_cast<uint32_t>(off_bytes[0]) << 24) |
                            (static_cast<uint32_t>(off_bytes[1]) << 16) |
                            (static_cast<uint32_t>(off_bytes[2]) << 8)  |
                            static_cast<uint32_t>(off_bytes[3]);
        if (fseek(f, first_font_offset, SEEK_SET) != 0) { fclose(f); return false; }
        if (fread(header, 1, 12, f) != 12) { fclose(f); return false; }
    }

    uint16_t num_tables = (static_cast<uint16_t>(header[4]) << 8) | header[5];
    bool has_glyf_or_cff = false;
    for (int i = 0; i < num_tables; i++) {
        unsigned char entry[16];
        if (fread(entry, 1, 16, f) != 16) break;
        if (memcmp(entry, "glyf", 4) == 0 || memcmp(entry, "CFF ", 4) == 0) {
            has_glyf_or_cff = true;
            break;
        }
    }
    fclose(f);
    return has_glyf_or_cff;
}

#if defined(__APPLE__)
#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>

// 探测 iOS / iPadOS App Bundle 资源包内的打包字体 (沙盒原生无阻碍直读)
static std::string queryAppleBundleFontPath(const char* resource_name, const char* ext) {
    CFBundleRef main_bundle = CFBundleGetMainBundle();
    if (!main_bundle) return "";
    CFStringRef cf_name = CFStringCreateWithCString(kCFAllocatorDefault, resource_name, kCFStringEncodingUTF8);
    CFStringRef cf_ext = ext ? CFStringCreateWithCString(kCFAllocatorDefault, ext, kCFStringEncodingUTF8) : nullptr;
    CFURLRef url = CFBundleCopyResourceURL(main_bundle, cf_name, cf_ext, nullptr);
    if (cf_name) CFRelease(cf_name);
    if (cf_ext) CFRelease(cf_ext);
    if (!url) return "";
    char path[1024] = {0};
    Boolean ok = CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, sizeof(path));
    CFRelease(url);
    if (ok && isFontSupportedByStbTrueType(path)) {
        return std::string(path);
    }
    return "";
}

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
    if (ok && isFontSupportedByStbTrueType(path)) {
        return std::string(path);
    }
    return "";
}
#endif

namespace Fonts {

void initialize(ImGuiIO& io) {
    // 跨平台字体路径备选池 (支持 iOS App Bundle、Apple CoreText 动态解析、Linux 路径与 Windows 路径)
    const std::vector<std::string> candidate_paths = {
#if defined(__APPLE__)
        queryAppleBundleFontPath("HiraginoSansGB", "ttc"),
        queryAppleBundleFontPath("HiraginoSansGB", "ttf"),
        queryAppleBundleFontPath("AppFont", "ttc"),
        queryAppleBundleFontPath("AppFont", "ttf"),
        queryAppleFontPath("HiraginoSansGB-W3"),
        queryAppleFontPath("STHeitiSC-Light"),
        "/System/Library/Fonts/Hiragino Sans GB.ttc",            // macOS 原生冬青黑体
        "/System/Library/Fonts/STHeiti Light.ttc",               // macOS 华文黑体
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
#endif
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",        // 树莓派文泉驿微米黑
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",// 树莓派 Noto Sans
        "C:\\Windows\\Fonts\\msyh.ttc",                          // Windows 微软雅黑
        "C:\\Windows\\Fonts\\simhei.ttf",                         // Windows 黑体
        "C:\\Windows\\Fonts\\simsun.ttc"                          // Windows 宋体
    };

    std::string matched_path;
    for (const auto& path : candidate_paths) {
        if (path.empty()) continue;
        if (isFontSupportedByStbTrueType(path.c_str())) {
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

    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();

    float sz_small   = UIConfig::FontSize::Small;
    float sz_regular = UIConfig::FontSize::Regular;
    float sz_medium  = UIConfig::FontSize::Medium;
    float sz_large   = UIConfig::FontSize::Large;

    // 当运行在 iPhone 设备架构时，全局字体等比例紧凑精致缩小，完美适配手机小屏，彻底杜绝所有字体溢出与偏大
    if (Platform::isIPhone()) {
        sz_small   = 9.5f;  // 12.0px -> 9.5px (精致微标/辅助文字/按钮)
        sz_regular = 11.5f; // 15.0px -> 11.5px (正文字体/选项正文)
        sz_medium  = 14.5f; // 20.0px -> 14.5px (页面与板块大标题)
        sz_large   = 18.0f; // 28.0px -> 18.0px (醒目标题)

        UIConfig::FontSize::Small   = sz_small;
        UIConfig::FontSize::Regular = sz_regular;
        UIConfig::FontSize::Medium  = sz_medium;
        UIConfig::FontSize::Large   = sz_large;
    }

    // 根据字阶配置，依序烘焙矢量字模
    Small   = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), sz_small,   &cfg, glyph_ranges);
    Regular = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), sz_regular, &cfg, glyph_ranges);
    Medium  = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), sz_medium,  &cfg, glyph_ranges);
    Large   = io.Fonts->AddFontFromFileTTF(matched_path.c_str(), sz_large,   &cfg, glyph_ranges);

    // 严密降级保护：若字体文件解析失败，强制挂载 ImGui 默认字库，严禁让未初始化的 GiantClock 夺取默认字体
    if (!Regular) {
        std::cout << "[Font] 警告: 加载中文字模失败，挂载内置字体作为安全托底。" << std::endl;
        Regular = io.Fonts->AddFontDefault();
    }
    io.FontDefault = Regular;

    // 专属烘焙 iOS StandBy 液态玻璃超大时钟字模 (仅 0-9 与冒号，极速烘焙，零纹理压力)
    // 优先选用粗圆体 (Arial Rounded Bold / Bold)，完美对齐 Apple iOS StandBy 胖圆粗字风格
    const std::vector<std::string> clock_font_candidates = {
#if defined(__APPLE__)
        queryAppleBundleFontPath("ArialRoundedBold", "ttf"),
        queryAppleBundleFontPath("ArialRoundedBold", "otf"),
        queryAppleFontPath("ArialRoundedMTBold"),
        queryAppleFontPath("HelveticaNeue-Bold"),
        "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf", // macOS 原生圆角特粗体
        "/System/Library/Fonts/SFNSDisplay-Bold.otf",
        "/System/Library/Fonts/Supplemental/Futura.ttc",
#endif
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "C:\\Windows\\Fonts\\segoeuib.ttf"
    };
    std::string clock_font_path;
    for (const auto& path : clock_font_candidates) {
        if (path.empty()) continue;
        if (isFontSupportedByStbTrueType(path.c_str())) {
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
    // 用户指定：字号再大 24px (原 145px + 24px = 169.0f)，iPhone 上适配横屏高度 (96.0f)
    float clock_sz = Platform::isIPhone() ? 96.0f : 169.0f;
    GiantClock = io.Fonts->AddFontFromFileTTF(clock_font_path.c_str(), clock_sz, &cfg, clock_ranges);

    // 设置默认全局字体
    io.FontDefault = Regular;
}

} // namespace Fonts