#include "audio_engine/DacDeviceCatalog.hpp"
#include <sstream>
#include <algorithm>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace audio_engine {

static std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\b') ss << "\\b";
        else if (c == '\f') ss << "\\f";
        else if (c == '\n') ss << "\\n";
        else if (c == '\r') ss << "\\r";
        else if (c == '\t') ss << "\\t";
        else ss << c;
    }
    return ss.str();
}

std::string AudioDeviceInfo::toJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"id\":\"" << escapeJsonString(id) << "\",";
    ss << "\"name\":\"" << escapeJsonString(name) << "\",";
    ss << "\"isBuiltIn\":" << (is_builtin ? "true" : "false") << ",";
    ss << "\"isDefault\":" << (is_default ? "true" : "false") << ",";
    ss << "\"transport\":\"" << escapeJsonString(transport) << "\",";
    ss << "\"currentSampleRate\":" << current_sample_rate << ",";
    ss << "\"maxSampleRate\":" << max_sample_rate << ",";
    ss << "\"maxBitDepth\":" << max_bit_depth << ",";

    // Profile 字段
    ss << "\"brand\":\"" << escapeJsonString(profile.brand) << "\",";
    ss << "\"model\":\"" << escapeJsonString(profile.model) << "\",";
    ss << "\"chipModel\":\"" << escapeJsonString(profile.chip_model) << "\",";
    ss << "\"usbInterface\":\"" << escapeJsonString(profile.usb_interface) << "\",";
    ss << "\"maxDsdRate\":\"" << escapeJsonString(profile.max_dsd_rate) << "\",";
    ss << "\"supportsDsd\":" << (profile.supports_dsd ? "true" : "false") << ",";
    ss << "\"supportsHogMode\":" << (profile.supports_hog_mode ? "true" : "false") << ",";
    ss << "\"supportsUacMode\":" << (profile.supports_uac_mode ? "true" : "false") << ",";
    ss << "\"supportsDpll\":" << (profile.supports_dpll ? "true" : "false") << ",";
    ss << "\"supportsGainControl\":" << (profile.supports_gain_control ? "true" : "false") << ",";
    ss << "\"supportsLineOutBypass\":" << (profile.supports_line_out_bypass ? "true" : "false") << ",";

    // 数字滤波列表
    ss << "\"digitalFilters\":[";
    for (size_t i = 0; i < profile.digital_filters.size(); ++i) {
        ss << "\"" << escapeJsonString(profile.digital_filters[i]) << "\"";
        if (i + 1 < profile.digital_filters.size()) ss << ",";
    }
    ss << "],";

    // 滤波听感描述
    ss << "\"filterDescriptions\":[";
    for (size_t i = 0; i < profile.filter_descriptions.size(); ++i) {
        ss << "\"" << escapeJsonString(profile.filter_descriptions[i]) << "\"";
        if (i + 1 < profile.filter_descriptions.size()) ss << ",";
    }
    ss << "]";

    ss << "}";
    return ss.str();
}

DacDeviceCatalog& DacDeviceCatalog::getInstance() {
    static DacDeviceCatalog instance;
    return instance;
}

DacProfile DacDeviceCatalog::getToppingDx12Profile() {
    DacProfile p;
    p.brand = "TOPPING (拓品)";
    p.model = "DX12 / DX1 II (桌面平衡解码耳放)";
    p.chip_model = "ESS Sabre (ES9039Q2M)";
    p.usb_interface = "XMOS XU316 超低抖动异步传输";
    p.max_sample_rate = 384000;
    p.max_bit_depth = 32;
    p.max_dsd_rate = "DSD256 (Native / DoP)";
    p.supports_dsd = true;
    p.supports_hog_mode = true;
    p.supports_uac_mode = true;
    p.supports_dpll = true;
    p.supports_gain_control = true;
    p.supports_line_out_bypass = true;

    // ESS ES9039Q2M 官方 8 种高精度数字滤波
    p.digital_filters = {
        "最小相位快速滚降 (Fast Roll-off, Minimum Phase)",
        "线性相位快速滚降 (Fast Roll-off, Linear Phase)",
        "最小相位慢速滚降 (Slow Roll-off, Minimum Phase)",
        "线性相位慢速滚降 (Slow Roll-off, Linear Phase)",
        "切趾快速滚降 (Apodizing Fast Roll-off)",
        "混合滤波 (Hybrid)",
        "砖墙滤波 (Brickwall)",
        "低色散滤波 (Low Dispersion)"
    };

    p.filter_descriptions = {
        "[默认推荐] 瞬态极佳，无预振铃，人声与现代流行音乐首选",
        "声场开阔自然，交响乐、大编制与古典弦乐首选",
        "温暖醇厚，消除高频数码毛刺，听感柔和耐听",
        "模拟黑胶唱片般的温润质感，音质厚实自然",
        "抑制瞬态冲激预振铃失真，极高保真母带重现",
        "兼顾空间结像深度与低频瞬态打击力度",
        "极陡峭阻带截止，高频细节与线条感极强",
        "全音频频带降低群延迟与相位失真"
    };

    return p;
}

