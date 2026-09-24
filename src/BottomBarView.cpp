#include "BottomBarView.hpp"
#include "Font.hpp"
#include "PlayerAdmin.hpp"
#include "UIConfig.hpp"
#include <algorithm>
#include <string>

BottomBarView::BottomBarView() {}

// ==============================================================================
// 纯矢量贝塞尔平滑流线模式图标 (100% 还原截图二圆弧质感，支持 scale 自由缩放)
// ==============================================================================
static void DrawPlayModeIcon(ImDrawList* dl, ImVec2 center, PlayMode mode, ImU32 color, float scale = 1.35f) {
    const float th = 1.6f * scale; // 描边粗细跟随放大

    // 辅助 Lambda：自动将相对于中心的偏移量按比例放大
    auto P = [&](float dx, float dy) -> ImVec2 {
        return ImVec2(center.x + dx * scale, center.y + dy * scale);
    };

    switch (mode) {
        case PlayMode::LoopList: {
            // [列表循环]：圆润 U 型贝塞尔平滑回环 + 箭头 (完全复刻截图二)
            // 1. 上半圆弧（从左下圆润平滑拐到右上）
            dl->AddBezierCubic(P(-6.0f, 1.0f), P(-6.0f, -4.5f), P(-2.5f, -4.5f), P(3.0f, -4.5f), color, th);
            dl->AddTriangleFilled(P(7.5f, -4.5f), P(2.5f, -8.0f), P(2.5f, -1.0f), color);

            // 2. 下半圆弧（从右上圆润平滑拐到左下）
            dl->AddBezierCubic(P(6.0f, -1.0f), P(6.0f, 4.5f), P(2.5f, 4.5f), P(-3.0f, 4.5f), color, th);
            dl->AddTriangleFilled(P(-7.5f, 4.5f), P(-2.5f, 1.0f), P(-2.5f, 8.0f), color);
            break;
        }
        case PlayMode::LoopSingle: {
            // [单曲循环]：平滑圆弧 + 中心数字 "1"
            dl->AddBezierCubic(P(-6.0f, 1.0f), P(-6.0f, -5.0f), P(-2.5f, -5.0f), P(3.0f, -5.0f), color, th * 0.9f);
            dl->AddTriangleFilled(P(7.5f, -5.0f), P(2.5f, -8.5f), P(2.5f, -1.5f), color);

            dl->AddBezierCubic(P(6.0f, -1.0f), P(6.0f, 5.0f), P(2.5f, 5.0f), P(-3.0f, 5.0f), color, th * 0.9f);
            dl->AddTriangleFilled(P(-7.5f, 5.0f), P(-2.5f, 1.5f), P(-2.5f, 8.5f), color);

            // 中心数字 "1"
            dl->AddLine(P(0.0f, -3.0f), P(0.0f, 3.0f), color, th);
            dl->AddLine(P(-1.6f, -1.2f), P(0.0f, -3.0f), color, th * 0.9f);
            break;
        }
        case PlayMode::Shuffle: {
            // [随机播放]：截图二同款极度圆润的 S 型剪刀流线 (🔀)
            dl->AddBezierCubic(P(-7.0f, -4.5f), P(-2.0f, -4.5f), P(0.5f, 4.5f), P(4.5f, 4.5f), color, th);
            dl->AddTriangleFilled(P(8.5f, 4.5f), P(3.5f, 1.5f), P(3.5f, 7.5f), color);

            dl->AddBezierCubic(P(-7.0f, 4.5f), P(-2.0f, 4.5f), P(0.5f, -4.5f), P(4.5f, -4.5f), color, th);
            dl->AddTriangleFilled(P(8.5f, -4.5f), P(3.5f, -7.5f), P(3.5f, -1.5f), color);
            break;
        }
        case PlayMode::Sequence: {
            // [顺序播放]：平滑前进箭头 + 终止挡板 (➔|)
            dl->AddLine(P(-7.0f, 0.0f), P(3.0f, 0.0f), color, th);
            dl->AddTriangleFilled(P(6.5f, 0.0f), P(2.0f, -4.0f), P(2.0f, 4.0f), color);
            dl->AddLine(P(7.5f, -4.5f), P(7.5f, 4.5f), color, th);
            break;
        }
    }
}

// ==============================================================================
// 1. 绘制外部液态玻璃磨砂胶囊底板与 1px 微光折射边框
// ==============================================================================
void BottomBarView::drawCapsuleBackground(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    dl->AddRectFilled(p_min, p_max, UIConfig::Color::ContainerBg, rounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::ContainerBorder, rounding, 0, 1.0f);
}

