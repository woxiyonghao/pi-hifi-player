#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <cstdlib>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include <sys/sysctl.h>
#include <objc/runtime.h>
#include <objc/message.h>
#endif
#endif

// ==============================================================================
// 全局跨平台硬件与操作系统架构定义 (Single Source of Truth)
// 严格精确区分 6 大核心平台架构:
// 1. mac     (macOS 桌面端)
// 2. window  (Windows 桌面端)
// 3. linux   (通用 Linux / 树莓派 5 发烧纯音中枢)
// 4. ipad    (Apple iPad / iPadOS 平板端)
// 5. iphone  (Apple iPhone 手机端)
// 6. android (Android 手机 / 车载 / 便携播放器)
// ==============================================================================
enum class PlatformType {
    Mac,
    Window,
    Linux,
    IPad,
    IPhone,
    Android,
    Unknown,

    // 兼容历史命名别名
    MacOS = Mac,
    Windows = Window,
    iOS = IPhone,
    RaspberryPi = Linux
};

class Platform {
public:
    // 获取当前运行平台类型 (支持运行时动态注入与硬件指纹自动探测)
    static PlatformType current() {
        if (s_override_platform != PlatformType::Unknown) {
            return s_override_platform;
        }

#if defined(__APPLE__)
    #if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        return detectAppleMobilePlatform();
    #else
        return PlatformType::Mac;
    #endif
#elif defined(__ANDROID__)
        return PlatformType::Android;
#elif defined(_WIN32) || defined(_WIN64)
        return PlatformType::Window;
#elif defined(__linux__)
        return PlatformType::Linux;
#else
        return PlatformType::Unknown;
#endif
    }

    // 允许应用程序层 (如 iOS AppDelegate / PadViewController) 运行时显式指定
    static void setOverride(PlatformType override_type) {
        s_override_platform = override_type;
    }

    // 重置覆盖值，恢复自动检测
    static void resetOverride() {
        s_override_platform = PlatformType::Unknown;
    }

    // 获取当前平台标准小写字符串标识 (mac, window, linux, ipad, iphone, android)
    static std::string name() {
        switch (current()) {
            case PlatformType::Mac:     return "mac";
            case PlatformType::Window:  return "window";
            case PlatformType::Linux:   return "linux";
            case PlatformType::IPad:    return "ipad";
            case PlatformType::IPhone:  return "iphone";
            case PlatformType::Android: return "android";
            default:                    return "unknown";
        }
    }

    // 获取友好显示名称
    static std::string displayName() {
        switch (current()) {
            case PlatformType::Mac:     return "macOS Desktop";
            case PlatformType::Window:  return "Windows Desktop";
            case PlatformType::Linux:
                return isRaspberryPi() ? "Raspberry Pi 5 (Linux HiFi Engine)" : "Linux Desktop / Embedded";
            case PlatformType::IPad:    return "Apple iPad / iPadOS";
            case PlatformType::IPhone:  return "Apple iPhone / iOS";
            case PlatformType::Android: return "Android Mobile / Tablet";
            default:                    return "Generic Platform";
        }
    }

    // 快捷平台判断方法 (六大核心分类)
    static bool isMac()         { return current() == PlatformType::Mac; }
    static bool isWindow()      { return current() == PlatformType::Window; }
    static bool isLinux()       { return current() == PlatformType::Linux; }
    static bool isIPad()        { return current() == PlatformType::IPad; }
    static bool isPhone()       { return current() == PlatformType::IPhone || current() == PlatformType::Android; }
    static bool isIPhone()      { return isPhone(); }
    static bool isAndroid()     { return current() == PlatformType::Android; }

    // 兼容与复合形态判断
    static bool isMacOS()       { return isMac(); }
    static bool isWindows()     { return isWindow(); }
    static bool isIOS()         { return isIPad() || isIPhone(); }
    static bool isMobile()      { return isIPad() || isIPhone() || isAndroid(); }
    static bool isTablet()      { return isIPad(); }
    static bool isDesktop()     { return isMac() || isWindow() || (isLinux() && !isRaspberryPi()); }

    // 树莓派精准硬件识别 (运行在 Linux 内核上的特定单板计算机)
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
    // 【核心策略】：树莓派使用 30fps (控温降载)，其他平台（Mac / Windows / Linux / iPad / iPhone / Android）默认 60fps
    static int getTargetFps() {
        return isRaspberryPi() ? 30 : 60;
    }

    // 目标单帧耗时预算 (秒)
    static double getTargetFrameTime() {
        return 1.0 / static_cast<double>(getTargetFps());
    }

private:
#if defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    // iOS / iPadOS 动态硬件形态智能判定
    static PlatformType detectAppleMobilePlatform() {
        // 1. 运行时优先通过 UIKit UIDevice 查询 userInterfaceIdiom (Pad=1, Phone=0)
        Class ui_device_class = objc_getClass("UIDevice");
        if (ui_device_class != nullptr) {
            SEL sel_current = sel_registerName("currentDevice");
            id device_inst = ((id (*)(Class, SEL))objc_msgSend)(ui_device_class, sel_current);
            if (device_inst != nullptr) {
                SEL sel_idiom = sel_registerName("userInterfaceIdiom");
                long idiom = ((long (*)(id, SEL))objc_msgSend)(device_inst, sel_idiom);
                if (idiom == 1) return PlatformType::IPad;
                if (idiom == 0) return PlatformType::IPhone;
            }
        }

        // 2. 模拟器环境识别 (读取 Xcode 注入的 SIMULATOR_MODEL_IDENTIFIER)
        const char* sim_model = std::getenv("SIMULATOR_MODEL_IDENTIFIER");
        if (sim_model != nullptr) {
            std::string sm(sim_model);
            if (sm.find("iPad") != std::string::npos) return PlatformType::IPad;
            if (sm.find("iPhone") != std::string::npos) return PlatformType::IPhone;
        }

        // 3. 真机内核硬件特征识别 (hw.machine: "iPad13,4", "iPhone14,2" 等)
        char hw_machine[128] = {0};
        size_t len = sizeof(hw_machine);
        if (sysctlbyname("hw.machine", hw_machine, &len, nullptr, 0) == 0) {
            std::string hm(hw_machine);
            if (hm.find("iPad") != std::string::npos) return PlatformType::IPad;
            if (hm.find("iPhone") != std::string::npos) return PlatformType::IPhone;
        }

        // 默认按 iPad 宽屏回退
        return PlatformType::IPad;
    }
#endif

private:
    inline static PlatformType s_override_platform = PlatformType::Unknown;
};
