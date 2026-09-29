#include "themes/VUMeterRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace {

struct MeterPalette {
    ImU32 chassis_bg;
    ImU32 meter_bg_base;
    ImU32 glow_core;
    ImU32 glow_outer;
    ImU32 border;
    ImU32 arc_color;
    ImU32 tick_safe;
    ImU32 tick_text_safe;
    ImU32 overload_red;
    ImU32 needle_color;
    ImU32 needle_glow;
    ImU32 pivot_base;
    ImU32 pivot_ring;
    ImU32 footer_badge;
    const char* sub_label;
    const char* footer_left;
    const char* footer_right;
};

const MeterPalette& getPalette(MeterThemeType theme) {
    static const MeterPalette mcintosh = {
        IM_COL32(3, 7, 18, 255),        // 深空底板黑
        IM_COL32(2, 16, 38, 255),       // 表盘深邃暗蓝底色
        IM_COL32(0, 180, 255, 130),     // 轴心核心灯泡强光
        IM_COL32(0, 100, 180, 85),      // 弥散外层柔和湖蓝
        IM_COL32(56, 189, 248, 150),    // 荧光外框
        IM_COL32(186, 230, 253, 215),   // 主刻度弧线
        IM_COL32(224, 242, 254, 230),   // 安全刻度线
        IM_COL32(186, 230, 253, 215),   // 安全刻度文字
        IM_COL32(239, 68, 68, 255),     // 过载警示红 (>=0dB)
        IM_COL32(255, 59, 48, 255),     // 亮红动圈指针
        IM_COL32(255, 59, 48, 60),      // 指针背光阴影
        IM_COL32(30, 41, 59, 255),      // 轴心石墨底座
        IM_COL32(148, 163, 184, 255),   // 轴心外圈金属圈
        IM_COL32(56, 189, 248, 180),    // 底部铭牌青
        "DECIBELS / WATTS",
        "McIntosh Precision Power Meter · Bit-Perfect 192kHz/24Bit ALSA",
        "AK4191EQ + AK4499EX VELVET SOUND"
    };

    static const MeterPalette accuphase = {
        IM_COL32(14, 11, 8, 255),
        IM_COL32(32, 25, 14, 255),
        IM_COL32(255, 220, 110, 140),
        IM_COL32(217, 160, 40, 80),
        IM_COL32(234, 179, 8, 170),
        IM_COL32(254, 243, 199, 220),
        IM_COL32(253, 230, 138, 230),
        IM_COL32(254, 240, 180, 220),
        IM_COL32(249, 115, 22, 255),
        IM_COL32(245, 60, 35, 255),
        IM_COL32(234, 179, 8, 60),
        IM_COL32(40, 32, 20, 255),
        IM_COL32(210, 180, 120, 255),
        IM_COL32(234, 179, 8, 200),
        "DECIBELS / PEAK WATTS",
        "Accuphase Laboratory, Inc. · 40th Anniversary Dual Balanced",
        "VELVET SOUND ULTRA · FEMTOSECOND CLOCK"
    };

    static const MeterPalette retro_tape = {
        IM_COL32(14, 9, 6, 255),
        IM_COL32(28, 16, 9, 255),
        IM_COL32(255, 180, 70, 130),
        IM_COL32(249, 115, 22, 75),
        IM_COL32(249, 115, 22, 170),
        IM_COL32(254, 215, 170, 220),
        IM_COL32(253, 186, 116, 230),
        IM_COL32(254, 215, 170, 220),
        IM_COL32(239, 68, 68, 255),
        IM_COL32(255, 69, 58, 255),
        IM_COL32(249, 115, 22, 60),
        IM_COL32(35, 22, 15, 255),
        IM_COL32(180, 130, 90, 255),
        IM_COL32(249, 115, 22, 200),
        "NAB / IEC EQUALIZATION",
        "Studer Revox Master Tape · Pure Analog Direct Calibration",
        "15 IPS 2-TRACK MASTER · 0VU = +4dBu"
    };

    static const MeterPalette modern_crimson = {
        IM_COL32(12, 6, 10, 255),
        IM_COL32(22, 8, 14, 255),
        IM_COL32(255, 100, 130, 120),
        IM_COL32(250, 45, 72, 70),
        IM_COL32(250, 45, 72, 160),
        IM_COL32(255, 220, 230, 220),
        IM_COL32(254, 205, 211, 230),
        IM_COL32(255, 220, 230, 220),
        IM_COL32(255, 215, 0, 255),
        IM_COL32(255, 45, 85, 255),
        IM_COL32(250, 45, 72, 60),
        IM_COL32(30, 16, 22, 255),
        IM_COL32(180, 130, 150, 255),
        IM_COL32(250, 45, 72, 200),
        "LUFS / TRUE PEAK AUDIO",
        "Studio Reference Monitor · Bit-Perfect 384kHz 32-Bit Float",
        "APPLE LOSSLESS AUDIO · ULTRA HI-RES"
    };

    switch (theme) {
        case MeterThemeType::Accuphase:     return accuphase;
        case MeterThemeType::RetroTape:     return retro_tape;
        case MeterThemeType::ModernCrimson: return modern_crimson;
        case MeterThemeType::McIntosh:
        default:                            return mcintosh;
    }
}

