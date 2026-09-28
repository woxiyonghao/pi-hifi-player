#include "widgets/LEDSpectrumWidget.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cmath>

void LEDSpectrumWidget::drawMatrix(ImDrawList* dl, ImVec2 center, float total_w, float total_h,
                                   int num_cols, int num_rows, ImU32 lit_color, ImU32 unlit_color,
                                   bool is_playing, const float* spectrum_levels) {
    const float gap_x = 1.2f;
    const float gap_y = 1.0f;
    const float col_w = (total_w - (num_cols - 1) * gap_x) / num_cols;
    const float seg_h = (total_h - (num_rows - 1) * gap_y) / num_rows;
    const float seg_round = 0.4f;

    const float start_x = center.x - total_w * 0.5f;
    const float bot_y   = center.y + total_h * 0.5f;

    for (int c = 0; c < num_cols; ++c) {
        float x0 = start_x + c * (col_w + gap_x);
        float x1 = x0 + col_w;

        // 计算当前列点亮的格数 (从底部往上计数)
        int active_count = 0;
        if (is_playing) {
            float level = (spectrum_levels != nullptr) ? spectrum_levels[c] : 0.0f;
            active_count = static_cast<int>(std::round(level * num_rows));
            active_count = std::clamp(active_count, 0, num_rows);
        } else {
            // 如果没有播放，显示 0 个方块
            active_count = 0;
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

    // 取消 blur/hover 切换，始终使用 hover 的样式高亮主题色 UIConfig::Color::Accent
    const ImU32 lit_col = UIConfig::Color::Accent;
    const ImU32 unlit_col = IM_COL32(255, 255, 255, 18); // 暗底未点亮段

    const int num_cols = 12;
    const int num_rows = 10;

    // 触控交互响应区 (纵向 36px 便于触摸点击)
    ImVec2 btn_min(center.x - width * 0.5f - 6.0f, center.y - 18.0f);
    ImVec2 btn_size(width + 12.0f, 36.0f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_center_spectrum_widget", btn_size);

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.togglePlayPause();
        }
    }

    // 根据实时音频信号获取 12 频段跳动幅度
    float spectrum_levels[num_cols] = {0.0f};
    if (player.isPlaying()) {
        player.getSpectrumLevels(spectrum_levels, num_cols);
    }

    drawMatrix(dl, center, width, height, num_cols, num_rows, lit_col, unlit_col, player.isPlaying(), spectrum_levels);
    return clicked;
}