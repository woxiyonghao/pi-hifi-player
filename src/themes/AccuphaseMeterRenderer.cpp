#include "themes/AccuphaseMeterRenderer.hpp"
#include "public/Font.hpp"
#include "tools/PlayerAdmin.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

// 金嗓子全景发烧调色板 (完美联动 4 大经典名机与发烧友自由调色)
struct AccuphasePalette {
    ImU32 chassis_bg;
    ImU32 meter_bg;
    ImU32 bezel_outer;
    ImU32 bezel_inner;
    ImU32 ribbon_safe;      // 上下色带主色
    ImU32 ribbon_glow;      // 表盘透光微辉光
    ImU32 overload_red;     // 过载红色带
    ImU32 tick_safe;        // 安全刻度线
    ImU32 text_safe;        // 刻度文字与居中 dB
    ImU32 needle_color;     // 纯白指针
    ImU32 needle_glow;      // 指针背光柔和辉光
    ImU32 emblem_color;     // 中央 Accuphase 徽标
    ImU32 emblem_glow;      // 徽标呼吸发光
    ImU32 footer_color;     // 底部发烧铭牌色
};

AccuphasePalette getAccuphasePalette(int theme_id, ImVec4 custom_col) {
    // 0: ModernCrimson, 1: McIntosh, 2: Accuphase, 3: RetroTape, 4: Custom
    if (theme_id == 4) { // Custom 自由调色 (色轮/明度/色格联动)
        float r_f = std::clamp(custom_col.x, 0.0f, 1.0f);
        float g_f = std::clamp(custom_col.y, 0.0f, 1.0f);
        float b_f = std::clamp(custom_col.z, 0.0f, 1.0f);

        uint32_t cr = static_cast<uint32_t>(r_f * 255.0f);
        uint32_t cg = static_cast<uint32_t>(g_f * 255.0f);
        uint32_t cb = static_cast<uint32_t>(b_f * 255.0f);

        // 透光高亮色带 (80% 纯正强调色 + 20% 纯白透光亮底)
        uint32_t rib_r = static_cast<uint32_t>(std::clamp(r_f * 205.0f + 50.0f, 0.0f, 255.0f));
        uint32_t rib_g = static_cast<uint32_t>(std::clamp(g_f * 205.0f + 50.0f, 0.0f, 255.0f));
        uint32_t rib_b = static_cast<uint32_t>(std::clamp(b_f * 205.0f + 50.0f, 0.0f, 255.0f));

        uint32_t txt_r = static_cast<uint32_t>(std::clamp(r_f * 45.0f + 210.0f, 0.0f, 255.0f));
        uint32_t txt_g = static_cast<uint32_t>(std::clamp(g_f * 45.0f + 210.0f, 0.0f, 255.0f));
        uint32_t txt_b = static_cast<uint32_t>(std::clamp(b_f * 45.0f + 210.0f, 0.0f, 255.0f));

        AccuphasePalette pal;
        pal.chassis_bg = IM_COL32(14, 11, 8, 255);
        pal.meter_bg = IM_COL32(7, 8, 10, 255);
        pal.bezel_outer = IM_COL32(static_cast<uint32_t>(r_f * 50.0f + 30.0f),
                                   static_cast<uint32_t>(g_f * 50.0f + 30.0f),
                                   static_cast<uint32_t>(b_f * 50.0f + 30.0f), 255);
        pal.bezel_inner = IM_COL32(24, 21, 17, 255);
        pal.ribbon_safe = IM_COL32(rib_r, rib_g, rib_b, 255);
        pal.ribbon_glow = IM_COL32(cr, cg, cb, 40);
        pal.overload_red = IM_COL32(235, 38, 24, 255);
        pal.tick_safe = IM_COL32(txt_r, txt_g, txt_b, 255);
        pal.text_safe = IM_COL32(txt_r, txt_g, txt_b, 255);
        pal.needle_color = IM_COL32(255, 255, 255, 255);
        pal.needle_glow = IM_COL32(cr, cg, cb, 80);
        pal.emblem_color = IM_COL32(cr, cg, cb, 255);
        pal.emblem_glow = IM_COL32(cr, cg, cb, 45);
        pal.footer_color = IM_COL32(cr, cg, cb, 210);
        return pal;
    }

    if (theme_id == 0) { // 现代深空玫红
        return {
            IM_COL32(14, 8, 11, 255),
            IM_COL32(8, 7, 10, 255),
            IM_COL32(75, 25, 38, 255),
            IM_COL32(24, 17, 20, 255),
            IM_COL32(255, 115, 140, 255),
            IM_COL32(250, 45, 72, 40),
            IM_COL32(255, 215, 0, 255),
            IM_COL32(255, 220, 230, 255),
            IM_COL32(255, 220, 230, 255),
            IM_COL32(255, 255, 255, 255),
            IM_COL32(250, 45, 72, 80),
            IM_COL32(250, 45, 72, 255),
            IM_COL32(250, 45, 72, 45),
            IM_COL32(250, 45, 72, 210)
        };
    }

    if (theme_id == 1) { // 麦景图湖蓝
        return {
            IM_COL32(6, 12, 22, 255),
            IM_COL32(6, 10, 16, 255),
            IM_COL32(20, 55, 80, 255),
            IM_COL32(14, 20, 28, 255),
            IM_COL32(100, 225, 255, 255),
            IM_COL32(0, 180, 240, 40),
            IM_COL32(239, 68, 68, 255),
            IM_COL32(215, 240, 255, 255),
            IM_COL32(215, 240, 255, 255),
            IM_COL32(255, 255, 255, 255),
            IM_COL32(0, 180, 240, 80),
            IM_COL32(0, 180, 240, 255),
            IM_COL32(0, 180, 240, 45),
            IM_COL32(0, 180, 240, 210)
        };
    }

    if (theme_id == 3) { // 复古琥珀卡座
        return {
            IM_COL32(18, 11, 8, 255),
            IM_COL32(10, 8, 7, 255),
            IM_COL32(75, 42, 20, 255),
            IM_COL32(26, 18, 14, 255),
            IM_COL32(255, 175, 95, 255),
            IM_COL32(249, 115, 22, 40),
            IM_COL32(239, 68, 68, 255),
            IM_COL32(255, 225, 195, 255),
            IM_COL32(255, 225, 195, 255),
            IM_COL32(255, 255, 255, 255),
            IM_COL32(249, 115, 22, 80),
            IM_COL32(249, 115, 22, 255),
            IM_COL32(249, 115, 22, 45),
            IM_COL32(249, 115, 22, 210)
        };
    }

    // 默认金嗓子原机 (ThemeId::Accuphase = 2)
    return {
        IM_COL32(14, 11, 8, 255),
        IM_COL32(7, 8, 10, 255),
        IM_COL32(65, 54, 40, 255),
        IM_COL32(24, 21, 17, 255),
        IM_COL32(212, 232, 252, 255), // 原机经典冰青暖玉色带
        IM_COL32(212, 232, 252, 25),
        IM_COL32(235, 38, 24, 255),
        IM_COL32(228, 240, 252, 255),
        IM_COL32(228, 240, 252, 255),
        IM_COL32(255, 255, 255, 255),
        IM_COL32(212, 232, 252, 60),
        IM_COL32(0, 235, 150, 255),   // 标志性祖母绿宝石徽标
        IM_COL32(0, 235, 150, 35),
        IM_COL32(210, 180, 120, 220)  // 香槟金铭牌
    };
}

} // namespace