DacProfile DacDeviceCatalog::getAppleBuiltinProfile(uint32_t max_sr) {
    DacProfile p;
    p.brand = "Apple Inc.";
    p.model = "Apple 内置音频输出";
    p.chip_model = "Apple Studio Grade CoreAudio Engine";
    p.usb_interface = "Apple Silicon Direct Bus";
    p.max_sample_rate = max_sr > 0 ? max_sr : 96000;
    p.max_bit_depth = 32;
    p.max_dsd_rate = "无 (自动无损转换为高解析 PCM)";
    p.supports_dsd = false;
    p.supports_hog_mode = true;
    p.supports_uac_mode = false;
    p.supports_dpll = false;
    p.supports_gain_control = false;
    p.supports_line_out_bypass = true;

    p.digital_filters = {
        "Apple 原生 64-bit 浮点线性滤波 (Bit-Perfect)",
        "系统自适应低延迟模式"
    };

    p.filter_descriptions = {
        "macOS 核心音频底层 64 位浮点超高保真抗混叠直通",
        "优化系统响应延迟，适合快速监听与多任务"
    };

    return p;
}

DacProfile DacDeviceCatalog::getIpadBuiltinProfile(uint32_t max_sr) {
    DacProfile p;
    p.brand = "Apple Inc.";
    p.model = "iPad 本机音频直通";
    p.chip_model = "Apple Studio Grade AOP (Audio Output Processor)";
    p.usb_interface = "Apple Silicon Direct AOP 零延迟总线";
    p.max_sample_rate = max_sr > 0 ? max_sr : 48000;
    p.max_bit_depth = 32;
    p.max_dsd_rate = "无 (自动无损转换为高解析 PCM)";
    p.supports_dsd = false;
    p.supports_hog_mode = false;
    p.supports_uac_mode = false;
    p.supports_dpll = false;
    p.supports_gain_control = false;
    p.supports_line_out_bypass = true;

    p.digital_filters = {
        "Apple AOP 原生 64-bit 浮点无损滤波",
        "低延迟硬件抗混叠模式"
    };

    p.filter_descriptions = {
        "iPadOS 硬件音频处理器原生 64 位浮点超低失真直出",
        "针对触控多媒体与发烧母带优化的极低延迟硬件滤波"
    };

    return p;
}

DacProfile DacDeviceCatalog::getAlsaBuiltinProfile(uint32_t max_sr) {
    DacProfile p;
    p.brand = "Raspberry Pi / Linux";
    p.model = "树莓派 ALSA 硬件直通";
    p.chip_model = "BCM2712 Direct / I2S Master Clock";
    p.usb_interface = "Broadcom I2S / DMA Direct";
    p.max_sample_rate = max_sr > 0 ? max_sr : 192000;
    p.max_bit_depth = 32;
    p.max_dsd_rate = "DoP (DSD over PCM)";
    p.supports_dsd = true;
    p.supports_hog_mode = true;
    p.supports_uac_mode = false;
    p.supports_dpll = false;
    p.supports_gain_control = false;
    p.supports_line_out_bypass = true;

    p.digital_filters = {
        "ALSA MMAP 零拷贝直通",
        "Linux 物理硬件双时钟锁定"
    };

    p.filter_descriptions = {
        "完全跳过 Linux PulseAudio / PipeWire 混音服务，物理级 0 劣化直出",
        "使用 44.1k 与 48k 双独立硬件晶振，杜绝分数倍频重采样失真"
    };

    return p;
}

