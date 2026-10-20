#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace audio_engine {

/**
 * @brief DAC 解码芯片与硬件外设规格属性档案
 */
struct DacProfile {
    std::string brand;               // 品牌 (如 "TOPPING (拓品)", "Apple", "FiiO (飞傲)")
    std::string model;               // 型号 (如 "DX12 / DX1 II", "MacBook Pro 扬声器")
    std::string chip_model;          // 核心 DAC 芯片 (如 "ESS Sabre (ES9039Q2M)", "Apple CoreAudio Direct")
    std::string usb_interface;       // USB 界面/通道 (如 "XMOS XU316 异步低抖动时钟")
    uint32_t max_sample_rate = 384000; // 最高支持 PCM 采样率 (Hz)
    uint32_t max_bit_depth = 32;       // 最高位深 (Bit)
    std::string max_dsd_rate = "DSD256"; // 最高支持 DSD 规格 ("无", "DSD128", "DSD256", "DSD512")
    bool supports_dsd = true;
    bool supports_hog_mode = true;
    bool supports_uac_mode = true;      // 支持 UAC 2.0 / UAC 1.0 切换
    bool supports_dpll = true;          // 支持 ESS DPLL 抖动消除带宽调节
    bool supports_gain_control = true;  // 支持耳机放大增益切换 (0dB / +6dB)
    bool supports_line_out_bypass = true; // 支持 0dB 纯解码源码直出旁路
    std::vector<std::string> digital_filters;      // 芯片内置数字滤波选项
    std::vector<std::string> filter_descriptions;  // 听感说明
};

/**
 * @brief 检测到的物理/虚拟音频输出设备实体
 */
struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool is_builtin = false;
    bool is_default = false;
    std::string transport; // "BuiltIn", "USB", "HDMI", "Bluetooth", "Other"
    uint32_t current_sample_rate = 44100;
    uint32_t max_sample_rate = 384000;
    uint32_t max_bit_depth = 32;
    DacProfile profile;

    std::string toJson() const;
};

/**
 * @brief 热门发烧 DAC 芯片与知名外设特征知识库 (单例)
 * 涵盖：TOPPING DX12 (ES9039Q2M)、TOPPING DX3/DX7、FiiO 系列、Apple 直通、ALSA 直通与通用 USB DAC
 */
class DacDeviceCatalog {
public:
    static DacDeviceCatalog& getInstance();

    /**
     * @brief 根据设备名称、传输通道与探测到的参数智能匹配芯片画像
     */
    DacProfile matchProfile(const std::string& device_name, bool is_builtin, const std::string& transport, uint32_t detected_max_sr);

    /**
     * @brief 获取 TOPPING DX12 (ESS ES9039Q2M) 权威发烧配置画像
     */
    static DacProfile getToppingDx12Profile();

    /**
     * @brief 获取 Apple 直通原生声卡配置画像 (macOS 桌面)
     */
    static DacProfile getAppleBuiltinProfile(uint32_t max_sr = 96000);

    /**
     * @brief 获取 iPad / iOS AOP 原生音频架构配置画像
     */
    static DacProfile getIpadBuiltinProfile(uint32_t max_sr = 48000);

    /**
     * @brief 获取 Linux ALSA 直通声卡配置画像
     */
    static DacProfile getAlsaBuiltinProfile(uint32_t max_sr = 192000);

    /**
     * @brief 获取通用 USB Audio Class 2.0 高解析 DAC 画像
     */
    static DacProfile getGenericUsbProfile(const std::string& name, uint32_t max_sr);

private:
    DacDeviceCatalog() = default;
};

} // namespace audio_engine