AccuphaseMeterRenderer::AccuphaseMeterRenderer() {
    needle_val_l_ = 0.0f;
    needle_val_r_ = 0.0f;
}

void AccuphaseMeterRenderer::updateBallistics(float target_l, float target_r) {
    // 非线性听觉感官映射 (动态低电平展宽，高电平迅猛)
    float norm_l = std::pow(std::clamp(target_l, 0.0f, 1.0f), 0.82f);
    float norm_r = std::pow(std::clamp(target_r, 0.0f, 1.0f), 0.82f);

    // Attack ~12ms 迅猛冲顶，Decay ~280ms 柔和回落
    if (norm_l > needle_val_l_) {
        needle_val_l_ += (norm_l - needle_val_l_) * 0.35f;
    } else {
        needle_val_l_ += (norm_l - needle_val_l_) * 0.07f;
    }

    if (norm_r > needle_val_r_) {
        needle_val_r_ += (norm_r - needle_val_r_) * 0.35f;
    } else {
        needle_val_r_ += (norm_r - needle_val_r_) * 0.07f;
    }
}

void AccuphaseMeterRenderer::render(float screen_w, float screen_h, float raw_level_l, float raw_level_r) {
    updateBallistics(raw_level_l, raw_level_r);

    const AccuphasePalette pal = getAccuphasePalette(theme_id_, custom_color_);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 金嗓子原机香槟黑曜石底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), pal.chassis_bg);

    // 2. 双表头几何排版 (左右对称布局)
    const float pad_x = 24.0f;
    const float gap_x = 32.0f;
    const float meter_w = (screen_w - pad_x * 2.0f - gap_x) * 0.5f;
    const float pad_y = 52.0f;
    const float meter_h = 390.0f;

    ImVec2 left_min(pad_x, pad_y);
    ImVec2 left_max(pad_x + meter_w, pad_y + meter_h);

    ImVec2 right_min(pad_x + meter_w + gap_x, pad_y);
    ImVec2 right_max(screen_w - pad_x, pad_y + meter_h);

    // 3. 渲染左右两只金嗓子原机大表头
    drawSingleMeter(dl, left_min, left_max, needle_val_l_, "LEFT CHANNEL");
    drawSingleMeter(dl, right_min, right_max, needle_val_r_, "RIGHT CHANNEL");

    // 4. 中央 Accuphase 徽标
    drawCenterDisplay(dl, screen_w * 0.5f, screen_h);

    // 5. 底部发烧名机铭牌
    const char* footer_left = "Accuphase Laboratory, Inc. · Dual Balanced Precision Power Meter";
    const char* footer_right = "BALANCED AAVA · PURE CLASS A OPERATION";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(pad_x + 6.0f, screen_h - 32.0f), pal.footer_color, footer_left);
    ImVec2 r_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - r_sz.x - pad_x - 6.0f, screen_h - 32.0f), pal.footer_color, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}

