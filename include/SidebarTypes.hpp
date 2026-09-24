#pragma once

#include <cstdint>

// 侧边栏全局页面与功能路由枚举
enum class SidebarTab : uint8_t {
    ScanMusic,      // 扫描音乐曲库
    Equalizer,      // 10段图形 EQ 调音台
    DACSettings,    // DAC 硬件滤波与时钟输出
    ThemeSettings,  // 调色盘与液态玻璃主题风格
    SystemSettings, // 树莓派底层硬件与系统配置
    AllMusic,       // 全部曲库 (播放列表根视图)
    CustomPlaylist  // 自定义实体歌单
};

// 通用指示器目标坐标 (供父容器绘制发光滑动胶囊)
struct NavIndicatorTarget {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class NavIcon : uint8_t {
    Search,         // 放大镜 (扫描音乐)
    Equalizer,      // 调音滑块 (EQ)
    DAC,            // 芯片底座 (DAC)
    Theme,          // 调色板 (主题)
    System,         // 齿轮滑块 (系统)
    Music,          // 音符 (所有音乐)
    Playlist,       // 列表横线 (播放列表)
    Add             // 加号 (添加播放列表)
};