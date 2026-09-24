#include "BottomBarView.hpp"
#include "Font.hpp"
#include "PlayerAdmin.hpp"
#include "UIConfig.hpp"
#include <algorithm>
#include <cmath>
#include <string>

BottomBarView::BottomBarView() {}

// ==============================================================================
// 纯矢量平滑圆角多边形/三角形 (Apple 质感圆润倒角，彻底告别尖锐与生硬感)
// ==============================================================================
static void DrawRoundedTriangle(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImVec2 p2, float radius, ImU32 col) {
    if (radius <= 0.1f) {
        dl->AddTriangleFilled(p0, p1, p2, col);
        return;
    }

    // 强制顺时针绕向 (CW winding order)：确保 ImGui 外抗锯齿羽化边界 (AA fringe) 完美向外展开，边缘极致丝滑
    float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (cross < 0.0f) {
        std::swap(p1, p2);
    }

    const ImVec2 pts[3] = { p0, p1, p2 };
    for (int i = 0; i < 3; ++i) {
        ImVec2 prev_p = pts[(i + 2) % 3];
        ImVec2 curr_p = pts[i];
        ImVec2 next_p = pts[(i + 1) % 3];

        float v1x = prev_p.x - curr_p.x;
        float v1y = prev_p.y - curr_p.y;
        float v2x = next_p.x - curr_p.x;
        float v2y = next_p.y - curr_p.y;

        float len1 = std::sqrt(v1x * v1x + v1y * v1y);
        float len2 = std::sqrt(v2x * v2x + v2y * v2y);
        if (len1 < 1e-4f || len2 < 1e-4f) {
            continue;
        }

        float u1x = v1x / len1, u1y = v1y / len1;
        float u2x = v2x / len2, u2y = v2y / len2;

        float dot = std::clamp(u1x * u2x + u1y * u2y, -1.0f, 1.0f);
        float half_angle = std::acos(dot) * 0.5f;
        float d = (half_angle > 1e-3f) ? (radius / std::tan(half_angle)) : 0.0f;
        d = std::min(d, std::min(len1, len2) * 0.46f);

        ImVec2 t_in(curr_p.x + u1x * d, curr_p.y + u1y * d);
        ImVec2 t_out(curr_p.x + u2x * d, curr_p.y + u2y * d);

        dl->PathLineTo(t_in);
        // 使用 4 段高阶贝塞尔细分，保证即使在高分屏下转角也犹如矢量水滴般丝滑圆润
        dl->PathBezierQuadraticCurveTo(curr_p, t_out, 4);
    }
    dl->PathFillConvex(col);
}

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
            dl->AddCircleFilled(P(-6.0f, 1.0f), th * 0.5f, color); // 倒角圆润末端
            DrawRoundedTriangle(dl, P(2.5f, -8.0f), P(7.5f, -4.5f), P(2.5f, -1.0f), 0.9f * scale, color);

            // 2. 下半圆弧（从右上圆润平滑拐到左下）
            dl->AddBezierCubic(P(6.0f, -1.0f), P(6.0f, 4.5f), P(2.5f, 4.5f), P(-3.0f, 4.5f), color, th);
            dl->AddCircleFilled(P(6.0f, -1.0f), th * 0.5f, color); // 倒角圆润末端
            DrawRoundedTriangle(dl, P(-2.5f, 1.0f), P(-7.5f, 4.5f), P(-2.5f, 8.0f), 0.9f * scale, color);
            break;
        }
        case PlayMode::LoopSingle: {
            // [单曲循环]：平滑圆弧 + 中心数字 "1"
            dl->AddBezierCubic(P(-6.0f, 1.0f), P(-6.0f, -5.0f), P(-2.5f, -5.0f), P(3.0f, -5.0f), color, th * 0.9f);
            dl->AddCircleFilled(P(-6.0f, 1.0f), th * 0.45f, color);
            DrawRoundedTriangle(dl, P(2.5f, -8.5f), P(7.5f, -5.0f), P(2.5f, -1.5f), 0.9f * scale, color);

            dl->AddBezierCubic(P(6.0f, -1.0f), P(6.0f, 5.0f), P(2.5f, 5.0f), P(-3.0f, 5.0f), color, th * 0.9f);
            dl->AddCircleFilled(P(6.0f, -1.0f), th * 0.45f, color);
            DrawRoundedTriangle(dl, P(-2.5f, 1.5f), P(-7.5f, 5.0f), P(-2.5f, 8.5f), 0.9f * scale, color);

            // 中心数字 "1"
            dl->AddLine(P(0.0f, -3.0f), P(0.0f, 3.0f), color, th);
            dl->AddCircleFilled(P(0.0f, -3.0f), th * 0.5f, color);
            dl->AddCircleFilled(P(0.0f, 3.0f), th * 0.5f, color);
            dl->AddLine(P(-1.6f, -1.2f), P(0.0f, -3.0f), color, th * 0.9f);
            break;
        }
        case PlayMode::Shuffle: {
            // [随机播放]：截图二同款极度圆润的 S 型剪刀流线 (🔀)
            dl->AddBezierCubic(P(-7.0f, -4.5f), P(-2.0f, -4.5f), P(0.5f, 4.5f), P(4.5f, 4.5f), color, th);
            dl->AddCircleFilled(P(-7.0f, -4.5f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(3.5f, 1.5f), P(8.5f, 4.5f), P(3.5f, 7.5f), 0.9f * scale, color);

            dl->AddBezierCubic(P(-7.0f, 4.5f), P(-2.0f, 4.5f), P(0.5f, -4.5f), P(4.5f, -4.5f), color, th);
            dl->AddCircleFilled(P(-7.0f, 4.5f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(3.5f, -7.5f), P(8.5f, -4.5f), P(3.5f, -1.5f), 0.9f * scale, color);
            break;
        }
        case PlayMode::Sequence: {
            // [顺序播放]：平滑前进箭头 + 终止挡板 (➔|)
            dl->AddLine(P(-7.0f, 0.0f), P(3.0f, 0.0f), color, th);
            dl->AddCircleFilled(P(-7.0f, 0.0f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(2.0f, -4.0f), P(6.5f, 0.0f), P(2.0f, 4.0f), 0.9f * scale, color);
            dl->AddRectFilled(P(6.8f, -4.5f), P(8.2f, 4.5f), color, 0.7f * scale);
            break;
        }
    }
}

// ==============================================================================
// 纯矢量分段式 LED 律动频谱图标 (参考发烧 HiFi 均衡器：暗底点阵 + 跃动灰色发光块)
// ==============================================================================
static void DrawLEDSpectrumIcon(ImDrawList* dl, ImVec2 center, float total_w, float total_h,
                               int num_cols, int num_rows, ImU32 lit_color, ImU32 unlit_color,
                               bool is_animating) {
    const float gap_x = 1.2f;
    const float gap_y = 1.0f;
    const float col_w = (total_w - (num_cols - 1) * gap_x) / num_cols;
    const float seg_h = (total_h - (num_rows - 1) * gap_y) / num_rows;
    const float seg_round = 0.4f;

    const float start_x = center.x - total_w * 0.5f;
    const float bot_y   = center.y + total_h * 0.5f;

    float t = static_cast<float>(ImGui::GetTime());

    // 经典 EQ 频段静态高度分布模板 (低频至高频的自然曲线，即使未播放时也能呈现精美静态频谱造型)
    static const float base_pattern[12] = {
        0.30f, 0.45f, 0.65f, 0.50f, 0.85f, 0.60f,
        0.95f, 0.75f, 0.40f, 0.70f, 0.55f, 0.35f
    };

    for (int c = 0; c < num_cols; ++c) {
        float x0 = start_x + c * (col_w + gap_x);
        float x1 = x0 + col_w;

        // 计算当前列点亮的格数 (从底部往上计数)
        int active_count = 1;
        float base_val = base_pattern[c % 12];
        if (is_animating) {
            // 播放状态：采用多频正余弦波叠加，随音乐节奏动态起伏跳跃
            float freq1 = 4.2f + (c % 3) * 1.6f;
            float freq2 = 8.0f - (c % 2) * 2.2f;
            float phase = c * 0.85f;
            float wave = std::sin(t * freq1 + phase) * 0.35f + 
                         std::cos(t * freq2 - phase * 1.4f) * 0.22f + (base_val * 0.55f);
            wave = std::clamp(wave, 0.12f, 1.0f);
            active_count = std::max(1, static_cast<int>(std::round(wave * num_rows)));
        } else {
            // 非播放状态：严格静止不动 (0 动画，完全脱离时间 t，呈现固定静止均衡器轮廓)
            float wave = base_val * 0.45f;
            active_count = std::max(1, static_cast<int>(std::round(wave * num_rows)));
        }

        for (int r = 0; r < num_rows; ++r) {
            float y1 = bot_y - r * (seg_h + gap_y);
            float y0 = y1 - seg_h;

            bool is_lit = (r < active_count);
            ImU32 seg_col = is_lit ? lit_color : unlit_color;

            if (!is_lit && (unlit_color & IM_COL32_A_MASK) == 0) {
                continue;
            }

            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), seg_col, seg_round);
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

    // 基础调色板 (低饱和中性暖灰，移开灰白、悬停玫红即时响应)
    const ImU32 col_blur = IM_COL32(130, 127, 123, 255); // #827F7B Apple 同款低饱和高级暖灰
    const ImU32 col_hover = UIConfig::Color::Accent;      // #FA2D48 纯粹玫红

    // 视觉尺寸与间距精密计算 (确保相邻 icon 边界间隙纯视觉严格等于 12px)
    // 1. 播放模式 (Mode)   : 视觉半宽 9.0px
    // 2. 上一首   (Prev)   : 视觉半宽 6.2px
    // 3. 播放/暂停 (Play)   : 视觉半宽 6.4px (播放态尖端在 +6.8px，基底在 -5.6px)
    // 4. 下一首   (Next)   : 视觉半宽 6.2px
    //
    // 边界与边界之间的净间隙设定为 12.0px：
    // - Mode 到 Prev: (c_prev - 6.2) - (c_mode + 9.0) = 12.0  => c_prev = c_mode + 27.2px
    // - Prev 到 Play: (c_play - 5.6) - (c_prev + 6.2) = 12.0  => c_play = c_prev + 23.8px
    // - Play 到 Next: (c_next - 6.2) - (c_play + 6.8) = 12.0  => c_next = c_play + 25.0px
    const float c_mode = start_x + 10.0f;
    const float c_prev = c_mode + 27.2f;
    const float c_play = c_prev + 23.8f;
    const float c_next = c_play + 25.0f;

    // 触控/点击交互区无缝划分 (100% 覆盖无死角，手指轻触即应)
    const float x0 = c_mode - 14.0f;
    const float x1 = (c_mode + c_prev) * 0.5f;
    const float x2 = (c_prev + c_play) * 0.5f;
    const float x3 = (c_play + c_next) * 0.5f;
    const float x4 = c_next + 14.0f;

    // =========================================================================
    // 2.1 播放模式切换按钮 (纯悬浮 + 悬停玫红 / 移开灰白即时响应)
    // =========================================================================
    {
        ImVec2 btn_min(x0, center_y - 18.0f);
        ImVec2 btn_size(x1 - x0, 36.0f);
        ImVec2 btn_center(c_mode, center_y);

        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_mode", btn_size)) {
            player.cyclePlayMode();
        }
        bool hov = ImGui::IsItemHovered();
        ImU32 dynamic_col = hov ? col_hover : col_blur;

        const float icon_scale = 1.25f;
        DrawPlayModeIcon(dl, btn_center, player.getPlayMode(), dynamic_col, icon_scale);
    }

    // =========================================================================
    // 2.2 上一首按钮 (小巧精致双左向圆角小三角形 ◀◀)
    // =========================================================================
    {
        ImVec2 btn_min(x1, center_y - 18.0f);
        ImVec2 btn_size(x2 - x1, 36.0f);
        ImVec2 btn_center(c_prev, center_y);

        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_prev", btn_size)) {
            player.previous();
        }
        bool hov = ImGui::IsItemHovered();
        ImU32 icon_col = hov ? col_hover : col_blur;

        // 小巧精致：高度 10px，左右两枚圆润左向三角形
        const float tri_h = 5.0f;  // 半高
        const float tri_w = 5.2f;  // 三角形横向冲程
        const float gap   = 1.8f;  // 两个三角形间隙
        const float r_tri = 1.3f;  // 倒角圆润半径

        // 左边三角形 (◀ 尖端朝左)
        // 顺时针顺序：右下 -> 尖端(左) -> 右上
        ImVec2 l_p0(btn_center.x - gap * 0.5f, btn_center.y + tri_h);
        ImVec2 l_p1(btn_center.x - gap * 0.5f - tri_w, btn_center.y);
        ImVec2 l_p2(btn_center.x - gap * 0.5f, btn_center.y - tri_h);
        DrawRoundedTriangle(dl, l_p0, l_p1, l_p2, r_tri, icon_col);

        // 右边三角形 (◀ 尖端朝左)
        // 顺时针顺序：右下 -> 尖端(左) -> 右上
        ImVec2 r_p0(btn_center.x + gap * 0.5f + tri_w, btn_center.y + tri_h);
        ImVec2 r_p1(btn_center.x + gap * 0.5f, btn_center.y);
        ImVec2 r_p2(btn_center.x + gap * 0.5f + tri_w, btn_center.y - tri_h);
        DrawRoundedTriangle(dl, r_p0, r_p1, r_p2, r_tri, icon_col);
    }

    // =========================================================================
    // 2.3 播放/暂停按钮 (核心 C 位：播放饱满圆润大三角 ▶ / 暂停双胶囊竖条 ⏸)
    // =========================================================================
    {
        ImVec2 btn_min(x2, center_y - 18.0f);
        ImVec2 btn_size(x3 - x2, 36.0f);
        ImVec2 btn_center(c_play, center_y);

        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_play_pause", btn_size)) {
            player.togglePlayPause();
        }
        bool hov = ImGui::IsItemHovered();
        ImU32 icon_col = hov ? col_hover : col_blur;

        if (player.isPlaying()) {
            // [正在播放中 -> 呈现暂停图标 ⏸]：两条完全圆润对称的竖向药丸胶囊
            const float bar_w = 3.6f;
            const float bar_h = 15.0f;
            const float bar_gap = 4.0f;
            const float bar_round = bar_w * 0.5f; // 1.8f 完全半圆胶囊封顶底
            float half_w = bar_w + bar_gap * 0.5f; // 3.6 + 2.0 = 5.6f
            float half_h = bar_h * 0.5f;           // 7.5f

            // 左竖条
            dl->AddRectFilled(ImVec2(btn_center.x - half_w, btn_center.y - half_h),
                              ImVec2(btn_center.x - half_w + bar_w, btn_center.y + half_h),
                              icon_col, bar_round);
            // 右竖条
            dl->AddRectFilled(ImVec2(btn_center.x + bar_gap * 0.5f, btn_center.y - half_h),
                              ImVec2(btn_center.x + half_w, btn_center.y + half_h),
                              icon_col, bar_round);
        } else {
            // [暂停/未播放 -> 呈现播放图标 ▶]：大圆角流线饱满右向大三角 (内置 +0.6px 光学重心微调)
            const float tri_h = 7.5f;   // 半高 (总高 15.0f，视觉黄金分割)
            const float tri_w = 12.4f;  // 冲程横宽
            const float opt_x = 0.6f;   // 光学重心向右补偿
            const float r_play = 2.4f;  // Apple 同款丝滑圆润倒角

            // 顺时针顺序：左上 -> 尖端(右) -> 左下
            ImVec2 p0(btn_center.x - tri_w * 0.5f + opt_x, btn_center.y - tri_h); // 左上 (-5.6, -7.5)
            ImVec2 p1(btn_center.x + tri_w * 0.5f + opt_x, btn_center.y);         // 尖端 (+6.8, 0.0)
            ImVec2 p2(btn_center.x - tri_w * 0.5f + opt_x, btn_center.y + tri_h); // 左下 (-5.6, +7.5)
            DrawRoundedTriangle(dl, p0, p1, p2, r_play, icon_col);
        }
    }

    // =========================================================================
    // 2.4 下一首按钮 (小巧精致双右向圆角小三角形 ▶▶)
    // =========================================================================
    {
        ImVec2 btn_min(x3, center_y - 18.0f);
        ImVec2 btn_size(x4 - x3, 36.0f);
        ImVec2 btn_center(c_next, center_y);

        ImGui::SetCursorScreenPos(btn_min);
        if (ImGui::InvisibleButton("##btn_next", btn_size)) {
            player.next();
        }
        bool hov = ImGui::IsItemHovered();
        ImU32 icon_col = hov ? col_hover : col_blur;

        // 与上一首严格镜像对称的圆润小巧尺度
        const float tri_h = 5.0f;  // 半高
        const float tri_w = 5.2f;  // 三角形横向冲程
        const float gap   = 1.8f;  // 两个三角形间隙
        const float r_tri = 1.3f;  // 倒角圆润半径

        // 左边三角形 (▶ 尖端朝右)
        // 顺时针顺序：左上 -> 尖端(右) -> 左下
        ImVec2 l_p0(btn_center.x - gap * 0.5f - tri_w, btn_center.y - tri_h);
        ImVec2 l_p1(btn_center.x - gap * 0.5f, btn_center.y);
        ImVec2 l_p2(btn_center.x - gap * 0.5f - tri_w, btn_center.y + tri_h);
        DrawRoundedTriangle(dl, l_p0, l_p1, l_p2, r_tri, icon_col);

        // 右边三角形 (▶ 尖端朝右)
        // 顺时针顺序：左上 -> 尖端(右) -> 左下
        ImVec2 r_p0(btn_center.x + gap * 0.5f, btn_center.y - tri_h);
        ImVec2 r_p1(btn_center.x + gap * 0.5f + tri_w, btn_center.y);
        ImVec2 r_p2(btn_center.x + gap * 0.5f, btn_center.y + tri_h);
        DrawRoundedTriangle(dl, r_p0, r_p1, r_p2, r_tri, icon_col);
    }
}

