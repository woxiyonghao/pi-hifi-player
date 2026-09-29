#pragma once

#include <string>
#include <vector>

// ==============================================================================
// 硬件 DAC 芯片类型枚举与通用元数据模型 (DACProfile)
// 支持根据接入或选定的硬件芯片动态下发支持的硬件参数
// ==============================================================================
enum class DACType {
    ES9038PRO = 0, // ESS SABRE 旗舰双并联架构
    AK4499EX  = 1, // AKM 旗舰分离式 Velvet Sound 架构
    CS43198   = 2, // Cirrus Logic MasterHIFI 架构
    GENERIC_I2S = 3  // 标准通用 I2S / 树莓派声卡 / 平台内置声卡
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
            "数字/模拟物理芯片级分离 Velvet Sound Verita",
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
