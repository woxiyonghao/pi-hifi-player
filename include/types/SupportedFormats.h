#pragma once

#include "types/MusicModel.hpp"
#include <array>
#include <optional>
#include <string_view>

// ==============================================================================
// 1. 发烧音频格式元数据描述体
// ==============================================================================
struct FormatDescriptor {
    std::string_view extension; // 标准小写扩展名，如 ".flac"
    AudioFormat format;
    std::string_view mime_type; // 如 "audio/flac"
    bool is_lossless;           // 是否为无损/发烧母带
    bool is_dsd;                // 是否为 1-bit DSD (SACD 规格)
};

// ==============================================================================
// 2. 编译期支持格式静态表 (C++20 std::array + std::string_view 零运行时开销)
// ==============================================================================
inline constexpr std::array SUPPORTED_FORMATS = {
    FormatDescriptor{".flac", AudioFormat::FLAC, "audio/flac", true, false},
    FormatDescriptor{".wav", AudioFormat::WAV, "audio/wav", true, false},
    FormatDescriptor{".dsf", AudioFormat::DSD_DSF, "audio/x-dsf", true, true},
    FormatDescriptor{".dff", AudioFormat::DSD_DFF, "audio/x-dff", true, true},
    FormatDescriptor{".alac", AudioFormat::ALAC, "audio/alac", true, false},
    FormatDescriptor{".mp3", AudioFormat::MP3, "audio/mpeg", false, false}};

namespace AudioFormatUtils {

// ==============================================================================
// 3. 辅助函数：ASCII 大小写不敏感字符/字符串比较 (完全 constexpr & noexcept)
// ==============================================================================
constexpr bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + ('a' - 'A'));
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

// 统一后缀比对：同时兼容带点（如 ".FLAC"）与不带点（如 "flac"）的输入
constexpr bool matchExtension(std::string_view ext, std::string_view standard_ext) noexcept {
    if (!ext.empty() && ext.front() == '.') {
        return iequals(ext, standard_ext);
    }
    // 若入参没有点，但标准扩展名有前导点（如 ".flac"），比对去掉点之后的后缀
    if (!standard_ext.empty() && standard_ext.front() == '.') {
        return iequals(ext, standard_ext.substr(1));
    }
    return iequals(ext, standard_ext);
}

// ==============================================================================
// 4. 对外核心查询工具接口 (大小写不敏感)
// ==============================================================================
// 检查某个扩展名（如 ".FLAC"、".flac"、"dsf"）是否受支持
constexpr bool isSupported(std::string_view ext) noexcept {
    for (const auto& fmt : SUPPORTED_FORMATS) {
        if (matchExtension(ext, fmt.extension)) {
            return true;
        }
    }
    return false;
}

// 查询对应的 AudioFormat 枚举（未找到返回 std::nullopt）
constexpr std::optional<AudioFormat> getAudioFormat(std::string_view ext) noexcept {
    for (const auto& fmt : SUPPORTED_FORMATS) {
        if (matchExtension(ext, fmt.extension)) {
            return fmt.format;
        }
    }
    return std::nullopt;
}

// 查询格式完整元数据描述体
constexpr std::optional<FormatDescriptor> findDescriptor(std::string_view ext) noexcept {
    for (const auto& fmt : SUPPORTED_FORMATS) {
        if (matchExtension(ext, fmt.extension)) {
            return fmt;
        }
    }
    return std::nullopt;
}

} // namespace AudioFormatUtils

// ==============================================================================
// 5. C++20 编译期静态断言自测 (确保大小写不敏感与后缀处理 100% 编译期无误)
// ==============================================================================
static_assert(AudioFormatUtils::isSupported(".FLAC"));
static_assert(AudioFormatUtils::isSupported(".flac"));
static_assert(AudioFormatUtils::isSupported("flac"));
static_assert(AudioFormatUtils::isSupported(".Dsf"));
static_assert(AudioFormatUtils::isSupported("DFF"));
static_assert(!AudioFormatUtils::isSupported(".txt"));
static_assert(!AudioFormatUtils::isSupported(".exe"));
static_assert(AudioFormatUtils::getAudioFormat(".WAV") == AudioFormat::WAV);
static_assert(AudioFormatUtils::getAudioFormat("mp3") == AudioFormat::MP3);