// ==============================================================================
// 3. 中区：底栏正中间宽屏 LED 动态点阵频谱区 (参考用户红框位置)
// ==============================================================================
void BottomBarView::renderCenterIcon(ImDrawList* dl, float center_x, float center_y) {
    auto& player = PlayerAdmin::getInstance();
    const ImU32 col_blur = IM_COL32(130, 127, 123, 255); // 播放按钮同款中性暖灰
    const ImU32 col_hover = UIConfig::Color::Accent;     // 悬停玫红高亮
    const ImU32 unlit_col = IM_COL32(255, 255, 255, 18); // 暗底未点亮段

    // 宽度减少 16px (从 64px 缩减至 48px，比例更精致)
    const float eq_w = 48.0f;
    const float eq_h = 20.0f;
    const int num_cols = 12;
    const int num_rows = 10;

    // 交互按键：点击亦可翻转播放/暂停，悬停变色
    ImVec2 btn_min(center_x - eq_w * 0.5f - 6.0f, center_y - 18.0f);
    ImVec2 btn_size(eq_w + 12.0f, 36.0f);
    ImGui::SetCursorScreenPos(btn_min);
    if (ImGui::InvisibleButton("##btn_center_spectrum", btn_size)) {
        player.togglePlayPause();
    }
    bool hov = ImGui::IsItemHovered();
    ImU32 lit_col = hov ? col_hover : col_blur;

    DrawLEDSpectrumIcon(dl, ImVec2(center_x, center_y), eq_w, eq_h, num_cols, num_rows,
                        lit_col, unlit_col, player.isPlaying());
}

