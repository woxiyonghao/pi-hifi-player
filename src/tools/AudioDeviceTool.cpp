#include "tools/AudioDeviceTool.hpp"
#include "public/Platform.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
#endif
#endif

namespace {

#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
AudioHardwareStatus detectLinuxAlsaCards() {
    AudioHardwareStatus status;
    std::ifstream file("/proc/asound/cards");
    if (!file.is_open()) {
        status.has_external_dac = false;
        status.dac_name = "未连接";
        status.dac_full_desc = "未能读取到系统 ALSA 声卡节点";
        status.active_output_name = "默认音频输出";
        return status;
    }

    std::string line;
    PhysicalAudioDevice current_dev;
    bool reading_card = false;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t first_non_space = line.find_first_not_of(" \t");
        if (first_non_space != std::string::npos && std::isdigit(line[first_non_space])) {
            if (reading_card) {
                status.devices.push_back(current_dev);
                current_dev = PhysicalAudioDevice();
            }

            reading_card = true;
            std::istringstream iss(line.substr(first_non_space));
            iss >> current_dev.card_num;

            size_t b1 = line.find('[');
            size_t b2 = line.find(']');
            if (b1 != std::string::npos && b2 != std::string::npos && b2 > b1) {
                current_dev.id = line.substr(b1 + 1, b2 - b1 - 1);
                size_t p1 = current_dev.id.find_first_not_of(" \t");
                size_t p2 = current_dev.id.find_last_not_of(" \t");
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    current_dev.id = current_dev.id.substr(p1, p2 - p1 + 1);
                }
            }

            size_t colon_pos = line.find(':', b2);
            if (colon_pos != std::string::npos) {
                std::string rest = line.substr(colon_pos + 1);
                size_t dash_pos = rest.find('-');
                if (dash_pos != std::string::npos) {
                    current_dev.driver = rest.substr(0, dash_pos);
                    current_dev.name = rest.substr(dash_pos + 1);
                } else {
                    current_dev.name = rest;
                }

                auto trim = [](std::string& s) {
                    size_t p1 = s.find_first_not_of(" \t");
                    size_t p2 = s.find_last_not_of(" \t\r\n");
                    if (p1 != std::string::npos && p2 != std::string::npos) {
                        s = s.substr(p1, p2 - p1 + 1);
                    } else {
                        s.clear();
                    }
                };
                trim(current_dev.driver);
                trim(current_dev.name);

                // 优化过长的 "Audio - USB Audio Device"
                if (current_dev.name.rfind("Audio - ", 0) == 0) {
                    current_dev.name = current_dev.name.substr(8);
                }
            }
        } else if (reading_card) {
            size_t p1 = line.find_first_not_of(" \t");
            if (p1 != std::string::npos) {
                current_dev.long_name = line.substr(p1);
                while (!current_dev.long_name.empty() && 
                       (current_dev.long_name.back() == '\r' || current_dev.long_name.back() == '\n')) {
                    current_dev.long_name.pop_back();
                }
            }
        }
    }

    if (reading_card) {
        status.devices.push_back(current_dev);
    }

    // 判断各个声卡是否属于外置独立 DAC / USB / I2S 声卡 (排除树莓派自带的 HDMI 0/1)
    for (auto& dev : status.devices) {
        std::string lower = dev.name + " " + dev.driver + " " + dev.long_name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

        bool is_hdmi = (lower.find("hdmi") != std::string::npos || lower.find("vc4") != std::string::npos);
        bool is_dummy = (lower.find("dummy") != std::string::npos);

        if (!is_hdmi && !is_dummy) {
            dev.is_external_dac = true;
        }
    }

    // 优先选中真实的外置 DAC
    for (auto& dev : status.devices) {
        if (dev.is_external_dac) {
            dev.is_active = true;
            status.has_external_dac = true;
            status.dac_name = dev.name.empty() ? dev.id : dev.name;
            status.dac_full_desc = dev.long_name.empty() ? dev.name : dev.long_name;
            status.active_output_name = status.dac_name;
            break;
        }
    }

    // 若未检测到任何外接独立 DAC，绝不伪造
    if (!status.has_external_dac) {
        status.dac_name = "未连接";
        status.dac_full_desc = "未检测到外接独立 DAC (当前仅为 HDMI 基础音频)";
        if (!status.devices.empty()) {
            status.devices[0].is_active = true;
            status.active_output_name = status.devices[0].name;
        } else {
            status.active_output_name = "无可用硬件输出";
        }
    }

    return status;
}
#endif

