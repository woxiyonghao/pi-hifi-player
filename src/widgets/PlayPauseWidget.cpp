#include "widgets/PlayPauseWidget.hpp"
#include "widgets/DrawUtils.hpp"
#include "public/UIConfig.hpp"

void PlayPauseWidget::drawIcon(ImDrawList* dl, ImVec2 center, bool is_playing, ImU32 color) {
    if (is_playing) {
        // [正在播放 -> 呈现暂停图标 ⏸]
        const float bar_w = 3.6f;
        const float bar_h = 15.0f;
        const float bar_gap = 4.0f;
        const float bar_round = bar_w * 0.5f;
        float half_w = bar_w + bar_gap * 0.5f;
        float half_h = bar_h * 0.5f;

        // 左胶囊竖条
        dl->AddRectFilled(ImVec2(center.x - half_w, center.y - half_h),
                          ImVec2(center.x - half_w + bar_w, center.y + half_h),
                          color, bar_round);
        // 右胶囊竖条
        dl->AddRectFilled(ImVec2(center.x + bar_gap * 0.5f, center.y - half_h),
                          ImVec2(center.x + half_w, center.y + half_h),
                          color, bar_round);
    } else {
        // [暂停/未播放 -> 呈现播放图标 ▶]：流线饱满右向大三角 (+0.6px 光学重心补偿)
        const float tri_h = 7.5f;
        const float tri_w = 12.4f;
        const float opt_x = 0.6f;
        const float r_play = 2.4f;

        ImVec2 p0(center.x - tri_w * 0.5f + opt_x, center.y - tri_h);
        ImVec2 p1(center.x + tri_w * 0.5f + opt_x, center.y);
        ImVec2 p2(center.x - tri_w * 0.5f + opt_x, center.y + tri_h);
        DrawRoundedTriangle(dl, p0, p1, p2, r_play, color);
    }
}

bool PlayPauseWidget::render(ImDrawList* dl, ImVec2 center, ImVec2 size, Callback on_click) {
    auto& player = PlayerAdmin::getInstance();
    // Blur 态：轻盈通透的灰白磨砂半透质感 (带有清晰 Alpha，不抢视觉重心)
    const ImU32 col_blur = IM_COL32(215, 222, 235, 175);
    // Hover 态：纯正主题色玫瑰红 (Alpha = 255)
    const ImU32 col_hover = UIConfig::Color::Accent;

    ImVec2 btn_min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    ImVec2 btn_max(center.x + size.x * 0.5f, center.y + size.y * 0.5f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_play_pause_widget", size);
    bool hov = ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(btn_min, btn_max);
    if (!clicked && hov && ImGui::IsMouseClicked(0)) {
        clicked = true;
    }

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.togglePlayPause();
        }
    }

    ImU32 icon_col = hov ? col_hover : col_blur;
    drawIcon(dl, center, player.isPlaying(), icon_col);
    return clicked;
}