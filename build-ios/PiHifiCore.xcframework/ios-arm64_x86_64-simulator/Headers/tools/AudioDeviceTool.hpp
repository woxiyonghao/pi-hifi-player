#pragma once

#include <string>
#include <vector>

// 物理音频输出硬件设备信息
struct PhysicalAudioDevice {
    int card_num = -1;
    std::string id;
    std::string name;          // 简短设备名 (如 "USB Audio Device")
    std::string long_name;     // 详细硬件描述 (如 "GeneralPlus USB Audio Device at usb-xhci-hcd.0-2")
    std::string driver;        // 驱动标识 (如 "USB-Audio", "vc4-hdmi")
    bool is_external_dac = false;
    bool is_active = false;
};

// 系统整体音频硬件与 DAC 状态
struct AudioHardwareStatus {
    bool has_external_dac = false;
    std::string dac_name;        // 外接 DAC 芯片/设备名 (如 "USB Audio Device" 或 "未连接")
    std::string dac_full_desc;   // 完整硬件信息
    std::string active_output_name; // 当前活跃音频输出设备
    std::vector<PhysicalAudioDevice> devices;
};

// ==============================================================================
// 真实物理音频硬件与 DAC 检测工具 (AudioDeviceTool)
// 绝不伪造虚假数据，直接读取 ALSA (/proc/asound/cards) 与 CoreAudio 底层物理声卡
// ==============================================================================
class AudioDeviceTool {
public:
    static AudioHardwareStatus getHardwareStatus();
};