#if defined(__APPLE__) && TARGET_OS_OSX
AudioHardwareStatus detectMacCoreAudioDevices() {
    AudioHardwareStatus status;
    AudioDeviceID default_dev = kAudioObjectUnknown;
    UInt32 size = sizeof(default_dev);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &default_dev);

    // 枚举系统中所有音频设备
    UInt32 devices_size = 0;
    AudioObjectPropertyAddress dev_list_addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &dev_list_addr, 0, nullptr, &devices_size) == noErr && devices_size > 0) {
        int dev_count = static_cast<int>(devices_size / sizeof(AudioDeviceID));
        std::vector<AudioDeviceID> device_ids(dev_count);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &dev_list_addr, 0, nullptr, &devices_size, device_ids.data()) == noErr) {
            int card_idx = 0;
            for (int i = 0; i < dev_count; ++i) {
                AudioDeviceID dev_id = device_ids[i];

                // 仅检测包含输出通道 (Output Streams) 的设备
                AudioObjectPropertyAddress stream_addr = {
                    kAudioDevicePropertyStreams,
                    kAudioDevicePropertyScopeOutput,
                    kAudioObjectPropertyElementMain
                };
                UInt32 stream_size = 0;
                if (AudioObjectGetPropertyDataSize(dev_id, &stream_addr, 0, nullptr, &stream_size) != noErr || stream_size == 0) {
                    continue; // 无输出通道，跳过纯输入设备
                }

                // 获取名称
                CFStringRef cfName = nullptr;
                UInt32 name_size = sizeof(cfName);
                AudioObjectPropertyAddress name_addr = {
                    kAudioObjectPropertyName,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                std::string dev_name = "音频设备";
                if (AudioObjectGetPropertyData(dev_id, &name_addr, 0, nullptr, &name_size, &cfName) == noErr && cfName) {
                    char buf[256] = {0};
                    CFStringGetCString(cfName, buf, sizeof(buf), kCFStringEncodingUTF8);
                    CFRelease(cfName);
                    if (buf[0] != '\0') dev_name = buf;
                }

                // 获取传输类型 (USB / BuiltIn / Bluetooth)
                UInt32 transport = 0;
                UInt32 t_size = sizeof(transport);
                AudioObjectPropertyAddress t_addr = {
                    kAudioDevicePropertyTransportType,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                bool is_usb = false;
                if (AudioObjectGetPropertyData(dev_id, &t_addr, 0, nullptr, &t_size, &transport) == noErr) {
                    is_usb = (transport == kAudioDeviceTransportTypeUSB);
                }

                PhysicalAudioDevice dev;
                dev.card_num = card_idx++;
                dev.name = dev_name;
                dev.long_name = dev_name + (is_usb ? " (USB 独立解码设备)" : " (Mac 内置音频架构)");
                dev.driver = is_usb ? "USB-Audio" : "AppleHDA";
                dev.is_external_dac = is_usb;
                dev.is_active = (dev_id == default_dev);
                status.devices.push_back(dev);

                if (dev.is_active) {
                    status.active_output_name = dev_name;
                    if (is_usb) {
                        status.has_external_dac = true;
                        status.dac_name = dev_name;
                        status.dac_full_desc = dev_name + " (CoreAudio 硬件直通)";
                    } else {
                        status.has_external_dac = false;
                        status.dac_name = "Apple Direct";
                        status.dac_full_desc = "当前使用 " + dev_name + " (Mac 原生直通)";
                    }
                }
            }
        }
    }

    if (status.devices.empty()) {
        status.has_external_dac = false;
        status.dac_name = "Apple Direct";
        status.dac_full_desc = "Mac 原生音频输出";
        status.active_output_name = "默认音频输出";
    }

    return status;
}
#endif

} // namespace

AudioHardwareStatus AudioDeviceTool::getHardwareStatus() {
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    return detectLinuxAlsaCards();
#elif defined(__APPLE__) && TARGET_OS_OSX
    return detectMacCoreAudioDevices();
#else
    AudioHardwareStatus status;
    status.has_external_dac = false;
    status.dac_name = "未连接";
    status.dac_full_desc = "当前平台不支持物理硬件探测";
    status.active_output_name = "系统默认音频";
    return status;
#endif
}
