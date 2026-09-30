#pragma once

#include <string>
#include <vector>

// ==============================================================================
// 硬件 DAC 芯片类型枚举与通用元数据模型 (DACProfile)
// 支持根据接入或选定的硬件芯片动态下发支持的硬件参数
// ==============================================================================
enum class DACType {
    APPLE_DIRECT = 0, // Apple Direct (MacBook Pro M1 Pro 硬件直通)
    ES9038PRO    = 1, // ESS SABRE 旗舰双并联架构
    AK4499EX     = 2, // AKM 旗舰分离式 Velvet Sound 架构
    CS43198      = 3, // Cirrus Logic MasterHIFI 架构
    R2R_DISCRETE = 4, // R-2R 纯分立电阻网络 (NOS/OS)
    ROHM_BD34301 = 5, // ROHM 罗姆 MUS-IC 旗舰架构
    GENERIC_I2S  = 6  // 标准通用 I2S / 平台声卡
};

struct DACProfileInfo {
    DACType type;
    std::string model_name;
    std::string short_name;
    std::string manufacturer;
    std::string architecture;
    std::string max_pcm;
    std::string max_dsd;
};

inline const std::vector<DACProfileInfo>& getSupportedDACProfiles() {
    static const std::vector<DACProfileInfo> kProfiles = {
        {
            DACType::APPLE_DIRECT,
            "Apple Direct",
            "Apple Direct",
            "Apple Inc. (CoreAudio)",
            "MacBook Pro M1 Pro 硬件直通 · Bit-Perfect 独占流",
            "192kHz / 32-bit Float",
            "DoP 硬件直通"
        },
        {
            DACType::ES9038PRO,
            "Dual ES9038PRO",
            "ES9038PRO",
            "ESS Technology (SABRE)",
            "双芯片 8-Ch 单声道全平衡并联 (Dual Mono)",
            "768kHz / 32-bit",
            "Native DSD512 / 1024"
        },
        {
            DACType::AK4499EX,
            "AKM AK4499EX + AK4191",
            "AK4499EX",
            "Asahi Kasei Microdevices (AKM)",
            "数模芯片级物理分离 Velvet Sound Verita",
            "1536kHz / 32-bit",
            "Native DSD512 / 1024"
        },
        {
            DACType::CS43198,
            "Cirrus Dual CS43198",
            "CS43198",
            "Cirrus Logic (MasterHIFI)",
            "双芯片低功耗超高保真 + NOS 无过采样",
            "384kHz / 32-bit",
            "Native DSD256"
        },
        {
            DACType::R2R_DISCRETE,
            "R-2R Ladder DAC",
            "R-2R Discrete",
            "Discrete Precision Resistor Matrix",
            "全分立 0.01% 精密电阻阵列 · NOS / OS 纯模拟声",
            "1536kHz / 24-bit",
            "DSD Direct 纯梯形网络"
        },
        {
            DACType::ROHM_BD34301,
            "ROHM BD34301EKV",
            "BD34301EKV",
            "ROHM Semiconductor (MUS-IC)",
            "旗舰 32-bit 发烧音响级 DAC · 浓郁音乐感与微动态",
            "768kHz / 32-bit",
            "Direct Path 纯模拟通道"
        },
        {
            DACType::GENERIC_I2S,
            "通用 I2S / 平台声卡",
            "Generic DAC",
            "Standard Audio Architecture",
            "硬件直通标准 I2S / USB Audio Class 2.0",
            "自适应原生采样率",
            "DoP / 软解直出"
        }
    };
    return kProfiles;
}

inline const DACProfileInfo& getDACProfileInfo(DACType type) {
    const auto& list = getSupportedDACProfiles();
    for (const auto& item : list) {
        if (item.type == type) return item;
    }
    return list[0];
}
