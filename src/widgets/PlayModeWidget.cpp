#include "widgets/PlayModeWidget.hpp"
#include "widgets/DrawUtils.hpp"
#include "public/UIConfig.hpp"

void PlayModeWidget::drawIcon(ImDrawList* dl, ImVec2 center, PlayMode mode, ImU32 color, float scale) {
    const float th = 1.6f * scale;

    auto P = [&](float dx, float dy) -> ImVec2 {
        return ImVec2(center.x + dx * scale, center.y + dy * scale);
    };

    switch (mode) {
        case PlayMode::LoopList: {
            // [列表循环]：圆润 U 型贝塞尔回环 + 箭头
            dl->AddBezierCubic(P(-6.0f, 1.0f), P(-6.0f, -4.5f), P(-2.5f, -4.5f), P(3.0f, -4.5f), color, th);
            dl->AddCircleFilled(P(-6.0f, 1.0f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(2.5f, -8.0f), P(7.5f, -4.5f), P(2.5f, -1.0f), 0.9f * scale, color);

            dl->AddBezierCubic(P(6.0f, -1.0f), P(6.0f, 4.5f), P(2.5f, 4.5f), P(-3.0f, 4.5f), color, th);
            dl->AddCircleFilled(P(6.0f, -1.0f), th * 0.5f, color);
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

            dl->AddLine(P(0.0f, -3.0f), P(0.0f, 3.0f), color, th);
            dl->AddCircleFilled(P(0.0f, -3.0f), th * 0.5f, color);
            dl->AddCircleFilled(P(0.0f, 3.0f), th * 0.5f, color);
            dl->AddLine(P(-1.6f, -1.2f), P(0.0f, -3.0f), color, th * 0.9f);
            break;
        }
        case PlayMode::Shuffle: {
            // [随机播放]：S 型平滑流线
            dl->AddBezierCubic(P(-7.0f, -4.5f), P(-2.0f, -4.5f), P(0.5f, 4.5f), P(4.5f, 4.5f), color, th);
            dl->AddCircleFilled(P(-7.0f, -4.5f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(3.5f, 1.5f), P(8.5f, 4.5f), P(3.5f, 7.5f), 0.9f * scale, color);

            dl->AddBezierCubic(P(-7.0f, 4.5f), P(-2.0f, 4.5f), P(0.5f, -4.5f), P(4.5f, -4.5f), color, th);
            dl->AddCircleFilled(P(-7.0f, 4.5f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(3.5f, -7.5f), P(8.5f, -4.5f), P(3.5f, -1.5f), 0.9f * scale, color);
            break;
        }
        case PlayMode::Sequence: {
            // [顺序播放]：前进箭头 + 终止挡板
            dl->AddLine(P(-7.0f, 0.0f), P(3.0f, 0.0f), color, th);
            dl->AddCircleFilled(P(-7.0f, 0.0f), th * 0.5f, color);
            DrawRoundedTriangle(dl, P(2.0f, -4.0f), P(6.5f, 0.0f), P(2.0f, 4.0f), 0.9f * scale, color);
            dl->AddRectFilled(P(6.8f, -4.5f), P(8.2f, 4.5f), color, 0.7f * scale);
            break;
        }
    }
}

bool PlayModeWidget::render(ImDrawList* dl, ImVec2 center, ImVec2 size, Callback on_click) {
    auto& player = PlayerAdmin::getInstance();
    // Blur 态：轻盈通透的灰白磨砂半透质感 (带有清晰 Alpha，不抢视觉重心)
    const ImU32 col_blur = IM_COL32(215, 222, 235, 175);
    // Hover 态：纯正主题色玫瑰红 (Alpha = 255)
    const ImU32 col_hover = UIConfig::Color::Accent;

    ImVec2 btn_min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    ImVec2 btn_max(center.x + size.x * 0.5f, center.y + size.y * 0.5f);
    ImGui::SetCursorScreenPos(btn_min);

    bool clicked = ImGui::InvisibleButton("##btn_play_mode_widget", size);
    bool hov = ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(btn_min, btn_max);
    if (!clicked && hov && ImGui::IsMouseClicked(0)) {
        clicked = true;
    }

    if (clicked) {
        if (on_click) {
            on_click();
        } else {
            player.cyclePlayMode();
        }
    }

    ImU32 dynamic_col = hov ? col_hover : col_blur;
    drawIcon(dl, center, player.getPlayMode(), dynamic_col, 1.25f);
    return clicked;
}