#pragma once
#include "imgui.h"
namespace Fonts {
    // 全局字体句柄指针
    inline ImFont* Small   = nullptr; // 12px: 分组小标题、发烧徽标、时间戳
    inline ImFont* Regular = nullptr; // 15px: 默认正文、菜单项、曲目列表
    inline ImFont* Medium  = nullptr; // 20px: 专辑名、分块大标题
    inline ImFont* Large   = nullptr; // 28px: 正在播放巨型曲名
    // 初始化载入多字阶字体集
    void initialize(ImGuiIO& io);
}