// 硬件级顶点插值径向渐变 (Triangle Fan)，通过 GPU 对每个像素做平滑插值
void DrawRadialGradient(ImDrawList* dl, ImVec2 center, float radius, ImU32 col_in, ImU32 col_out, int num_segments = 64) {
    if ((col_in & IM_COL32_A_MASK) == 0 && (col_out & IM_COL32_A_MASK) == 0)
        return;

    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    dl->PrimReserve(num_segments * 3, num_segments + 1);

    ImDrawIdx center_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    dl->PrimWriteVtx(center, uv, col_in);

    constexpr float kPi = 3.14159265358979323846f;
    for (int i = 0; i < num_segments; ++i) {
        float a = (static_cast<float>(i) / static_cast<float>(num_segments)) * 2.0f * kPi;
        ImVec2 pos(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
        dl->PrimWriteVtx(pos, uv, col_out);
    }

    for (int i = 0; i < num_segments; ++i) {
        dl->PrimWriteIdx(center_idx);
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + i));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + ((i + 1) % num_segments)));
    }
}

} // namespace

VUMeterRenderer::VUMeterRenderer() {
    needle_angle_l_ = kMinAngle;
    needle_angle_r_ = kMinAngle;
}

void VUMeterRenderer::updateBallistics(float target_l, float target_r) {
    float clamped_l = std::clamp(target_l, 0.0f, 1.0f);
    float clamped_r = std::clamp(target_r, 0.0f, 1.0f);

    float norm_l = std::pow(clamped_l, 0.9f);
    float norm_r = std::pow(clamped_r, 0.9f);

    float target_angle_l = kMinAngle + norm_l * kSweepAngle;
    float target_angle_r = kMinAngle + norm_r * kSweepAngle;

    if (target_angle_l > needle_angle_l_) {
        needle_angle_l_ += (target_angle_l - needle_angle_l_) * 0.32f;
    } else {
        needle_angle_l_ += (target_angle_l - needle_angle_l_) * 0.06f;
    }

    if (target_angle_r > needle_angle_r_) {
        needle_angle_r_ += (target_angle_r - needle_angle_r_) * 0.32f;
    } else {
        needle_angle_r_ += (target_angle_r - needle_angle_r_) * 0.06f;
    }
}

void VUMeterRenderer::render(float screen_w, float screen_h, float raw_level_l, float raw_level_r) {
    updateBallistics(raw_level_l, raw_level_r);

    const auto& pal = getPalette(current_theme_);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 夜空深色机架底板
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(screen_w, screen_h), pal.chassis_bg);

    // 2. 双表头几何排版 (左右各占半屏，发烧对称美学)
    float padding_x = 20.0f;
    float padding_y = 25.0f;
    float meter_w = (screen_w - 60.0f) * 0.5f;
    float meter_h = screen_h - 75.0f; // 留出底部发烧铭牌空间

    ImVec2 left_min(padding_x, padding_y);
    ImVec2 left_max(padding_x + meter_w, padding_y + meter_h);

    ImVec2 right_min(padding_x * 2.0f + meter_w, padding_y);
    ImVec2 right_max(padding_x * 2.0f + meter_w * 2.0f, padding_y + meter_h);

    // 3. 渲染左右两只动圈大表头
    drawSingleMeter(dl, left_min, left_max, needle_angle_l_, "LEFT CHANNEL");
    drawSingleMeter(dl, right_min, right_max, needle_angle_r_, "RIGHT CHANNEL");

    // 4. 底部发烧铭牌
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), pal.footer_badge, pal.footer_left);
    
    ImVec2 badge_sz = ImGui::CalcTextSize(pal.footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), pal.footer_badge, pal.footer_right);
}

