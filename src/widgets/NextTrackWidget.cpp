#include "widgets/NextTrackWidget.hpp"
#include "widgets/DrawUtils.hpp"
#include "public/UIConfig.hpp"

void NextTrackWidget::drawIcon(ImDrawList* dl, ImVec2 center, ImU32 color) {
    const float tri_h = 5.0f;
    const float tri_w = 5.2f;
    const float gap   = 1.8f;
    const float r_tri = 1.3f;

    // 左边三角形 (▶ 尖端向右)
    ImVec2 l_p0(center.x - gap * 0.5f - tri_w, center.y - tri_h);
    ImVec2 l_p1(center.x - gap * 0.5f, center.y);
    ImVec2 l_p2(center.x - gap * 0.5f - tri_w, center.y + tri_h);
    DrawRoundedTriangle(dl, l_p0, l_p1, l_p2, r_tri, color);

    // 右边三角形 (▶ 尖端向右)
    ImVec2 r_p0(center.x + gap * 0.5f, center.y - tri_h);
    ImVec2 r_p1(center.x + gap * 0.5f + tri_w, center.y);
    ImVec2 r_p2(center.x + gap * 0.5f, center.y + tri_h);
    DrawRoundedTriangle(dl, r_p0, r_p1, r_p2, r_tri, color);
}

bool NextTrackWidget::render(ImDrawList* dl, ImVec2 center, ImVec2 size, Callback on_click) {
    auto& player = PlayerAdmin::getInstance();
    const ImU32 col_blur = IM_COL32(130, 127, 123, 255);
    const ImU32 col_hover = UIConfig::Color::Accent;

    ImVec2 btn_min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_next_track_widget", size);
    bool hov = ImGui::IsItemHovered();

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.next();
        }
    }

    ImU32 icon_col = hov ? col_hover : col_blur;
    drawIcon(dl, center, icon_col);
    return clicked;
}