void AccuphaseMeterRenderer::drawCenterDisplay(ImDrawList* dl, float center_x, float screen_h) {
    (void)screen_h;
    const AccuphasePalette pal = getAccuphasePalette(theme_id_, custom_color_);

    // 标志性 Accuphase 徽标呼吸微光 (机皇灵魂)
    float logo_pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 1.5f) + 1.0f) * 0.5f;
    int logo_glow_a = static_cast<int>(18.0f + 26.0f * logo_pulse);
    ImU32 glow_color = (pal.emblem_glow & 0x00FFFFFF) | (static_cast<uint32_t>(logo_glow_a) << IM_COL32_A_SHIFT);
    dl->AddRectFilled(ImVec2(center_x - 60.0f, 15.0f), ImVec2(center_x + 60.0f, 44.0f), glow_color, 6.0f);

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    const char* logo_str = "Accuphase";
    ImVec2 logo_sz = ImGui::CalcTextSize(logo_str);
    dl->AddText(ImVec2(center_x - logo_sz.x * 0.5f, 18.0f), pal.emblem_color, logo_str);
    if (Fonts::Medium) ImGui::PopFont();

    // 经典红光 7 段数码管音量读数 (-24 dB / -- dB)
    auto& player = PlayerAdmin::getInstance();
    float vol = player.getVolume();
    int db_atten = (vol > 0.01f) ? static_cast<int>(std::round((1.0f - vol) * -50.0f)) : -99;
    char db_buf[16];
    if (db_atten <= -99) {
        std::snprintf(db_buf, sizeof(db_buf), " -- ");
    } else {
        std::snprintf(db_buf, sizeof(db_buf), "-%02d", std::abs(db_atten));
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 db_sz = ImGui::CalcTextSize(db_buf);
    dl->AddText(ImVec2(center_x - db_sz.x * 0.5f, 460.0f), IM_COL32(255, 45, 45, 240), db_buf);
    if (Fonts::Small) ImGui::PopFont();
}

