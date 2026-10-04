#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>

// ==============================================================================
// 全局跨平台硬件与操作系统架构定义 (Single Source of Truth)
// 支持精准判定: macOS, iOS/iPhone, Android, 树莓派 (Raspberry Pi), Windows
// 核心调度规范: 除了树莓派使用 30fps (控温降载)，其余平台均锁定 60fps 原生刷新
// ==============================================================================
enum class PlatformType {
    MacOS,
    iOS,
    Android,
    RaspberryPi,
    Windows,
    Unknown
};

class Platform {
public:
    // 获取当前运行平台类型
    static PlatformType current() {
#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        return PlatformType::iOS;
    #else
        return PlatformType::MacOS;
    #endif
#elif defined(__ANDROID__)
        return PlatformType::Android;
#elif defined(HIFI_PLATFORM_RPI)
        return PlatformType::RaspberryPi;
#elif defined(_WIN32) || defined(_WIN64)
        return PlatformType::Windows;
#elif defined(__linux__)
        if (isRaspberryPi()) {
            return PlatformType::RaspberryPi;
        }
        return PlatformType::Unknown;
#else
        return PlatformType::Unknown;
#endif
    }

    // 获取当前平台标准字符串标识
    static std::string name() {
        switch (current()) {
            case PlatformType::MacOS:       return "mac";
            case PlatformType::iOS:         return "ios";
            case PlatformType::Android:     return "android";
            case PlatformType::RaspberryPi: return "raspberry_pi";
            case PlatformType::Windows:     return "windows";
            default:                        return "unknown";
        }
    }

    // 获取友好显示名称
    static std::string displayName() {
        switch (current()) {
            case PlatformType::MacOS:       return "macOS Desktop";
            case PlatformType::iOS:         return "Apple iOS / iPhone";
            case PlatformType::Android:     return "Android Mobile";
            case PlatformType::RaspberryPi: return "Raspberry Pi 5 (HiFi Engine)";
            case PlatformType::Windows:     return "Windows Desktop";
            default:                        return "Generic Platform";
        }
    }

    // 快捷平台判断方法
    static bool isMacOS()       { return current() == PlatformType::MacOS; }
    static bool isIOS()         { return current() == PlatformType::iOS; }
    static bool isAndroid()     { return current() == PlatformType::Android; }
    static bool isWindows()     { return current() == PlatformType::Windows; }
    
    // 树莓派精准硬件识别 (编译期宏 + Linux 运行期设备树双重判定)
    static bool isRaspberryPi() {
#if defined(HIFI_PLATFORM_RPI)
        return true;
#elif defined(__linux__)
        static bool s_is_rpi = []() {
            // 1. 优先读取 device-tree model
            std::ifstream model_file("/proc/device-tree/model");
            if (model_file.is_open()) {
                std::string model;
                std::getline(model_file, model);
                if (model.find("Raspberry Pi") != std::string::npos) {
                    return true;
                }
            }
            // 2. 备用检查 /proc/cpuinfo
            std::ifstream cpuinfo("/proc/cpuinfo");
            if (cpuinfo.is_open()) {
                std::string line;
                while (std::getline(cpuinfo, line)) {
                    if (line.find("BCM2835") != std::string::npos ||
                        line.find("BCM2711") != std::string::npos ||
                        line.find("BCM2712") != std::string::npos ||
                        line.find("Raspberry Pi") != std::string::npos) {
                        return true;
                    }
                }
            }
            return false;
        }();
        return s_is_rpi;
#else
        return false;
#endif
    }

    // 目标渲染帧率规范：
    // 【核心策略】：除了树莓派使用 30fps (控温降载)，其他平台（Mac / iOS / Android / Windows）均使用 60fps
    static int getTargetFps() {
        return isRaspberryPi() ? 30 : 60;
    }

    // 目标单帧耗时预算 (秒)
    static double getTargetFrameTime() {
        return 1.0 / static_cast<double>(getTargetFps());
    }
};
