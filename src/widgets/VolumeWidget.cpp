#include "widgets/VolumeWidget.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cmath>
#include <string>

void VolumeWidget::drawSpeaker(ImDrawList* dl, ImVec2 center, float vol, bool is_muted, ImU32 color) {
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

void VolumeWidget::render(ImDrawList* dl, float right_limit, float center_y) {
    auto& player = PlayerAdmin::getInstance();
    float vol = player.getVolume();
    bool is_muted = player.isMuted();

    // Blur 态：轻盈通透的灰白磨砂半透质感 (带有清晰 Alpha，不抢视觉重心)
    const ImU32 col_blur = IM_COL32(215, 222, 235, 175);
    // Hover 态：纯正主题色玫瑰红 (Alpha = 255)
    const ImU32 col_hover = UIConfig::Color::Accent;
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
    // 1. 小喇叭按钮 (点击静音/解静音)
    // -------------------------------------------------------------------------
    float spk_cx = start_x + spk_w * 0.5f;
    ImVec2 spk_min(start_x, center_y - 14.0f);
    ImVec2 spk_max(start_x + spk_w, center_y + 14.0f);
    ImGui::SetCursorScreenPos(spk_min);
    bool clicked_spk = ImGui::InvisibleButton("##btn_spk_mute_widget", ImVec2(spk_w, 28.0f));
    bool spk_hov = ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(spk_min, spk_max);
    if (!clicked_spk && spk_hov && ImGui::IsMouseClicked(0)) {
        clicked_spk = true;
    }
    if (clicked_spk) {
        player.toggleMute();
        vol = player.getVolume();
        is_muted = player.isMuted();
    }

    ImU32 spk_col = (spk_hov || is_muted) ? col_hover : col_blur;
    drawSpeaker(dl, ImVec2(spk_cx, center_y), vol, is_muted, spk_col);

    // -------------------------------------------------------------------------
    // 2. 音量拖拽滑块条 (支持点击、连续拖拽、滚轮微调)
    // -------------------------------------------------------------------------
    float track_x0 = start_x + spk_w + gap1;
    float track_x1 = track_x0 + track_w;
    float track_y0 = center_y - track_h * 0.5f;
    float track_y1 = center_y + track_h * 0.5f;

    // 交互响应区 (纵向扩大至 28px，手指触控与鼠标皆舒适)
    ImVec2 slider_min(track_x0 - 4.0f, center_y - 14.0f);
    ImVec2 slider_max(track_x1 + 4.0f, center_y + 14.0f);
    ImVec2 slider_size(track_w + 8.0f, 28.0f);
    ImGui::SetCursorScreenPos(slider_min);
    ImGui::InvisibleButton("##volume_slider_widget", slider_size);
    bool slider_hov = ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(slider_min, slider_max);
    bool slider_act = ImGui::IsItemActive();

    // 拖拽与点击实时计算音量
    if ((slider_act || (slider_hov && ImGui::IsMouseClicked(0))) && ImGui::IsMouseDown(0)) {
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

    // 滑块手柄 (Thumb 纯白实心圆点)
    float knob_r = (slider_hov || slider_act) ? 5.5f : 4.5f;
    dl->AddCircleFilled(ImVec2(knob_x, center_y), knob_r, IM_COL32(245, 245, 250, 255));
    dl->AddCircle(ImVec2(knob_x, center_y), knob_r, IM_COL32(0, 0, 0, 70), 0, 1.0f);

    // -------------------------------------------------------------------------
    // 3. 音量数值百分比显示
    // -------------------------------------------------------------------------
    std::string text_str;
    if (is_muted) {
        text_str = "静音";
    } else {
        text_str = std::to_string(static_cast<int>(std::round(vol * 100.0f))) + "%";
    }

    if (Fonts::Small) {
        ImGui::PushFont(Fonts::Small);
    }
    ImVec2 text_size = ImGui::CalcTextSize(text_str.c_str());
    ImVec2 text_pos(track_x1 + gap2, center_y - text_size.y * 0.5f);
    dl->AddText(text_pos, (is_muted || slider_hov || slider_act) ? col_hover : col_text, text_str.c_str());
    if (Fonts::Small) {
        ImGui::PopFont();
    }
}