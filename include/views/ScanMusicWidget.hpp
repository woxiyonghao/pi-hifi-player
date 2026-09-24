#pragma once

#include "imgui.h"
#include "types/MusicModel.hpp"
#include "tools/MusicScanManager.hpp"
#include <vector>
#include <string>

// ==============================================================================
// 本地曲库检索专属交互组件 (ScanMusicWidget)
// 包含 3 种状态驱动模式：1.待机展示 2.动态扫描中 3.完成结果
// ==============================================================================
class ScanMusicWidget {
public:
    ScanMusicWidget();
    ~ScanMusicWidget() = default;

    // 主渲染入口：在传入的卡片矩形 [p_min, p_max] 内部自适应居中排版
    void render(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, std::vector<Playlist>& playlists);

private:
    // -------------------------------------------------------------------------
    // 三大核心状态子面板
    // -------------------------------------------------------------------------
    // 样式 1：待机未扫描状态 (纯矢量 🔍 放大镜 + 上下左右平滑巡游 + 侧边栏同款 Alpha 胶囊按键)
    void renderIdleState(ImDrawList* dl, ImVec2 center, std::vector<Playlist>& playlists);

    // 样式 2：扫描中状态 (Icon 大幅度巡游穿梭 + 底部主题色激光曲速光流动画)
    void renderScanningState(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImVec2 center);

    // 样式 3：扫描完成状态 (预留：发烧规格仪表盘 + 导入结果)
    void renderCompletedState(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, std::vector<Playlist>& playlists);

    // -------------------------------------------------------------------------
    // 矢量图形绘制工具
    // -------------------------------------------------------------------------
    // 绘制纯 GPU 矢量发烧大号放大镜 🔍 (支持 2D 上下左右多维浮动与雷达探针)
    void drawSearchIcon(ImDrawList* dl, ImVec2 center, float radius, float offset_x, float offset_y, bool is_scanning = false);

    // 绘制底部星球大战风格主题色曲速跃迁激光流 (Procedural Warp Laser System)
    void drawLaserWarpAnimation(ImDrawList* dl, ImVec2 emitter_pos, ImVec2 p_min, ImVec2 p_max);

private:
    char default_scan_path_[256] = "";
    float anim_timer_ = 0.0f; // 驱动待机呼吸与悬浮的连续时间基准
};