// ==============================================================================
// 纯矢量小喇叭图标 (支持发声声波与静音 ✕ 切换，纯几何抗锯齿)
// ==============================================================================
static void DrawSpeakerIcon(ImDrawList* dl, ImVec2 center, float vol, bool is_muted, ImU32 color) {
    const float cx = center.x;
    const float cy = center.y;

    // 1. 喇叭磁铁与扩音锥体
    dl->AddRectFilled(ImVec2(cx - 6.5f, cy - 3.0f), ImVec2(cx - 3.5f, cy + 3.0f), color, 0.6f);
    ImVec2 pts[4] = {
        ImVec2(cx - 3.5f, cy - 3.0f),
        ImVec2(cx + 0.5f, cy - 6.0f),
        ImVec2(cx + 0.5f, cy + 6.0f),
        ImVec2(cx - 3.5f, cy + 3.0f)
    };
    dl->AddConvexPolyFilled(pts, 4, color);

    // 2. 声波弧线 / 静音叉号
    if (is_muted || vol <= 0.001f) {
        // 静音形态：右侧绘制精美小叉号 ✕
        const float x_c = cx + 5.0f;
        const float r = 3.0f;
        dl->AddLine(ImVec2(x_c - r, cy - r), ImVec2(x_c + r, cy + r), color, 1.4f);
        dl->AddLine(ImVec2(x_c - r, cy + r), ImVec2(x_c + r, cy - r), color, 1.4f);
    } else {
        // 第一道声波弧 (小音量)
        dl->PathArcTo(ImVec2(cx - 0.5f, cy), 5.0f, -0.7f, 0.7f, 8);
        dl->PathStroke(color, 0, 1.4f);

        // 第二道声波弧 (大音量 vol > 0.45f)
        if (vol > 0.45f) {
            dl->PathArcTo(ImVec2(cx - 0.5f, cy), 8.5f, -0.7f, 0.7f, 10);
            dl->PathStroke(color, 0, 1.4f);
        }
    }
}

