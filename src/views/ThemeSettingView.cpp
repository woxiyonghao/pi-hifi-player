#include "views/ThemeSettingView.hpp"
#include "themes/ThemeManager.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

void ThemeSettingView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 绘制主舞台大底板卡片
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    auto& tm = ThemeManager::getInstance();
    ThemeId cur_theme = tm.getCurrentTheme();

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // ==============================================================================
    // 1. 顶部 Header 栏：左侧主标题「主题」(无 subtitle)，右侧「恢复名机预设」按钮
    // ==============================================================================
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "主题");
    if (Fonts::Medium) ImGui::PopFont();

    // 右上角：「恢复名机预设」按钮
    float rst_w = 110.0f;
    float rst_h = 28.0f;
    float rst_x = card_max.x - 20.0f - rst_w;
    float rst_y = card_min.y + 15.0f;

    ImGui::SetCursorScreenPos(ImVec2(rst_x, rst_y));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 48, 64, 180));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(55, 66, 88, 220));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70, 84, 110, 250));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextNormal);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    if (ImGui::Button("恢复名机预设", ImVec2(rst_w, rst_h))) {
        tm.setTheme(ThemeId::ModernCrimson);
        tm.setBackgroundVisualMode(BackgroundVisualMode::LEDSpectrum);
    }

    if (Fonts::Small) ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // 辅助毛玻璃板块卡片绘制闭包
    auto drawSectionFrostedCard = [&](ImVec2 p0, ImVec2 p1, float rounding = 10.0f) {
        // 1. 深空磨砂底板
        dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), rounding);
        // 2. 漫射高光层
        dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 8), rounding);
        // 3. 顶部微光棱线
        dl->AddLine(ImVec2(p0.x + rounding, p0.y + 0.5f), ImVec2(p1.x - rounding, p0.y + 0.5f),
                    IM_COL32(255, 255, 255, 45), 1.0f);
        // 4. 1px 微光边框
        dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 28), rounding, 0, 1.0f);
    };

    float sec_x0 = card_min.x + 20.0f;
    float sec_x1 = card_max.x - 20.0f;
    float sec_w = sec_x1 - sec_x0;

    // ==============================================================================
    // 2. 板块一：预设 (标题改为「预设」，无 subtitle，带独立毛玻璃容器与 Options)
    // ==============================================================================
    float sec1_y0 = card_min.y + 52.0f;
    float sec1_h = 154.0f;
    float sec1_y1 = sec1_y0 + sec1_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec1_y0), ImVec2(sec_x1, sec1_y1));

    // [Header] 仅标题「预设」，去除 subtitle
    float s1_head_y = sec1_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s1_head_y), UIConfig::Color::TextActive, "预设");
    if (Fonts::Regular) ImGui::PopFont();

    // [Options] 4 张名机卡片 (2 行 × 2 列)
    const auto& presets = tm.getAllPresets();
    float c_inner_w = sec_w - 32.0f;
    float col_gap = 12.0f;
    float c_w = (c_inner_w - col_gap) * 0.5f;
    float c_h = 50.0f;
    float row_gap = 8.0f;

    for (size_t i = 0; i < presets.size() && i < 4; ++i) {
        const auto& p = presets[i];
        int row = static_cast<int>(i / 2);
        int col = static_cast<int>(i % 2);

        float cx0 = sec_x0 + 16.0f + col * (c_w + col_gap);
        float cy0 = sec1_y0 + 36.0f + row * (c_h + row_gap);
        float cx1 = cx0 + c_w;
        float cy1 = cy0 + c_h;

        ImVec2 c_min(cx0, cy0);
        ImVec2 c_max(cx1, cy1);

        bool is_current = (cur_theme == p.id);

        std::string btn_id = "##ThemeCard_" + std::to_string(static_cast<int>(p.id));
        ImGui::SetCursorScreenPos(c_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(c_w, c_h));

        bool is_hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setTheme(p.id);
        }

        const ImU32 pr = (p.accent_color >> IM_COL32_R_SHIFT) & 0xFF;
        const ImU32 pg = (p.accent_color >> IM_COL32_G_SHIFT) & 0xFF;
        const ImU32 pb = (p.accent_color >> IM_COL32_B_SHIFT) & 0xFF;

        ImU32 bg_col = is_current ? IM_COL32(pr, pg, pb, 35) :
                       (is_hovered ? IM_COL32(255, 255, 255, 18) : IM_COL32(255, 255, 255, 8));
        dl->AddRectFilled(c_min, c_max, bg_col, 8.0f);

        ImU32 border_col = is_current ? p.accent_color :
                           (is_hovered ? IM_COL32(pr, pg, pb, 160) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(c_min, c_max, border_col, 8.0f, 0, is_current ? 1.6f : 1.0f);

        // 主题色标点
        float dot_cx = cx0 + 16.0f;
        float dot_cy = cy0 + c_h * 0.5f;
        dl->AddCircleFilled(ImVec2(dot_cx, dot_cy), 6.5f, p.accent_color);
        if (is_current) {
            dl->AddCircle(ImVec2(dot_cx, dot_cy), 10.5f, p.accent_color, 24, 1.4f);
        }

        // 主题名称与风格
        float label_x = dot_cx + 16.0f;
        if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
        dl->AddText(ImVec2(label_x, cy0 + 8.0f), is_current ? UIConfig::Color::TextActive : IM_COL32(220, 230, 245, 230), p.name.c_str());
        if (Fonts::Regular) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(label_x, cy0 + 28.0f), UIConfig::Color::TextMuted, p.sound_style.c_str());

        // 右侧色块与状态标签
        float chip_w = 13.0f;
        float chip_h = 18.0f;
        float chip_r = 3.0f;
        float chip_y = cy0 + (c_h - chip_h) * 0.5f;
        float right_pos = cx1 - 12.0f;

        if (is_current) {
            const char* act_tag = "已激活";
            float act_w = ImGui::CalcTextSize(act_tag).x + 10.0f;
            float act_x = right_pos - act_w;
            float act_y = cy0 + (c_h - 18.0f) * 0.5f;

            dl->AddRectFilled(ImVec2(act_x, act_y), ImVec2(act_x + act_w, act_y + 18.0f), IM_COL32(pr, pg, pb, 60), 4.0f);
            dl->AddRect(ImVec2(act_x, act_y), ImVec2(act_x + act_w, act_y + 18.0f), p.accent_color, 4.0f, 0, 1.0f);
            dl->AddText(ImVec2(act_x + 5.0f, act_y + 1.0f), UIConfig::Color::TextActive, act_tag);
            right_pos = act_x - 8.0f;
        } else if (is_hovered) {
            const char* hov_tag = "启用";
            float hov_w = ImGui::CalcTextSize(hov_tag).x + 10.0f;
            float hov_x = right_pos - hov_w;
            float hov_y = cy0 + (c_h - 18.0f) * 0.5f;

            dl->AddRectFilled(ImVec2(hov_x, hov_y), ImVec2(hov_x + hov_w, hov_y + 18.0f), IM_COL32(255, 255, 255, 22), 4.0f);
            dl->AddRect(ImVec2(hov_x, hov_y), ImVec2(hov_x + hov_w, hov_y + 18.0f), IM_COL32(255, 255, 255, 60), 4.0f, 0, 1.0f);
            dl->AddText(ImVec2(hov_x + 5.0f, hov_y + 1.0f), UIConfig::Color::TextActive, hov_tag);
            right_pos = hov_x - 8.0f;
        }

        // 3 颗调色代表色条
        float chip3_x = right_pos - chip_w;
        dl->AddRectFilled(ImVec2(chip3_x, chip_y), ImVec2(chip3_x + chip_w, chip_y + chip_h), p.peak_color, chip_r);
        dl->AddRect(ImVec2(chip3_x, chip_y), ImVec2(chip3_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        float chip2_x = chip3_x - chip_w - 4.0f;
        dl->AddRectFilled(ImVec2(chip2_x, chip_y), ImVec2(chip2_x + chip_w, chip_y + chip_h), p.lit_color, chip_r);
        dl->AddRect(ImVec2(chip2_x, chip_y), ImVec2(chip2_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        float chip1_x = chip2_x - chip_w - 4.0f;
        dl->AddRectFilled(ImVec2(chip1_x, chip_y), ImVec2(chip1_x + chip_w, chip_y + chip_h), p.accent_color, chip_r);
        dl->AddRect(ImVec2(chip1_x, chip_y), ImVec2(chip1_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        if (Fonts::Small) ImGui::PopFont();
    }

    // ==============================================================================
    // 3. 板块二：背景律动 (标题改为「背景律动」，无 subtitle，带独立毛玻璃容器与 Options)
    // ==============================================================================
    float sec2_y0 = sec1_y1 + 8.0f;
    float sec2_h = 72.0f;
    float sec2_y1 = sec2_y0 + sec2_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec2_y0), ImVec2(sec_x1, sec2_y1));

    // [Header] 仅标题「背景律动」，去除 subtitle
    float s2_head_y = sec2_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s2_head_y), UIConfig::Color::TextActive, "背景律动");
    if (Fonts::Regular) ImGui::PopFont();

    // [Options] 4 个动效风格按钮 (含金嗓子功放液晶双表头显示)
    auto cur_bg_mode = tm.getBackgroundVisualMode();
    const char* mode_labels[4] = {
        "48列全景 LED 频谱",
        "麦景图动圈大表头",
        "金嗓子动圈大表头",
        "极简纯黑发烧机架"
    };

    float mode_btn_gap = 10.0f;
    float mode_btn_w = (c_inner_w - mode_btn_gap * 3.0f) / 4.0f;
    float mode_btn_h = 28.0f;
    float mode_btn_y = sec2_y0 + 34.0f;

    for (int m = 0; m < 4; ++m) {
        float mx0 = sec_x0 + 16.0f + m * (mode_btn_w + mode_btn_gap);
        ImVec2 m_min(mx0, mode_btn_y);
        ImVec2 m_max(mx0 + mode_btn_w, mode_btn_y + mode_btn_h);

        bool is_mode_act = (static_cast<int>(cur_bg_mode) == m);
        std::string m_id = "##BgModeBtn_" + std::to_string(m);
        ImGui::SetCursorScreenPos(m_min);
        ImGui::InvisibleButton(m_id.c_str(), ImVec2(mode_btn_w, mode_btn_h));

        bool m_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setBackgroundVisualMode(static_cast<BackgroundVisualMode>(m));
        }

        ImU32 m_bg = is_mode_act ? IM_COL32(r, g, b, 70) :
                     (m_hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(m_min, m_max, m_bg, 7.0f);

        ImU32 m_border = is_mode_act ? accent :
                         (m_hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(m_min, m_max, m_border, 7.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(mode_labels[m]);
        float txt_x = mx0 + (mode_btn_w - txt_sz.x) * 0.5f;
        float txt_y = mode_btn_y + (mode_btn_h - txt_sz.y) * 0.5f;
        dl->AddText(ImVec2(txt_x, txt_y), is_mode_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, mode_labels[m]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // ==============================================================================
    // 4. 板块三：自由调色 (标题改为「自由调色」，无 subtitle，对齐 macOS 风格色轮与调色台)
    // ==============================================================================
    float sec3_y0 = sec2_y1 + 8.0f;
    float sec3_h = 186.0f;
    float sec3_y1 = sec3_y0 + sec3_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec3_y0), ImVec2(sec_x1, sec3_y1));

    // [Header] 仅标题「自由调色」，去除 subtitle
    float s3_head_y = sec3_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s3_head_y), UIConfig::Color::TextActive, "自由调色");
    if (Fonts::Regular) ImGui::PopFont();

    // Header 右侧当前色彩 HEX 与 RGB 实时信息药丸
    char hex_buf[32];
    std::snprintf(hex_buf, sizeof(hex_buf), "#%02X%02X%02X  ·  RGB(%u, %u, %u)", r, g, b, r, g, b);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    float hex_w = ImGui::CalcTextSize(hex_buf).x + 20.0f;
    float hex_h = 22.0f;
    float hex_x = sec_x1 - 16.0f - hex_w;
    float hex_y = s3_head_y - 2.0f;

    dl->AddRectFilled(ImVec2(hex_x, hex_y), ImVec2(hex_x + hex_w, hex_y + hex_h), IM_COL32(r, g, b, 45), 11.0f);
    dl->AddRect(ImVec2(hex_x, hex_y), ImVec2(hex_x + hex_w, hex_y + hex_h), IM_COL32(r, g, b, 140), 11.0f, 0, 1.0f);
    dl->AddText(ImVec2(hex_x + 10.0f, hex_y + 3.5f), UIConfig::Color::TextActive, hex_buf);
    if (Fonts::Small) ImGui::PopFont();

    // --------------------------------------------------------------------------
    // [Options] 左侧：Apple/macOS 风格全彩渐变色轮 (Hue-Saturation Color Wheel)
    // --------------------------------------------------------------------------
    ImVec4 custom_v4 = tm.getCustomColor();
    float cur_r = custom_v4.x;
    float cur_g = custom_v4.y;
    float cur_b = custom_v4.z;

    float cur_h = 0.0f, cur_s = 0.0f, cur_v = 1.0f;
    ImGui::ColorConvertRGBtoHSV(cur_r, cur_g, cur_b, cur_h, cur_s, cur_v);

    float wheel_radius = 65.0f;
    ImVec2 wheel_center(sec_x0 + 16.0f + wheel_radius + 4.0f, sec3_y0 + 38.0f + wheel_radius);

    // 1. GPU Triangle Fan 绘制平滑连续真色彩轮 (圆心为灰白，圆周为饱和纯色)
    const int num_wheel_segments = 64;
    dl->PrimReserve(num_wheel_segments * 3, num_wheel_segments + 1);

    ImDrawIdx center_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    // 中心顶点色：饱和度为 0 的明度对应基色
    float cr0, cg0, cb0;
    ImGui::ColorConvertHSVtoRGB(0.0f, 0.0f, cur_v, cr0, cg0, cb0);
    dl->PrimWriteVtx(wheel_center, uv, IM_COL32(static_cast<int>(cr0 * 255.0f), static_cast<int>(cg0 * 255.0f), static_cast<int>(cb0 * 255.0f), 255));

    constexpr float kPi = 3.14159265358979323846f;
    for (int s = 0; s < num_wheel_segments; ++s) {
        float a = (static_cast<float>(s) / static_cast<float>(num_wheel_segments)) * 2.0f * kPi;
        float h_edge = static_cast<float>(s) / static_cast<float>(num_wheel_segments);
        float er, eg, eb;
        ImGui::ColorConvertHSVtoRGB(h_edge, 1.0f, cur_v, er, eg, eb);
        ImVec2 p_edge(wheel_center.x + std::cos(a) * wheel_radius, wheel_center.y + std::sin(a) * wheel_radius);
        dl->PrimWriteVtx(p_edge, uv, IM_COL32(static_cast<int>(er * 255.0f), static_cast<int>(eg * 255.0f), static_cast<int>(eb * 255.0f), 255));
    }

    for (int s = 0; s < num_wheel_segments; ++s) {
        dl->PrimWriteIdx(center_idx);
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + s));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + ((s + 1) % num_wheel_segments)));
    }

    // 色轮精工微光外圈
    dl->AddCircle(wheel_center, wheel_radius, IM_COL32(255, 255, 255, 60), num_wheel_segments, 1.2f);

    // 2. 色轮交互：点击/拖拽动态拾取 Hue 与 Saturation
    ImGui::SetCursorScreenPos(ImVec2(wheel_center.x - wheel_radius, wheel_center.y - wheel_radius));
    ImGui::InvisibleButton("##MacColorWheelArea", ImVec2(wheel_radius * 2.0f, wheel_radius * 2.0f));

    if (ImGui::IsItemActive()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        float dx = m.x - wheel_center.x;
        float dy = m.y - wheel_center.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float new_s = std::clamp(dist / wheel_radius, 0.0f, 1.0f);
        float angle = std::atan2(dy, dx);
        float new_h = std::fmod((angle / (2.0f * kPi) + 1.0f), 1.0f);

        float new_r, new_g, new_b;
        ImGui::ColorConvertHSVtoRGB(new_h, new_s, cur_v, new_r, new_g, new_b);
        tm.setCustomColor(ImVec4(new_r, new_g, new_b, 1.0f));
    }

    // 3. 绘制类似图二的取色准星 (Crosshair Reticle)
    float ret_angle = cur_h * 2.0f * kPi;
    float ret_dist = cur_s * wheel_radius;
    ImVec2 ret_pos(wheel_center.x + std::cos(ret_angle) * ret_dist,
                   wheel_center.y + std::sin(ret_angle) * ret_dist);

    dl->AddCircle(ret_pos, 7.0f, IM_COL32(0, 0, 0, 220), 16, 2.2f);
    dl->AddCircle(ret_pos, 6.0f, IM_COL32(255, 255, 255, 255), 16, 1.5f);
    dl->AddCircleFilled(ret_pos, 3.0f, IM_COL32(r, g, b, 255), 16);

    // --------------------------------------------------------------------------
    // [Options] 右侧调色中枢：明度滑条 + 经典色彩数值栏 + 经典发烧色格矩阵 (对齐图二)
    // --------------------------------------------------------------------------
    float right_panel_x = wheel_center.x + wheel_radius + 24.0f;
    float right_panel_w = sec_x1 - 16.0f - right_panel_x;

    // 1. 横向明度渐变滑条 (Value Slider - 类似图二色轮下方滑块)
    float slider_y = sec3_y0 + 38.0f;
    float slider_h = 16.0f;
    ImVec2 slider_min(right_panel_x, slider_y + 16.0f);
    ImVec2 slider_max(right_panel_x + right_panel_w, slider_y + 16.0f + slider_h);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(right_panel_x, slider_y), UIConfig::Color::TextMuted, "明度 / 亮度 (Brightness)");
    char v_percent_buf[16];
    std::snprintf(v_percent_buf, sizeof(v_percent_buf), "%d%%", static_cast<int>(std::round(cur_v * 100.0f)));
    float v_txt_w = ImGui::CalcTextSize(v_percent_buf).x;
    dl->AddText(ImVec2(slider_max.x - v_txt_w, slider_y), UIConfig::Color::TextActive, v_percent_buf);
    if (Fonts::Small) ImGui::PopFont();

    // 绘制明度渐变条 (从纯黑到当前 Hue/Sat 的最大饱和纯色，采用 Triangle Strip 完美全圆角胶囊光栅化)
    float max_r, max_g, max_b;
    ImGui::ColorConvertHSVtoRGB(cur_h, cur_s, 1.0f, max_r, max_g, max_b);

    float track_r = slider_h * 0.5f;
    float cy = slider_min.y + track_r;
    float cx_left = slider_min.x + track_r;
    float cx_right = slider_max.x - track_r;

    // 硬件级顶点 Triangle Strip 绘制 100% 顺滑全圆角胶囊横向渐变条 (彻底去除直角毛刺，呈现圆润质感)
    const int num_slices = 64;
    dl->PrimReserve(num_slices * 6, (num_slices + 1) * 2);

    ImDrawIdx base_vtx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv_white = ImGui::GetIO().Fonts->TexUvWhitePixel;

    for (int k = 0; k <= num_slices; ++k) {
        float t = static_cast<float>(k) / static_cast<float>(num_slices);
        float x = slider_min.x + t * right_panel_w;

        float dy = track_r;
        if (x < cx_left) {
            float dx = cx_left - x;
            dy = std::sqrt(std::max(0.0f, track_r * track_r - dx * dx));
        } else if (x > cx_right) {
            float dx = x - cx_right;
            dy = std::sqrt(std::max(0.0f, track_r * track_r - dx * dx));
        }

        ImVec2 v_top(x, cy - dy);
        ImVec2 v_bot(x, cy + dy);

        uint32_t c_r = static_cast<uint32_t>(std::clamp(max_r * t * 255.0f, 0.0f, 255.0f));
        uint32_t c_g = static_cast<uint32_t>(std::clamp(max_g * t * 255.0f, 0.0f, 255.0f));
        uint32_t c_b = static_cast<uint32_t>(std::clamp(max_b * t * 255.0f, 0.0f, 255.0f));
        ImU32 col_t = IM_COL32(c_r, c_g, c_b, 255);

        dl->PrimWriteVtx(v_top, uv_white, col_t);
        dl->PrimWriteVtx(v_bot, uv_white, col_t);
    }

    for (int k = 0; k < num_slices; ++k) {
        ImDrawIdx i0 = static_cast<ImDrawIdx>(base_vtx + k * 2);
        ImDrawIdx i1 = static_cast<ImDrawIdx>(base_vtx + k * 2 + 1);
        ImDrawIdx i2 = static_cast<ImDrawIdx>(base_vtx + (k + 1) * 2);
        ImDrawIdx i3 = static_cast<ImDrawIdx>(base_vtx + (k + 1) * 2 + 1);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i1);
        dl->PrimWriteIdx(i3);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i3);
        dl->PrimWriteIdx(i2);
    }

    // 绘制胶囊外圈 1px 高光边框
    dl->AddRect(slider_min, slider_max, IM_COL32(255, 255, 255, 50), track_r, 0, 1.0f);

    // 明度游标尺寸与极简全圆角胶囊设计 (对齐 macOS 与苹果风格)
    float thumb_w = 14.0f;
    float thumb_h = slider_h + 6.0f; // 稍高于轨道，呈现立体质感
    float thumb_r = thumb_w * 0.5f;   // 100% 全圆角胶囊
    float thumb_travel = right_panel_w - thumb_w;
    float thumb_x = slider_min.x + thumb_w * 0.5f + cur_v * thumb_travel;

    // 明度滑条交互
    ImGui::SetCursorScreenPos(ImVec2(slider_min.x, cy - thumb_h * 0.5f));
    ImGui::InvisibleButton("##MacBrightnessSlider", ImVec2(right_panel_w, thumb_h));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetIO().MousePos.x;
        float new_v = std::clamp((mx - (slider_min.x + thumb_w * 0.5f)) / thumb_travel, 0.05f, 1.0f);
        float new_r, new_g, new_b;
        ImGui::ColorConvertHSVtoRGB(cur_h, cur_s, new_v, new_r, new_g, new_b);
        tm.setCustomColor(ImVec4(new_r, new_g, new_b, 1.0f));
    }

    // 绘制极简圆滑胶囊手柄 (全圆角 pill 造型，配备微投影与精工倒角)
    ImVec2 t0(thumb_x - thumb_r, cy - thumb_h * 0.5f);
    ImVec2 t1(thumb_x + thumb_r, cy + thumb_h * 0.5f);

    // 1. 柔和环境微投影
    dl->AddRectFilled(ImVec2(t0.x, t0.y + 1.5f), ImVec2(t1.x, t1.y + 2.5f), IM_COL32(0, 0, 0, 90), thumb_r);
    // 2. 润白实体胶囊手柄
    dl->AddRectFilled(t0, t1, IM_COL32(255, 255, 255, 255), thumb_r);
    // 3. 内部高光层
    dl->AddRect(ImVec2(t0.x + 0.5f, t0.y + 0.5f), ImVec2(t1.x - 0.5f, t1.y - 0.5f), IM_COL32(255, 255, 255, 200), thumb_r - 0.5f, 0, 1.0f);
    // 4. 金属微光轮廓线
    dl->AddRect(t0, t1, IM_COL32(0, 0, 0, 110), thumb_r, 0, 1.0f);

    // 2. 颜色预览大块与三行格式编码 (完全对齐图二的 ff2e8c / hsl / rgb)
    float info_y = slider_max.y + 12.0f;

    // 当前色实时预览大色块 (类似图二左下角大方块)
    float swatch_sz = 34.0f;
    ImVec2 sw_min(right_panel_x, info_y);
    ImVec2 sw_max(right_panel_x + swatch_sz, info_y + swatch_sz);
    dl->AddRectFilled(sw_min, sw_max, IM_COL32(r, g, b, 255), 6.0f);
    dl->AddRect(sw_min, sw_max, IM_COL32(255, 255, 255, 120), 6.0f, 0, 1.2f);

    // 旁边三列标签胶囊：HEX、RGB、HSL (类似图二的三个可复制字段)
    float box_x = right_panel_x + swatch_sz + 10.0f;
    float box_w = (right_panel_w - swatch_sz - 10.0f - 16.0f) / 3.0f;
    float box_h = 34.0f;

    auto drawCodeBox = [&](float bx, const char* label, const char* val) {
        ImVec2 b_min(bx, info_y);
        ImVec2 b_max(bx + box_w, info_y + box_h);
        dl->AddRectFilled(b_min, b_max, IM_COL32(255, 255, 255, 10), 6.0f);
        dl->AddRect(b_min, b_max, IM_COL32(255, 255, 255, 24), 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(bx + 8.0f, info_y + 3.0f), UIConfig::Color::TextMuted, label);
        dl->AddText(ImVec2(bx + 8.0f, info_y + 16.0f), UIConfig::Color::TextActive, val);
        if (Fonts::Small) ImGui::PopFont();
    };

    char hex_str[16];
    std::snprintf(hex_str, sizeof(hex_str), "#%02x%02x%02x", r, g, b);
    drawCodeBox(box_x, "HEX", hex_str);

    char rgb_str[24];
    std::snprintf(rgb_str, sizeof(rgb_str), "%u, %u, %u", r, g, b);
    drawCodeBox(box_x + box_w + 8.0f, "RGB", rgb_str);

    char hsl_str[24];
    std::snprintf(hsl_str, sizeof(hsl_str), "%d°, %d%%, %d%%",
                  static_cast<int>(std::round(cur_h * 360.0f)),
                  static_cast<int>(std::round(cur_s * 100.0f)),
                  static_cast<int>(std::round(cur_v * 100.0f)));
    drawCodeBox(box_x + (box_w + 8.0f) * 2.0f, "HSL", hsl_str);

    // 3. 经典发烧色格矩阵 (对齐图二底部的调色板小格矩阵)
    float grid_y = info_y + box_h + 10.0f;
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(right_panel_x, grid_y + 2.0f), UIConfig::Color::TextMuted, "经典名机色格：");
    if (Fonts::Small) ImGui::PopFont();

    static const struct {
        const char* name;
        ImVec4 col;
    } mac_swatches[] = {
        {"现代深空", ImVec4(0.98f, 0.18f, 0.28f, 1.0f)}, // Apple Music 玫红
        {"麦景图蓝", ImVec4(0.00f, 0.71f, 0.94f, 1.0f)}, // 麦景图冰蓝
        {"金嗓子金", ImVec4(0.92f, 0.70f, 0.03f, 1.0f)}, // 金嗓子香槟金
        {"复古琥珀", ImVec4(0.98f, 0.45f, 0.09f, 1.0f)}, // 模拟开盘机
        {"索尼黑金", ImVec4(0.90f, 0.73f, 0.35f, 1.0f)}, // 索尼金砖
        {"英国之宝", ImVec4(0.01f, 0.52f, 0.78f, 1.0f)}, // Meridian
        {"翡翠纯翠", ImVec4(0.06f, 0.73f, 0.51f, 1.0f)}, // 极光翠
        {"赛博极光", ImVec4(0.55f, 0.36f, 0.96f, 1.0f)}, // Cyber Violet
        {"经典朱砂", ImVec4(0.86f, 0.15f, 0.15f, 1.0f)}, // Ruby
        {"银月纯白", ImVec4(0.89f, 0.91f, 0.94f, 1.0f)}  // Silver White
    };

    float sw_start_x = right_panel_x + 95.0f;
    float sw_gap = 7.0f;
    float sw_w = 32.0f;
    float sw_h = 20.0f;

    for (size_t s = 0; s < 10; ++s) {
        float sx0 = sw_start_x + s * (sw_w + sw_gap);
        if (sx0 + sw_w > sec_x1 - 16.0f) break; // 安全边界防溢出

        ImVec2 s_min(sx0, grid_y);
        ImVec2 s_max(sx0 + sw_w, grid_y + sw_h);

        std::string s_id = "##MacSwatch_" + std::to_string(s);
        ImGui::SetCursorScreenPos(s_min);
        ImGui::InvisibleButton(s_id.c_str(), ImVec2(sw_w, sw_h));

        bool s_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setCustomColor(mac_swatches[s].col);
        }

        uint32_t sr = static_cast<uint32_t>(mac_swatches[s].col.x * 255.0f);
        uint32_t sg = static_cast<uint32_t>(mac_swatches[s].col.y * 255.0f);
        uint32_t sb = static_cast<uint32_t>(mac_swatches[s].col.z * 255.0f);
        ImU32 s_col = IM_COL32(sr, sg, sb, 255);

        dl->AddRectFilled(s_min, s_max, s_col, 4.0f);
        dl->AddRect(s_min, s_max, s_hov ? IM_COL32(255, 255, 255, 220) : IM_COL32(0, 0, 0, 60), 4.0f, 0, s_hov ? 1.5f : 1.0f);

        if (s_hov) {
            dl->AddRect(ImVec2(s_min.x - 1.5f, s_min.y - 1.5f), ImVec2(s_max.x + 1.5f, s_max.y + 1.5f), IM_COL32(255, 255, 255, 120), 5.5f, 0, 1.0f);
        }
    }
}
