#include "widgets/MusicItem.h"
#include "widgets/DrawUtils.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cmath>
#include <string>

std::string MusicItem::truncateTextWithEllipsis(const std::string& text, float max_width) {
    if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= max_width) {
        return text;
    }
    std::string result = text;
    while (!result.empty() && ImGui::CalcTextSize((result + "...").c_str()).x > max_width) {
        // 安全弹出 UTF-8 字符（先弹出后继字节 10xxxxxx，再弹出起始字节）
        while (!result.empty() && (static_cast<unsigned char>(result.back()) & 0xC0) == 0x80) {
            result.pop_back();
        }
        if (!result.empty()) {
            result.pop_back();
        }
    }
    return result + "...";
}

void MusicItem::drawVinylCdIcon(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, float rotation_rad) {
    const float thickness = std::max(1.8f, radius * 0.08f);
    const float r_in = radius * 0.28f;
    const float r_mid = radius * 0.65f;

    // 1. 外部主光盘轮廓圈 (Outer Disc Rim)
    dl->AddCircle(center, radius, col, 64, thickness);

    // 2. 内部主轴孔圈 (Center Spindle Ring)
    dl->AddCircle(center, r_in, col, 48, thickness);

    // 3. 动态光泽凹槽双圆弧 (Groove Sheen Arcs with Round Caps)
    // 弧段 1：顶部顺时针斜弧 (-90° ~ -36°)
    float a1_start = -1.5708f + rotation_rad;
    float a1_end   = -0.6283f + rotation_rad;
    dl->PathArcTo(center, r_mid, a1_start, a1_end, 24);
    dl->PathStroke(col, 0, thickness);

    ImVec2 c1_0(center.x + r_mid * std::cos(a1_start), center.y + r_mid * std::sin(a1_start));
    ImVec2 c1_1(center.x + r_mid * std::cos(a1_end), center.y + r_mid * std::sin(a1_end));
    dl->AddCircleFilled(c1_0, thickness * 0.5f, col, 16);
    dl->AddCircleFilled(c1_1, thickness * 0.5f, col, 16);

    // 弧段 2：底部对称顺时针斜弧 (+90° ~ +144°)
    float a2_start = 1.5708f + rotation_rad;
    float a2_end   = 2.5133f + rotation_rad;
    dl->PathArcTo(center, r_mid, a2_start, a2_end, 24);
    dl->PathStroke(col, 0, thickness);

    ImVec2 c2_0(center.x + r_mid * std::cos(a2_start), center.y + r_mid * std::sin(a2_start));
    ImVec2 c2_1(center.x + r_mid * std::cos(a2_end), center.y + r_mid * std::sin(a2_end));
    dl->AddCircleFilled(c2_0, thickness * 0.5f, col, 16);
    dl->AddCircleFilled(c2_1, thickness * 0.5f, col, 16);
}

bool MusicItem::render(ImDrawList* dl, ImVec2 size, const Track& track, const char* category_tag) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    return render(dl, pos, size, track, category_tag);
}