// ==============================================================================
// 4. 右区：音量调节条 (小喇叭 + 发烧可拖拽滑块 + 百分比)
// ==============================================================================
void BottomBarView::renderRightVolume(ImDrawList* dl, float right_limit, float center_y) {
    auto& player = PlayerAdmin::getInstance();
    float vol = player.getVolume();
    bool is_muted = player.isMuted();

    const ImU32 col_blur = IM_COL32(130, 127, 123, 255); // 控制栏同款高级暖灰
    const ImU32 col_hover = UIConfig::Color::Accent;      // 悬停玫红
    const ImU32 col_text = UIConfig::Color::TextMuted;    // 次级文字灰

    // 几何排版参数：
    // [小喇叭 22px] -> 间隙 6px -> [滑块条 90px] -> 间隙 8px -> [百分比文字 ~34px]
    const float spk_w   = 22.0f;
    const float track_w = 90.0f;
    const float track_h = 4.0f;
    const float text_w  = 34.0f;
    const float gap1    = 6.0f;
    const float gap2    = 8.0f;
    const float total_w = spk_w + gap1 + track_w + gap2 + text_w;

    const float start_x = right_limit - total_w;

    // -------------------------------------------------------------------------
    // 4.1 小喇叭按钮 (点击静音/解静音)
    // -------------------------------------------------------------------------
    float spk_cx = start_x + spk_w * 0.5f;
    ImVec2 spk_min(start_x, center_y - 14.0f);
    ImGui::SetCursorScreenPos(spk_min);
    if (ImGui::InvisibleButton("##btn_spk_mute", ImVec2(spk_w, 28.0f))) {
        player.toggleMute();
    }
    bool spk_hov = ImGui::IsItemHovered();
    ImU32 spk_col = spk_hov ? col_hover : col_blur;
    DrawSpeakerIcon(dl, ImVec2(spk_cx, center_y), vol, is_muted, spk_col);

    // -------------------------------------------------------------------------
    // 4.2 音量拖拽滑块条 (支持点击、连续拖拽、滚轮微调)
    // -------------------------------------------------------------------------
    float track_x0 = start_x + spk_w + gap1;
    float track_x1 = track_x0 + track_w;
    float track_y0 = center_y - track_h * 0.5f;
    float track_y1 = center_y + track_h * 0.5f;

    // 交互响应区 (纵向扩大至 28px，手指触控和鼠标都能轻松点击拖拽)
    ImVec2 slider_min(track_x0 - 4.0f, center_y - 14.0f);
    ImVec2 slider_size(track_w + 8.0f, 28.0f);
    ImGui::SetCursorScreenPos(slider_min);
    ImGui::InvisibleButton("##volume_slider", slider_size);
    bool slider_hov = ImGui::IsItemHovered();
    bool slider_act = ImGui::IsItemActive();

    // 拖拽与点击实时计算音量
    if (slider_act && ImGui::IsMouseDown(0)) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float new_vol = std::clamp((mouse_x - track_x0) / track_w, 0.0f, 1.0f);
        player.setVolume(new_vol);
        if (is_muted && new_vol > 0.01f) {
            player.toggleMute();
        }
        vol = player.getVolume();
        is_muted = player.isMuted();
    }
    // 滚轮微调音量 (±5%)
    if (slider_hov && ImGui::GetIO().MouseWheel != 0.0f) {
        float new_vol = std::clamp(vol + ImGui::GetIO().MouseWheel * 0.05f, 0.0f, 1.0f);
        player.setVolume(new_vol);
        if (is_muted && new_vol > 0.01f) {
            player.toggleMute();
        }
        vol = player.getVolume();
        is_muted = player.isMuted();
    }

    // 轨道底槽 (暗色微光底板)
    dl->AddRectFilled(ImVec2(track_x0, track_y0), ImVec2(track_x1, track_y1), IM_COL32(255, 255, 255, 25), 2.0f);

    // 已填充有效音量条
    float fill_ratio = is_muted ? 0.0f : std::clamp(vol, 0.0f, 1.0f);
    float knob_x = track_x0 + track_w * fill_ratio;
    ImU32 fill_col = (slider_hov || slider_act) ? col_hover : col_blur;
    if (fill_ratio > 0.001f) {
        dl->AddRectFilled(ImVec2(track_x0, track_y0), ImVec2(knob_x, track_y1), fill_col, 2.0f);
    }

    // 滑块手柄 (Thumb 纯白发光圆点，悬停/拖动时微扩)
    float knob_r = (slider_hov || slider_act) ? 5.5f : 4.5f;
    dl->AddCircleFilled(ImVec2(knob_x, center_y), knob_r, IM_COL32(240, 240, 245, 255));
    dl->AddCircle(ImVec2(knob_x, center_y), knob_r, IM_COL32(0, 0, 0, 70), 0, 1.0f);

    // -------------------------------------------------------------------------
    // 4.3 音量数值百分比显示
    // -------------------------------------------------------------------------
    std::string text_str;
    if (is_muted) {
        text_str = "静音";
    } else {
        text_str = std::to_string(static_cast<int>(std::round(vol * 100.0f))) + "%";
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 text_size = ImGui::CalcTextSize(text_str.c_str());
    ImVec2 text_pos(track_x1 + gap2, center_y - text_size.y * 0.5f);
    dl->AddText(text_pos, is_muted ? col_hover : col_text, text_str.c_str());
    if (Fonts::Small) ImGui::PopFont();
}

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
    float center_x = (left_x + right_x) * 0.5f;

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

    // 左区：播放操作控制块
    renderLeftControls(dl, left_x + rounding + 4.0f, center_y);

    // 中区：LED 律动频谱仪 (截图红框位置)
    renderCenterIcon(dl, center_x, center_y);

    // 右区：发烧音量调节组件 (小喇叭 + 可拖拽滑块条 + 百分比)
    float right_limit = right_x - rounding - 4.0f;
    renderRightVolume(dl, right_limit, center_y);

    ImGui::End();
    ImGui::PopStyleVar();
}