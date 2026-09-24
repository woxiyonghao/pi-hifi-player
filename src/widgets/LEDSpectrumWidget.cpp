#include "widgets/LEDSpectrumWidget.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cmath>

void LEDSpectrumWidget::drawMatrix(ImDrawList* dl, ImVec2 center, float total_w, float total_h,
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

    // 经典 EQ 频段静态高度分布模板 (低频至高频的自然曲线，未播放时呈现精美静态均衡器造型)
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
            // 非播放状态：严格静止不动 (0 动画，完全脱离时间 t，呈现固定静止轮廓)
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

bool LEDSpectrumWidget::render(ImDrawList* dl, ImVec2 center, float width, float height, Callback on_click) {
    auto& player = PlayerAdmin::getInstance();
    const ImU32 col_blur = IM_COL32(130, 127, 123, 255); // 播放控制同款中性暖灰
    const ImU32 col_hover = UIConfig::Color::Accent;     // 悬停玫红高亮
    const ImU32 unlit_col = IM_COL32(255, 255, 255, 18); // 暗底未点亮段

    const int num_cols = 12;
    const int num_rows = 10;

    // 触控交互响应区 (纵向 36px 便于触摸点击)
    ImVec2 btn_min(center.x - width * 0.5f - 6.0f, center.y - 18.0f);
    ImVec2 btn_size(width + 12.0f, 36.0f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_center_spectrum_widget", btn_size);
    bool hov = ImGui::IsItemHovered();

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.togglePlayPause();
        }
    }

    ImU32 lit_col = hov ? col_hover : col_blur;
    drawMatrix(dl, center, width, height, num_cols, num_rows, lit_col, unlit_col, player.isPlaying());
    return clicked;
}