// ==============================================================================
// 2. 左区：播放模式 -> 上一曲 -> 播放/暂停 -> 下一曲
// ==============================================================================
void BottomBarView::renderLeftControls(ImDrawList* dl, float start_x, float center_y) {
    auto& player = PlayerAdmin::getInstance();
    float cur_x = start_x;

    // 2.1 播放模式切换按钮 (纯悬浮 + 悬停玫红 / 移开灰白即时响应)
    {
        float btn_sz = 36.0f; // 按钮触控区域
        float btn_radius = btn_sz * 0.5f;
        ImVec2 btn_min(cur_x, center_y - btn_radius);
        ImVec2 btn_center(cur_x + btn_radius, center_y);

        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_mode", ImVec2(btn_sz, btn_sz))) {
            player.cyclePlayMode();
        }
        bool hov = ImGui::IsItemHovered();

        // 颜色直接响应：悬停时 Apple 标志性玫红，移开时中性纯正灰白 (彻底去掉淡入淡出，0 延迟、0 闪跳)
        ImU32 col_blur = IM_COL32(180, 180, 180, 255); // 纯正中性高级灰白
        ImU32 col_hover = UIConfig::Color::Accent;     // #FA2D48 纯粹玫红
        ImU32 dynamic_col = hov ? col_hover : col_blur;

        // 纯净矢量图标绘制 (传入 scale = 1.35f 放大图标本身)
        const float icon_scale = 1.35f;
        DrawPlayModeIcon(dl, btn_center, player.getPlayMode(), dynamic_col, icon_scale);

        cur_x += btn_sz + 6.0f; // 间距统一为 6px 紧凑舒适微距
    }
    // =========================================================================
    // 2.2 上一首按钮 (双左向三角形 ◀◀ + 悬停玫红 / 移开灰白即时响应)
    // =========================================================================
    {
        float btn_sz = 36.0f;
        float btn_radius = btn_sz * 0.5f;
        ImVec2 btn_min(cur_x, center_y - btn_radius);
        ImVec2 btn_center(cur_x + btn_radius, center_y);
        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_prev", ImVec2(btn_sz, btn_sz))) {
            player.previous(); // 触发上一首切歌
        }
        bool hov = ImGui::IsItemHovered();

        ImU32 col_blur = IM_COL32(180, 180, 180, 255);
        ImU32 col_hover = UIConfig::Color::Accent;
        ImU32 icon_col = hov ? col_hover : col_blur;

        // 绘制并排双左向三角形 ◀◀ (拉长三角形冲程，流线型更饱满)
        const float scale = 1.35f;
        auto P = [&](float dx, float dy) -> ImVec2 {
            return ImVec2(btn_center.x + dx * scale, btn_center.y + dy * scale);
        };
        // 核心几何参数：
        const float tri_w = 7.5f; // 💡 三角形横向长度 (从 5.5 拉长到 7.5，更有动感)
        const float tri_h = 5.0f; // 三角形半高 (上下对称)
        const float gap   = 2.0f; // 两只三角形之间的间距
        // 左边三角形 (◀ 尖端在最左侧)
        dl->AddTriangleFilled(P(-8.5f, 0.0f),         // 箭头尖端
                              P(-8.5f + tri_w, -tri_h), // 右上角
                              P(-8.5f + tri_w, tri_h),  // 右下角
                              icon_col);
        // 右边三角形 (◀ 紧随其后)
        float r_tip_x = -8.5f + tri_w + gap; // 右三角形尖端 X
        dl->AddTriangleFilled(P(r_tip_x, 0.0f),         // 箭头尖端
                              P(r_tip_x + tri_w, -tri_h), // 右上角
                              P(r_tip_x + tri_w, tri_h),  // 右下角
                              icon_col);
        cur_x += btn_sz + 6.0f; // 步进，按键之间留出 6px 紧凑舒适间隙
    }
}

// ==============================================================================
// 3. 中区：App Icon (先绘制字母 "A"，点击一键切换全屏)
// ==============================================================================
void BottomBarView::renderCenterIcon([[maybe_unused]] ImDrawList* dl, [[maybe_unused]] float center_x, [[maybe_unused]] float center_y) {}

// ==============================================================================
// 4. 右区：音量调节条 (小喇叭 + 发烧可拖拽滑块 + 百分比)
// ==============================================================================
void BottomBarView::renderRightVolume([[maybe_unused]] ImDrawList* dl, [[maybe_unused]] float right_limit, [[maybe_unused]] float center_y) {}

void BottomBarView::render(float screen_w, float screen_h) {
    const float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    const float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    const float rounding = height_ * 0.5f;                     // 24.0f (半高半圆)

    // 几何对齐：
    float left_x = UIConfig::Layout::SidebarWidth + margin_x;
    float right_x = screen_w - margin_x;
    float bot_y = screen_h - margin_y;
    float top_y = bot_y - height_;
    float center_y = (top_y + bot_y) * 0.5f;

    // 1. 设置窗口位置与大小
    ImGui::SetNextWindowPos(ImVec2(left_x, top_y));
    ImGui::SetNextWindowSize(ImVec2(right_x - left_x, height_));

    // 2. 窗口标志：无标题栏、无多余边框、无背景
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##BottomBarContainer", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 渲染液态玻璃的底层
    drawCapsuleBackground(dl, ImVec2(left_x, top_y), ImVec2(right_x, bot_y), rounding);

    // 左边操作块
    renderLeftControls(dl, left_x + rounding + 4.0f, center_y);

    ImGui::End();
    ImGui::PopStyleVar();
}