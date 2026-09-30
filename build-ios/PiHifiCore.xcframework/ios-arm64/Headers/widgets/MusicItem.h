#pragma once

#include "imgui.h"
#include "types/MusicModel.hpp"
#include <string>

// ==============================================================================
// 发烧级单曲卡片独立渲染组件 (MusicItem)
// 统一管控曲目封面、格式指示徽标、悬停动效、文字截断排版与交互事件
// ==============================================================================
class MusicItem {
public:
    // 默认发烧卡片排版常量
    static constexpr float DefaultWidth = 150.0f;
    static constexpr float DefaultHeight = 225.0f;
    static constexpr float DefaultRounding = 10.0f;

    MusicItem() = default;
    ~MusicItem() = default;

    // 渲染卡片：
    // dl: 绘制目标 DrawList (传入当前 Child 窗口的 DrawList 即可开启硬件裁剪)
    // pos: 屏幕绝对位置 (左上角起始坐标)
    // size: 卡片总尺寸 (默认 150 × 225)
    // track: 曲目元数据实体模型
    // category_tag: 可选分类标签 (如 "Studio Master"、"流行热播"，为 nullptr 时自动忽略)
    // 返回值: 是否被用户点击 (true 代表触发点击播放，内置触控拖拽防误触过滤)
    static bool render(ImDrawList* dl, ImVec2 pos, ImVec2 size, const Track& track, const char* category_tag = nullptr);

    // 便捷重载：自动获取当前 ImGui 光标所在屏幕位置 (GetCursorScreenPos)
    static bool render(ImDrawList* dl, ImVec2 size, const Track& track, const char* category_tag = nullptr);

    // 辅助工具：自动计算文字宽度并生成末尾带 "..." 的截断文本 (支持 UTF-8 多字节安全截断)
    static std::string truncateTextWithEllipsis(const std::string& text, float max_width);

    // 矢量黑胶/CD 光盘图标绘制工具 (1:1 纯 GPU 矢量还原发烧唱片质感，支持动态旋转)
    static void drawVinylCdIcon(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, float rotation_rad = 0.0f);
};
