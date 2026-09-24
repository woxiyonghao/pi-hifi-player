
#include "widgets/PrevTrackWidget.hpp"
#include "widgets/DrawUtils.hpp"
#include "public/UIConfig.hpp"

void PrevTrackWidget::drawIcon(ImDrawList* dl, ImVec2 center, ImU32 color) {
    const float tri_h = 5.0f;  // 半高
    const float tri_w = 5.2f;  // 三角形横向冲程
    const float gap   = 1.8f;  // 两个三角形间隙
    const float r_tri = 1.3f;  // 倒角圆润半径

    // 左边三角形 (◀ 尖端向左)
    ImVec2 l_p0(center.x - gap * 0.5f, center.y + tri_h);
    ImVec2 l_p1(center.x - gap * 0.5f - tri_w, center.y);
    ImVec2 l_p2(center.x - gap * 0.5f, center.y - tri_h);
    DrawRoundedTriangle(dl, l_p0, l_p1, l_p2, r_tri, color);

    // 右边三角形 (◀ 尖端向左)
    ImVec2 r_p0(center.x + gap * 0.5f + tri_w, center.y + tri_h);
    ImVec2 r_p1(center.x + gap * 0.5f, center.y);
    ImVec2 r_p2(center.x + gap * 0.5f + tri_w, center.y - tri_h);
    DrawRoundedTriangle(dl, r_p0, r_p1, r_p2, r_tri, color);
}

bool PrevTrackWidget::render(ImDrawList* dl, ImVec2 center, ImVec2 size, Callback on_click) {
    auto& player = PlayerAdmin::getInstance();
    // Blur 态：轻盈通透的灰白磨砂半透质感 (带有清晰 Alpha，不抢视觉重心)
    const ImU32 col_blur = IM_COL32(215, 222, 235, 175);
    // Hover 态：纯正主题色玫瑰红 (Alpha = 255)
    const ImU32 col_hover = UIConfig::Color::Accent;

    ImVec2 btn_min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    ImVec2 btn_max(center.x + size.x * 0.5f, center.y + size.y * 0.5f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_prev_track_widget", size);
    bool hov = ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(btn_min, btn_max);
    if (!clicked && hov && ImGui::IsMouseClicked(0)) {
        clicked = true;
    }

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.previous();
        }
    }

    // Hover 态：液态玻璃微光底板 + 1px 折射微光边框 + 主题色高亮
    if (hov) {
        dl->AddRectFilled(btn_min, btn_max, UIConfig::Color::GlassHover, 8.0f);
        dl->AddRect(btn_min, btn_max, UIConfig::Color::GlassBorder, 8.0f, 0, 1.0f);
        // 环境微发光
        dl->AddCircleFilled(center, 13.0f, IM_COL32(250, 45, 72, 35));
    }

    ImU32 icon_col = hov ? col_hover : col_blur;
    drawIcon(dl, center, icon_col);
    return clicked;
}