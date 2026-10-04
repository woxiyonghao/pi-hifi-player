#include "themes/SiriWaveformRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

SiriWaveformRenderer::SiriWaveformRenderer() = default;

void SiriWaveformRenderer::drawWaveRibbon(ImDrawList* dl, float center_x, float center_y, float width,
                                          float amp, float freq, float phase, float speed_t,
                                          ImU32 col_top, ImU32 col_bot, float max_half_h,
                                          float gaussian_pow) {
    // 提升采样细分密度至 280 个极密微元，彻底消除低采样分段感
    constexpr int segments = 280;
    dl->PrimReserve(segments * 6, (segments + 1) * 2);

    ImDrawIdx base_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    float half_w = width * 0.5f;

    std::vector<ImVec2> top_pts(segments + 1);
    std::vector<ImVec2> bot_pts(segments + 1);
    std::vector<float> edge_fades(segments + 1);

    uint32_t tr_t = (col_top >> IM_COL32_R_SHIFT) & 0xFF;
    uint32_t tg_t = (col_top >> IM_COL32_G_SHIFT) & 0xFF;
    uint32_t tb_t = (col_top >> IM_COL32_B_SHIFT) & 0xFF;
    uint32_t ta_t = (col_top >> IM_COL32_A_SHIFT) & 0xFF;

    uint32_t tr_b = (col_bot >> IM_COL32_R_SHIFT) & 0xFF;
    uint32_t tg_b = (col_bot >> IM_COL32_G_SHIFT) & 0xFF;
    uint32_t tb_b = (col_bot >> IM_COL32_B_SHIFT) & 0xFF;
    uint32_t ta_b = (col_bot >> IM_COL32_A_SHIFT) & 0xFF;

    for (int i = 0; i <= segments; ++i) {
        float norm_x = static_cast<float>(i) / static_cast<float>(segments); // 0.0 ~ 1.0
        float norm_centered = (norm_x - 0.5f) * 2.0f; // -1.0 ~ +1.0
        float abs_x = std::abs(norm_centered);

        // 高斯钟形平滑衰减包络：两侧两端平滑归零收缩
        float gaussian = std::exp(-gaussian_pow * norm_centered * norm_centered);

        // 多频高阶谐波平滑正弦行波
        float wave_val = std::sin(norm_centered * freq * 3.14159265f - speed_t + phase)
                       + 0.32f * std::sin(norm_centered * (freq * 1.85f) * 3.14159265f - speed_t * 1.25f + phase * 1.4f)
                       + 0.12f * std::sin(norm_centered * (freq * 3.1f) * 3.14159265f - speed_t * 0.75f);

        // 两侧两端极度柔和的渐隐衰减 (消除生硬截断，向两端完全归零)
        float fade = std::clamp((1.0f - abs_x) / 0.38f, 0.0f, 1.0f);
        fade = fade * fade * (3.0f - 2.0f * fade); // smoothstep
        edge_fades[i] = fade;

        float half_h = std::clamp(std::abs(wave_val) * amp * gaussian, 0.2f, max_half_h);

        float px = center_x - half_w + norm_x * width;
        ImVec2 v_top(px, center_y - half_h);
        ImVec2 v_bot(px, center_y + half_h);

        top_pts[i] = v_top;
        bot_pts[i] = v_bot;

        ImU32 v_col_top = IM_COL32(tr_t, tg_t, tb_t, static_cast<uint32_t>(ta_t * fade));
        ImU32 v_col_bot = IM_COL32(tr_b, tg_b, tb_b, static_cast<uint32_t>(ta_b * fade));

        dl->PrimWriteVtx(v_top, uv, v_col_top);
        dl->PrimWriteVtx(v_bot, uv, v_col_bot);
    }

    // 填充波带实体三角形内部
    for (int i = 0; i < segments; ++i) {
        ImDrawIdx i0 = static_cast<ImDrawIdx>(base_idx + i * 2);
        ImDrawIdx i1 = static_cast<ImDrawIdx>(base_idx + i * 2 + 1);
        ImDrawIdx i2 = static_cast<ImDrawIdx>(base_idx + (i + 1) * 2);
        ImDrawIdx i3 = static_cast<ImDrawIdx>(base_idx + (i + 1) * 2 + 1);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i1);
        dl->PrimWriteIdx(i3);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i3);
        dl->PrimWriteIdx(i2);
    }

    // ==============================================================================
    // [抗锯齿与端部渐隐黑科技]
    // 轮廓带状描边伴随 fade 平滑衰减，在两端完全隐入纯黑舞台，彻底根除生硬色斑
    // ==============================================================================
    for (int i = 0; i < segments; ++i) {
        float seg_fade = (edge_fades[i] + edge_fades[i + 1]) * 0.5f;
        if (seg_fade < 0.01f) continue;

        uint32_t a_line = static_cast<uint32_t>(ta_t * seg_fade);
        uint32_t a_glow = a_line / 3;
        uint32_t a_edge = std::min(255u, a_line + static_cast<uint32_t>(30.0f * seg_fade));

        // 1. 次级柔化微光晕外边沿 (3.2px 软羽化)
        dl->AddLine(top_pts[i], top_pts[i + 1], IM_COL32(tr_t, tg_t, tb_t, a_glow), 3.2f);
        dl->AddLine(bot_pts[i], bot_pts[i + 1], IM_COL32(tr_t, tg_t, tb_t, a_glow), 3.2f);

        // 2. 核心极致平滑边缘线 (1.6px AA 轮廓)
        dl->AddLine(top_pts[i], top_pts[i + 1], IM_COL32(tr_t, tg_t, tb_t, a_edge), 1.6f);
        dl->AddLine(bot_pts[i], bot_pts[i + 1], IM_COL32(tr_t, tg_t, tb_t, a_edge), 1.6f);
    }
}