DacProfile DacDeviceCatalog::getGenericUsbProfile(const std::string& name, uint32_t max_sr) {
    DacProfile p;
    p.brand = "High-Res USB Audio";
    p.model = name.empty() ? "外接 USB 解码器" : name;
    p.chip_model = "USB Audio Class 2.0 异步直通 DAC";
    p.usb_interface = "UAC 2.0 异步数字传输";
    p.max_sample_rate = max_sr > 0 ? max_sr : 384000;
    p.max_bit_depth = 32;
    p.max_dsd_rate = "DSD256 (DoP / Native)";
    p.supports_dsd = true;
    p.supports_hog_mode = true;
    p.supports_uac_mode = true;
    p.supports_dpll = false;
    p.supports_gain_control = false;
    p.supports_line_out_bypass = true;

    p.digital_filters = {
        "外接硬件芯片原生滤波 (Hardware Direct)",
        "最小相位平滑模式"
    };

    p.filter_descriptions = {
        "直接使用外接 USB DAC 内部硬件配置的默认超采样滤波",
        "软件层提供温和瞬态抗混叠"
    };

    return p;
}

static std::string toLower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return res;
}

DacProfile DacDeviceCatalog::matchProfile(const std::string& device_name, bool is_builtin, const std::string& transport, uint32_t detected_max_sr) {
    std::string lower = toLower(device_name);
    const auto bus = toLower(transport);
    const bool bluetooth = bus.find("bluetooth") != std::string::npos || lower.find("airpod") != std::string::npos;
    if (bluetooth || bus.find("hdmi") != std::string::npos || bus.find("displayport") != std::string::npos ||
        bus.find("airplay") != std::string::npos) {
        DacProfile p;
        p.brand = lower.find("airpod") != std::string::npos ? "Apple Inc." : "System Audio";
        p.model = device_name;
        p.chip_model = bluetooth ? "系统蓝牙音频编解码器" : "系统数字音频输出";
        p.usb_interface = bluetooth ? "Bluetooth（系统协商）" : transport;
        p.max_sample_rate = detected_max_sr > 0 ? detected_max_sr : 48000;
        p.max_dsd_rate = "无（自动转换为 PCM）";
        p.supports_dsd = p.supports_hog_mode = p.supports_uac_mode = false;
        p.supports_dpll = p.supports_gain_control = p.supports_line_out_bypass = false;
        return p;
    }

    if (is_builtin || transport == "BuiltIn" || transport == "BuiltIn (AOP)") {
#if defined(__APPLE__) && TARGET_OS_IPHONE
        return getIpadBuiltinProfile(detected_max_sr > 0 ? detected_max_sr : 48000);
#elif defined(__APPLE__)
        return getAppleBuiltinProfile(detected_max_sr > 0 ? detected_max_sr : 96000);
#else
        return getAlsaBuiltinProfile(detected_max_sr > 0 ? detected_max_sr : 192000);
#endif
    }

    // 拓品 TOPPING 匹配 (特别是 TOPPING DX12 / DX1 II / DX 系列)
    if (lower.find("topping") != std::string::npos ||
        lower.find("dx12") != std::string::npos ||
        lower.find("dx1") != std::string::npos ||
        lower.find("dx3") != std::string::npos ||
        lower.find("dx7") != std::string::npos) {
        DacProfile p = getToppingDx12Profile();
        if (detected_max_sr > 0) {
            p.max_sample_rate = detected_max_sr;
        }
        return p;
    }

    // 飞傲 FiiO 匹配
    if (lower.find("fiio") != std::string::npos || lower.find("k7") != std::string::npos || lower.find("k9") != std::string::npos) {
        DacProfile p;
        p.brand = "FiiO (飞傲)";
        p.model = device_name;
        p.chip_model = "AKM Velvet Sound (AK4493SEQ) / ESS Sabre";
        p.usb_interface = "XMOS 异步低抖动时钟";
        p.max_sample_rate = detected_max_sr > 0 ? detected_max_sr : 384000;
        p.max_bit_depth = 32;
        p.max_dsd_rate = "DSD256";
        p.supports_dsd = true;
        p.supports_hog_mode = true;
        p.supports_uac_mode = true;
        p.supports_dpll = false;
        p.supports_gain_control = true;
        p.supports_line_out_bypass = true;
        p.digital_filters = {
            "锐利滚降 (Sharp Roll-Off) - 干净解析",
            "缓慢滚降 (Slow Roll-Off) - 温暖温润",
            "短延时锐利滚降 (Short Delay Sharp)",
            "短延时缓慢滚降 (Short Delay Slow)",
            "超级缓慢滚降 (Super Slow Roll-Off)"
        };
        p.filter_descriptions = {
            "纯净通透，高频层次分明",
            "柔和温暖，人声乐感极佳",
            "瞬态反应敏捷，定位精准",
            "浓厚模拟听感，适合人声老歌",
            "平缓声学衰减，极宽泛音"
        };
        return p;
    }

    // 通用外接 USB / HDMI DAC
    return getGenericUsbProfile(device_name, detected_max_sr);
}

} // namespace audio_engine