void AccuphaseMeterRenderer::drawSingleMeter(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float needle_val, const char* channel_label) {
    const AccuphasePalette pal = getAccuphasePalette(theme_id_, custom_color_);
    const float w = p_max.x - p_min.x;
    const float h = p_max.y - p_min.y;
    const float cx = p_min.x + w * 0.5f;

    // 1. 开启精确剪裁与表壳金属拉丝边框
    dl->PushClipRect(p_min, p_max, true);

    // 外层金属压铸框倒角
    dl->AddRect(ImVec2(p_min.x - 5.0f, p_min.y - 5.0f), ImVec2(p_max.x + 5.0f, p_max.y + 5.0f), pal.bezel_outer, 6.0f, 0, 2.0f);
    dl->AddRect(ImVec2(p_min.x - 2.0f, p_min.y - 2.0f), ImVec2(p_max.x + 2.0f, p_max.y + 2.0f), pal.bezel_inner, 4.0f, 0, 2.0f);

    // 表盘纯黑底面
    dl->AddRectFilled(p_min, p_max, pal.meter_bg);
    dl->AddRect(p_min, p_max, IM_COL32(40, 36, 30, 255), 2.0f);

    // 2. 机械指针枢轴点 (位于表盘底部正中)
    const ImVec2 pivot(cx, p_max.y - 10.0f);

    // 3. 水平双层色带基准纵向坐标
    const float y_top = p_min.y + h * 0.38f;
    const float y_bot = y_top + 13.0f;
    const float y_gap = 3.5f;
    const float y_low_top = y_bot + y_gap;
    const float y_low_bot = y_low_top + 4.0f;

    // 水平色带左右跨度
    const float span_l = w * 0.44f;
    const float x_start = cx - span_l;
    const float x_end = cx + span_l;

    // 动圈径向射线方程闭包：计算经过 (u, y_top) 并在任意 y 处的投影 x 坐标
    auto get_ray_x = [&](float u, float y) -> float {
        float x_ref = x_start + u * (x_end - x_start);
        return pivot.x + (x_ref - pivot.x) * (y - pivot.y) / (y_top - pivot.y);
    };

    // 表盘透光柔和背光微光晕
    if ((pal.ribbon_glow & IM_COL32_A_MASK) != 0) {
        ImVec2 glow_center(cx, y_top + 8.0f);
        dl->AddCircleFilled(glow_center, span_l * 0.65f, pal.ribbon_glow);
    }

    // 4. 精确物理刻度归一化位置 (对齐 Accuphase 原机照片)
    constexpr float u_minus  = 0.040f;
    constexpr float u_50     = 0.120f;
    constexpr float u_40     = 0.235f;
    constexpr float u_30     = 0.355f;
    constexpr float u_20     = 0.470f;
    constexpr float u_10     = 0.590f;
    constexpr float u_5_safe = 0.700f;
    constexpr float u_0      = 0.795f; // 0 dB 临界过载点 (红色区起始)
    constexpr float u_5_peak = 0.885f; // +5 dB
    constexpr float u_plus   = 0.950f; // + 标度

    // 5. 绘制上层梯形主色带 (Safe 强调色区 + Overload 警示红区)
    // [左侧安全主色块]
    constexpr float u_rib_l = 0.030f;
    ImVec2 tl_cyan(get_ray_x(u_rib_l - 0.02f, y_top), y_top);
    ImVec2 tr_cyan(get_ray_x(u_0, y_top), y_top);
    ImVec2 br_cyan(get_ray_x(u_0, y_bot), y_bot);
    ImVec2 bl_cyan(get_ray_x(u_rib_l, y_bot), y_bot);
    dl->AddQuadFilled(tl_cyan, tr_cyan, br_cyan, bl_cyan, pal.ribbon_safe);

    // [0dB ~ +5dB 警示红带底部连贯基座]
    const float y_red_mid = y_bot - 4.5f;
    ImVec2 r_tl(get_ray_x(u_0, y_red_mid), y_red_mid);
    ImVec2 r_tr(get_ray_x(u_5_peak + 0.02f, y_red_mid), y_red_mid);
    ImVec2 r_br(get_ray_x(u_5_peak + 0.02f, y_bot), y_bot);
    ImVec2 r_bl(get_ray_x(u_0, y_bot), y_bot);
    dl->AddQuadFilled(r_tl, r_tr, r_br, r_bl, pal.overload_red);

    // [0dB ~ +5dB 原机标志性 3 根倾斜红色梳齿]
    const float u_teeth[3] = {u_0, (u_0 + u_5_peak) * 0.5f, u_5_peak};
    const float tooth_w = 2.5f;
    for (float u_tooth : u_teeth) {
        ImVec2 t_tl(get_ray_x(u_tooth, y_top) - tooth_w * 0.5f, y_top);
        ImVec2 t_tr(get_ray_x(u_tooth, y_top) + tooth_w * 0.5f, y_top);
        ImVec2 t_br(get_ray_x(u_tooth, y_red_mid) + tooth_w * 0.5f, y_red_mid);
        ImVec2 t_bl(get_ray_x(u_tooth, y_red_mid) - tooth_w * 0.5f, y_red_mid);
        dl->AddQuadFilled(t_tl, t_tr, t_br, t_bl, pal.overload_red);
    }

    // [右侧延伸主色块]
    ImVec2 tr2_tl(get_ray_x(u_5_peak + 0.02f, y_top), y_top);
    ImVec2 tr2_tr(get_ray_x(u_plus + 0.03f, y_top), y_top);
    ImVec2 tr2_br(get_ray_x(u_plus + 0.01f, y_bot), y_bot);
    ImVec2 tr2_bl(get_ray_x(u_5_peak + 0.02f, y_bot), y_bot);
    dl->AddQuadFilled(tr2_tl, tr2_tr, tr2_br, tr2_bl, pal.ribbon_safe);

    // 6. 绘制下层细长梯形色带
    ImVec2 ll_tl(get_ray_x(u_rib_l + 0.02f, y_low_top), y_low_top);
    ImVec2 ll_tr(get_ray_x(u_plus + 0.01f, y_low_top), y_low_top);
    ImVec2 ll_br(get_ray_x(u_plus - 0.01f, y_low_bot), y_low_bot);
    ImVec2 ll_bl(get_ray_x(u_rib_l + 0.04f, y_low_bot), y_low_bot);
    dl->AddQuadFilled(ll_tl, ll_tr, ll_br, ll_bl, pal.ribbon_safe);

    // 7. 绘制上方 dB 刻度线与读数
    struct DBMark {
        float u;
        const char* label;
        bool is_major;
    };
    const DBMark db_marks[] = {
        {u_minus, "-", true},
        {u_50, "50", true},
        {(u_50 + u_40) * 0.5f, "", false},
        {u_40, "40", true},
        {(u_40 + u_30) * 0.5f, "", false},
        {u_30, "30", true},
        {(u_30 + u_20) * 0.5f, "", false},
        {u_20, "20", true},
        {(u_20 + u_10) * 0.5f, "", false},
        {u_10, "10", true},
        {(u_10 + u_5_safe) * 0.5f, "", false},
        {u_5_safe, "5", true},
        {u_0, "0", true},
        {(u_0 + u_5_peak) * 0.5f, "", false},
        {u_5_peak, "5", true},
        {u_plus, "+", true}
    };

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    for (const auto& m : db_marks) {
        bool is_red = (m.u >= u_0 - 0.005f && m.u <= u_5_peak + 0.005f);
        ImU32 tick_col = is_red ? pal.overload_red : pal.tick_safe;
        float tick_len = m.is_major ? 15.0f : 9.0f;

        float x_top = get_ray_x(m.u, y_top);
        float dx = x_top - pivot.x;
        float dy = y_top - pivot.y;
        float dist = std::hypot(dx, dy);
        float ux = dx / dist;
        float uy = dy / dist; // uy 为负值 (向上延伸)

        ImVec2 p0(x_top, y_top);
        ImVec2 p1(x_top + ux * tick_len, y_top + uy * tick_len);
        dl->AddLine(p0, p1, tick_col, m.is_major ? 2.0f : 1.0f);

        if (m.label && m.label[0] != '\0') {
            float tx = x_top + ux * (tick_len + 11.0f);
            float ty = y_top + uy * (tick_len + 11.0f);
            ImVec2 sz = ImGui::CalcTextSize(m.label);
            dl->AddText(ImVec2(tx - sz.x * 0.5f, ty - sz.y * 0.5f), pal.text_safe, m.label);
        }
    }
    if (Fonts::Small) ImGui::PopFont();

    // 8. 绘制下方 % 功率百分比刻度线与读数
    struct PctMark {
        float u;
        const char* label;
    };
    const PctMark pct_marks[] = {
        {u_50 + 0.04f, "0"},
        {u_40, "0.01"},
        {u_30, "0.1"},
        {u_20, "1"},
        {u_10, "10"},
        {u_0, "100"},
        {u_5_peak - 0.03f, "200"},
        {u_plus - 0.02f, "%"}
    };

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    for (const auto& pm : pct_marks) {
        float x_low = get_ray_x(pm.u, y_low_bot);
        float dx = x_low - pivot.x;
        float dy = y_low_bot - pivot.y;
        float dist = std::hypot(dx, dy);
        float ux = dx / dist;
        float uy = dy / dist;

        ImVec2 p0(x_low, y_low_bot);
        ImVec2 p1(x_low - ux * 5.0f, y_low_bot - uy * 5.0f); // 向下延伸
        dl->AddLine(p0, p1, pal.tick_safe, 1.0f);

        float tx = x_low - ux * 15.0f;
        float ty = y_low_bot - uy * 15.0f;
        ImVec2 sz = ImGui::CalcTextSize(pm.label);
        dl->AddText(ImVec2(tx - sz.x * 0.5f, ty - sz.y * 0.5f), pal.text_safe, pm.label);
    }
    if (Fonts::Small) ImGui::PopFont();

    // 9. 居中绘制标牌：dB 与 PEAK POWER LEVEL
    if (Fonts::Large) ImGui::PushFont(Fonts::Large);
    ImVec2 db_sz = ImGui::CalcTextSize("dB");
    dl->AddText(ImVec2(cx - db_sz.x * 0.5f, p_max.y - 110.0f), pal.text_safe, "dB");
    if (Fonts::Large) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 peak_sz = ImGui::CalcTextSize("PEAK POWER LEVEL");
    dl->AddText(ImVec2(cx - peak_sz.x * 0.5f, p_max.y - 78.0f), pal.text_safe, "PEAK POWER LEVEL");

    // 声道指示
    ImVec2 ch_sz = ImGui::CalcTextSize(channel_label);
    dl->AddText(ImVec2(cx - ch_sz.x * 0.5f, p_min.y + 16.0f), pal.text_safe, channel_label);
    if (Fonts::Small) ImGui::PopFont();

    // 10. 动圈指针 (白金纤细细针，穿透色带直达刻度顶端)
    float clamped_val = std::clamp(needle_val, 0.0f, 1.0f);
    float u_needle = u_minus + clamped_val * (u_plus - u_minus);
    float tip_y = y_top - 28.0f;
    float tip_x = get_ray_x(u_needle, tip_y);
    ImVec2 tip(tip_x, tip_y);

    // 指针柔和强调色微辉光
    dl->AddLine(pivot, tip, pal.needle_glow, 4.5f);
    // 纯白高精度实体指针
    dl->AddLine(pivot, tip, pal.needle_color, 2.0f);

    // 11. 底部机械半球金属旋转轴心盖 (Dome Cap)
    constexpr float r_cap = 24.0f;
    // 外圈暗金属底盘
    dl->PathArcTo(pivot, r_cap, 3.14159265f, 6.2831853f, 32);
    dl->PathFillConvex(IM_COL32(38, 42, 48, 255));
    dl->PathArcTo(pivot, r_cap, 3.14159265f, 6.2831853f, 32);
    dl->PathStroke(IM_COL32(135, 142, 155, 255), 0, 2.0f);

    // 中层高光金属球冠
    dl->PathArcTo(pivot, r_cap * 0.75f, 3.14159265f, 6.2831853f, 32);
    dl->PathFillConvex(IM_COL32(165, 172, 182, 255));

    // 顶端反光微弧
    dl->PathArcTo(pivot, r_cap * 0.45f, 3.14159265f, 6.2831853f, 32);
    dl->PathFillConvex(IM_COL32(220, 228, 238, 255));

    // 指针进入轴心的机械深色凹槽
    dl->AddRectFilled(ImVec2(pivot.x - 4.0f, pivot.y - 14.0f), ImVec2(pivot.x + 4.0f, pivot.y - 3.0f), IM_COL32(20, 22, 26, 255));

    dl->PopClipRect();
}