void SiriWaveformRenderer::render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 动态能量阻尼
    float raw_energy = (raw_level_l + raw_level_r) * 0.5f;
    float target_energy = is_playing ? raw_energy : 0.0f;
    smooth_energy_ += (target_energy - smooth_energy_) * (is_playing ? 14.0f : 6.0f) * dt;

    anim_time_ += (2.2f + smooth_energy_ * 3.8f) * dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空黑底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    float center_x = screen_w * 0.5f;
    float center_y = screen_h * 0.48f;
    float total_w = std::min(screen_w * 0.88f, 800.0f);

    // 2. 贯穿两端的激光水平微光线 (分段平滑渐变衰减，避免硬端点)
    constexpr int laser_segs = 64;
    float laser_half_w = total_w * 0.52f;
    for (int s = 0; s < laser_segs; ++s) {
        float t0 = static_cast<float>(s) / static_cast<float>(laser_segs);
        float t1 = static_cast<float>(s + 1) / static_cast<float>(laser_segs);
        float x0 = (center_x - laser_half_w) + t0 * (laser_half_w * 2.0f);
        float x1 = (center_x - laser_half_w) + t1 * (laser_half_w * 2.0f);

        float mid_norm = ((t0 + t1) * 0.5f - 0.5f) * 2.0f;
        float l_fade = std::clamp((1.0f - std::abs(mid_norm)) / 0.35f, 0.0f, 1.0f);
        l_fade = l_fade * l_fade * (3.0f - 2.0f * l_fade);
        if (l_fade <= 0.01f) continue;

        ImVec2 pA(x0, center_y);
        ImVec2 pB(x1, center_y);
        dl->AddLine(pA, pB, IM_COL32(255, 255, 255, static_cast<int>(25 * l_fade)), 3.0f);
        dl->AddLine(pA, pB, IM_COL32(255, 255, 255, static_cast<int>(80 * l_fade)), 1.2f);
        dl->AddLine(pA, pB, IM_COL32(255, 255, 255, static_cast<int>(210 * l_fade)), 0.6f);
    }

    // 基础静音待机波幅 ~12px，播放时随音乐大动态激荡至 45~95px
    float base_amp = 12.0f + smooth_energy_ * 72.0f;

    // 3. 渲染 3 层经典 Siri 多彩交错流体波浪 (Cyan / Magenta / Purple)
    // [波层 1: 电光青蓝 Cyan Ribbon]
    ImU32 col_cyan_top = IM_COL32(0, 242, 254, 140);
    ImU32 col_cyan_bot = IM_COL32(0, 180, 240, 50);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 0.95f, 2.2f, 0.0f, anim_time_ * 1.0f,
                   col_cyan_top, col_cyan_bot, 90.0f, 3.5f);

    // [波层 2: 鲜亮玫红 Magenta Ribbon]
    ImU32 col_mag_top = IM_COL32(255, 8, 68, 150);
    ImU32 col_mag_bot = IM_COL32(250, 45, 72, 50);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 1.12f, 2.8f, 1.85f, anim_time_ * 1.15f,
                   col_mag_top, col_mag_bot, 95.0f, 3.5f);

    // [波层 3: 极光紫罗兰 Violet Ribbon]
    ImU32 col_pur_top = IM_COL32(168, 85, 247, 135);
    ImU32 col_pur_bot = IM_COL32(121, 40, 202, 45);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 0.82f, 1.8f, 3.42f, anim_time_ * 0.85f,
                   col_pur_top, col_pur_bot, 85.0f, 3.5f);

    // [波层 4: 中央聚核白光高亮核心]
    // 使用全宽 + 陡峭高斯衰减 (gaussian_pow = 7.5f)，使白光高度聚集于中央并无缝融化入背景，杜绝任何硬截断
    ImU32 col_core_top = IM_COL32(255, 255, 255, 220);
    ImU32 col_core_bot = IM_COL32(255, 255, 255, 100);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 0.55f, 3.2f, 0.92f, anim_time_ * 1.25f,
                   col_core_top, col_core_bot, 45.0f, 7.5f);

    // 4. 底部发烧流体声波铭牌
    const char* footer_left = "Apple Siri Fluid Acoustic Waveform · Real-Time Spectral Modulation";
    const char* footer_right = "MULTI-LAYER TRAVELING SINE WAVE FLUIDITY";
    ImU32 badge_col = UIConfig::Color::Accent;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), badge_col, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), badge_col, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