bool MusicItem::render(ImDrawList* dl, ImVec2 pos, ImVec2 size, const Track& track, const char* category_tag) {
    const float rounding = DefaultRounding;
    const float cover_w = size.x;
    ImVec2 cover_p0 = pos;
    ImVec2 cover_p1 = ImVec2(pos.x + cover_w, pos.y + cover_w);

    ImGui::SetCursorScreenPos(pos);
    std::string btn_id = "##music_item_" + std::to_string(track.id);
    bool is_clicked = ImGui::InvisibleButton(btn_id.c_str(), size);
    bool is_hovered = ImGui::IsItemHovered();

    // 1. 封面底板 (液态玻璃深色磨砂与 1px 微光边框)
    ImU32 cover_bg = is_hovered ? IM_COL32(36, 44, 58, 255) : IM_COL32(22, 26, 36, 255);
    dl->AddRectFilled(cover_p0, cover_p1, cover_bg, rounding);
    dl->AddRect(cover_p0, cover_p1, is_hovered ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 20),
                rounding, 0, 1.0f);

    ImVec2 cover_center((cover_p0.x + cover_p1.x) * 0.5f, (cover_p0.y + cover_p1.y) * 0.5f);
    ImVec2 cd_center(cover_center.x, cover_p0.y + 66.0f);

    // 状态持久化插值：鼠标悬停时平滑放大微动效 (60fps 弹簧动效，移开时平滑回缩)
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID anim_id = ImGui::GetID(("##anim_scale_" + std::to_string(track.id)).c_str());
    float hover_factor = storage->GetFloat(anim_id, 0.0f);
    float target_factor = is_hovered ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    hover_factor += (target_factor - hover_factor) * std::clamp(dt * 15.0f, 0.0f, 1.0f);
    if (std::abs(target_factor - hover_factor) < 0.005f) {
        hover_factor = target_factor;
    }
    storage->SetFloat(anim_id, hover_factor);

    // 2. 绘制唱片/CD 光盘主图标 (常态半径 40px，悬停时呼吸放大至 46px，伴随优雅旋转)
    const float cd_base_radius = 40.0f;
    const float cd_hover_expand = 6.0f;
    const float cd_radius = cd_base_radius + cd_hover_expand * hover_factor;

    float rot = (hover_factor > 0.01f) ? (static_cast<float>(ImGui::GetTime()) * 1.8f) : 0.0f;
    ImU32 cd_col = ImColor(
        185.0f + (250.0f - 185.0f) * hover_factor,
        200.0f + (45.0f - 200.0f) * hover_factor,
        220.0f + (72.0f - 220.0f) * hover_factor,
        160.0f + 30.0f * hover_factor
    );
    drawVinylCdIcon(dl, cd_center, cd_radius, cd_col, rot);

    // 3. 发烧格式指示胶囊徽标 (Format Badge Capsule，悬停时亦协同平滑放大)
    std::string badge = track.getFormatBadge();
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 b_sz = ImGui::CalcTextSize(badge.c_str());
    float badge_w = b_sz.x + 16.0f + 2.0f * hover_factor;
    float badge_h = 20.0f + 1.0f * hover_factor;
    ImVec2 b_p0(cover_center.x - badge_w * 0.5f, cover_p1.y - 28.0f);
    ImVec2 b_p1(cover_center.x + badge_w * 0.5f, b_p0.y + badge_h);

    dl->AddRectFilled(b_p0, b_p1, IM_COL32(12, 16, 24, 210), 10.0f);
    dl->AddRect(b_p0, b_p1, is_hovered ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25), 10.0f, 0, 1.0f);
    dl->AddText(ImVec2(b_p0.x + (badge_w - b_sz.x) * 0.5f, b_p0.y + (badge_h - b_sz.y) * 0.5f),
                is_hovered ? UIConfig::Color::TextActive : IM_COL32(175, 190, 210, 210), badge.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 4. 悬停态：在唱片中心浮现超圆滑 Apple 质感主题色 ▶ 播放按钮 (伴随透明度与尺寸浮现)
    if (hover_factor > 0.02f) {
        ImVec2 play_center = cd_center;
        const float btn_radius = 21.0f + 2.5f * hover_factor;
        int alpha = std::clamp(static_cast<int>(255.0f * hover_factor), 0, 255);

        // 柔和暗影与外发光
        int shadow_alpha = std::clamp(static_cast<int>(95.0f * hover_factor), 0, 255);
        int glow_alpha = std::clamp(static_cast<int>(75.0f * hover_factor), 0, 255);
        dl->AddCircleFilled(ImVec2(play_center.x, play_center.y + 2.0f), btn_radius, IM_COL32(0, 0, 0, shadow_alpha), 48);
        dl->AddCircle(play_center, btn_radius + 1.5f, IM_COL32(250, 45, 72, glow_alpha), 48, 2.0f);

        // 主体高饱圆环 (48 细分平滑无棱角)
        dl->AddCircleFilled(play_center, btn_radius, IM_COL32(250, 45, 72, alpha), 48);

        // 圆润播放三角形 (流线饱满圆滑倒角 + 0.8px 光学居中补偿)
        const float scale_tri = 0.85f + 0.15f * hover_factor;
        const float tri_h = 7.5f * scale_tri;
        const float tri_w = 12.0f * scale_tri;
        const float opt_x = 0.8f;
        const float r_play = 2.6f;

        ImVec2 tri_p0(play_center.x - tri_w * 0.5f + opt_x, play_center.y - tri_h);
        ImVec2 tri_p1(play_center.x + tri_w * 0.5f + opt_x, play_center.y);
        ImVec2 tri_p2(play_center.x - tri_w * 0.5f + opt_x, play_center.y + tri_h);
        DrawRoundedTriangle(dl, tri_p0, tri_p1, tri_p2, r_play, IM_COL32(255, 255, 255, alpha));
    }

    // 5. 文字排版区域
    float text_y = cover_p1.y + 10.0f;

    // Line 1 (可选): 分类小标签 (如 "Studio Master"、"流行热播")
    if (category_tag && category_tag[0] != '\0') {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(pos.x, text_y), UIConfig::Color::TextMuted, category_tag);
        text_y += 16.0f;
        if (Fonts::Small) ImGui::PopFont();
    }

    // Line 2: 歌曲主标题 (悬停高亮主题色，支持超长自动省略号)
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImU32 title_col = is_hovered ? UIConfig::Color::Accent : UIConfig::Color::TextActive;
    std::string title_display = truncateTextWithEllipsis(track.title, cover_w);
    dl->AddText(ImVec2(pos.x, text_y), title_col, title_display.c_str());
    text_y += 20.0f;
    if (Fonts::Regular) ImGui::PopFont();

    // Line 3: 歌手与艺术家信息
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string artist_display = truncateTextWithEllipsis(track.artist, cover_w);
    dl->AddText(ImVec2(pos.x, text_y), UIConfig::Color::TextMuted, artist_display.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 触控拖拽防误触过滤：若鼠标处于滑动或拖拽位移中，不视为有效点击
    bool was_dragged = (std::abs(ImGui::GetMouseDragDelta(0).x) > 4.0f || std::abs(ImGui::GetMouseDragDelta(0).y) > 4.0f);
    return is_clicked && !was_dragged;
}