void VUMeterRenderer::drawSingleMeter(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float angle, const char* channel_label) {
    const auto& pal = getPalette(current_theme_);
    float meter_w = p_max.x - p_min.x;
    float meter_h = p_max.y - p_min.y;
    ImVec2 center(p_min.x + meter_w * 0.5f, p_min.y + meter_h - 35.0f);
    float radius = meter_w * 0.44f;

    // 1. 开启精确圆角裁剪，保证内部光晕不溢出边框
    dl->PushClipRect(p_min, p_max, true);

    // 2. 表盘基底暗色
    dl->AddRectFilled(p_min, p_max, pal.meter_bg_base, 16.0f);

    // 3. 硬件级无级径向漫反射光晕 (Triangle Fan GPU 插值)
    // [外层大范围弥散环境柔光]
    ImU32 glow_outer_fade = pal.glow_outer & 0x00FFFFFF; // Alpha 0
    DrawRadialGradient(dl, ImVec2(center.x, center.y - 20.0f), radius * 1.18f,
                       pal.glow_outer, glow_outer_fade, 64);

    // [内层灯泡核心通透光晕]
    ImU32 glow_core_fade = pal.glow_core & 0x00FFFFFF; // Alpha 0
    DrawRadialGradient(dl, ImVec2(center.x, center.y - 10.0f), radius * 0.58f,
                       pal.glow_core, glow_core_fade, 64);

    dl->PopClipRect();

    // 4. 表盘发光外框 (16px 圆角)
    dl->AddRect(p_min, p_max, pal.border, 16.0f, 0, 2.0f);

    // 5. 绘制主刻度弧线与精确对数分度
    drawScaleAndTicks(dl, center, radius, kMinAngle, kSweepAngle);

    // 6. 绘制动圈红针与金属轴盖
    drawNeedle(dl, center, radius, angle);

    // 7. 居中绘制声道与发烧单位标牌
    ImVec2 title_sz = ImGui::CalcTextSize(channel_label);
    dl->AddText(ImVec2(center.x - title_sz.x * 0.5f, p_min.y + 36.0f), pal.arc_color, channel_label);

    ImVec2 sub_sz = ImGui::CalcTextSize(pal.sub_label);
    dl->AddText(ImVec2(center.x - sub_sz.x * 0.5f, center.y - 65.0f), pal.arc_color, pal.sub_label);
}

void VUMeterRenderer::drawScaleAndTicks(ImDrawList* dl, ImVec2 center, float radius, float start_angle, float total_sweep) {
    const auto& pal = getPalette(current_theme_);

    // 1. 主刻度弧线
    dl->PathArcTo(center, radius, start_angle, start_angle + total_sweep, 64);
    dl->PathStroke(pal.arc_color, 0, 3.0f);

    // 2. 10 档对数 dB 标度
    static const int marks[] = {-20, -10, -7, -5, -3, -1, 0, 1, 2, 3};
    static const int num_marks = sizeof(marks) / sizeof(marks[0]);

    for (int i = 0; i < num_marks; ++i) {
        int db = marks[i];
        float a = start_angle + (static_cast<float>(i) / (num_marks - 1)) * total_sweep;

        float cos_a = std::cos(a);
        float sin_a = std::sin(a);

        float tick_len = (i % 2 == 0) ? 18.0f : 10.0f;
        ImVec2 p_outer(center.x + cos_a * (radius - 4.0f), center.y + sin_a * (radius - 4.0f));
        ImVec2 p_inner(center.x + cos_a * (radius - tick_len), center.y + sin_a * (radius - tick_len));

        ImU32 tick_color = (db >= 0) ? pal.overload_red : pal.tick_safe;
        float tick_thick = (db >= 0) ? 3.0f : 2.0f;
        dl->AddLine(p_inner, p_outer, tick_color, tick_thick);

        // 主刻度标注数值 (-20, -7, -3, 0, +2)
        if (i % 2 == 0) {
            std::string text_str = (db > 0) ? ("+" + std::to_string(db)) : std::to_string(db);
            ImVec2 text_sz = ImGui::CalcTextSize(text_str.c_str());
            ImVec2 text_pos(
                center.x + cos_a * (radius - 32.0f) - text_sz.x * 0.5f,
                center.y + sin_a * (radius - 32.0f) - text_sz.y * 0.5f
            );
            ImU32 text_color = (db >= 0) ? pal.overload_red : pal.tick_text_safe;
            dl->AddText(text_pos, text_color, text_str.c_str());
        }
    }
}

void VUMeterRenderer::drawNeedle(ImDrawList* dl, ImVec2 center, float radius, float angle) {
    const auto& pal = getPalette(current_theme_);
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    ImVec2 needle_tip(center.x + cos_a * (radius - 8.0f), center.y + sin_a * (radius - 8.0f));

    // 指针发光阴影层
    dl->AddLine(center, needle_tip, pal.needle_glow, 6.0f);

    // 指针实体线
    dl->AddLine(center, needle_tip, pal.needle_color, 3.0f);

    // 机械轴盖 (双层同心圆拟合机械金属感)
    dl->AddCircleFilled(center, 14.0f, pal.pivot_base);
    dl->AddCircle(center, 14.0f, pal.pivot_ring, 32, 2.0f);
}
