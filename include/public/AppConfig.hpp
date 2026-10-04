#include "public/Platform.hpp"
#include <string>
#include <cstdlib>
#include <filesystem>

// ==============================================================================
// 全局应用系统配置中心 (Single Source of Truth)
// 集中管理系统路径、曲库默认目录、硬件设备标识等全局设置
// ==============================================================================
namespace AppConfig {

    // 运行平台判定与帧率调度
    namespace Target {
        inline PlatformType getPlatform() { return Platform::current(); }
        inline std::string getPlatformName() { return Platform::name(); }
        inline std::string getDisplayName() { return Platform::displayName(); }
        inline int getTargetFps() { return Platform::getTargetFps(); }
        inline bool isRaspberryPi() { return Platform::isRaspberryPi(); }
    }

    namespace Path {
        // 默认曲库子路径相对目录 (如果希望直接扫描用户根目录可设为 ""，若扫描 Music 目录则设为 "/Music")
        // 【全局唯一配置点】：修改此处即可全局生效！
        inline constexpr const char* DefaultMusicSubDir = "";

        // 获取音乐库默认扫描根目录 (全局唯一事实来源)
        inline std::string& getMusicDir() {
            static std::string s_music_dir = []() {
                const char* home = std::getenv("HOME");
                if (home && home[0] != '\0') {
                    return std::string(home) + DefaultMusicSubDir;
                }
#if defined(HIFI_PLATFORM_RPI)
                return std::string("/home/pi") + DefaultMusicSubDir;
#else
                return std::string(DefaultMusicSubDir);
#endif
            }();
            return s_music_dir;
        }

        // 允许运行时动态更新或覆盖曲库扫描路径
        inline void setMusicDir(const std::string& path) {
            getMusicDir() = path;
        }

        // 获取用户配置文件存储目录 (全局唯一事实来源)
        inline std::string getConfigDir() {
            const char* home = std::getenv("HOME");
            std::string dir;
            if (home && home[0] != '\0') {
                dir = std::string(home) + "/.config/hifi_player";
            } else {
#if defined(HIFI_PLATFORM_RPI)
                dir = "/home/pi/.config/hifi_player";
#else
                dir = "./config";
#endif
            }
            return dir;
        }
    } // namespace Path

    namespace Audio {
        inline constexpr const char* DefaultDacName = "AK4499EX";
        inline constexpr const char* DefaultDacDesc = "AK4191EQ + AK4499EX 旗舰平衡解码前级设置";
    } // namespace Audio

} // namespace AppConfig
