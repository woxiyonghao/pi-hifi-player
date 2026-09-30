#include "themes/SiriWaveformRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>

SiriWaveformRenderer::SiriWaveformRenderer() = default;

void SiriWaveformRenderer::drawWaveRibbon(ImDrawList* dl, float center_x, float center_y, float width,
                                          float amp, float freq, float phase, float speed_t,
                                          ImU32 col_top, ImU32 col_bot, float max_half_h) {
    const int segments = 120;
    dl->PrimReserve(segments * 6, (segments + 1) * 2);

    ImDrawIdx base_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    float half_w = width * 0.5f;

    for (int i = 0; i <= segments; ++i) {
        float norm_x = static_cast<float>(i) / static_cast<float>(segments); // 0.0 ~ 1.0
        float norm_centered = (norm_x - 0.5f) * 2.0f; // -1.0 ~ +1.0

        // 高斯钟形衰减包络：两侧两端平滑归零收缩
        float gaussian = std::exp(-3.6f * norm_centered * norm_centered);

        // 多谐波正弦行波
        float wave_val = std::sin(norm_centered * freq * 3.14159265f - speed_t + phase)
                       + 0.35f * std::sin(norm_centered * (freq * 1.8f) * 3.14159265f - speed_t * 1.3f + phase * 1.5f);

        float half_h = std::clamp(std::abs(wave_val) * amp * gaussian, 1.2f, max_half_h);

        float px = center_x - half_w + norm_x * width;
        ImVec2 v_top(px, center_y - half_h);
        ImVec2 v_bot(px, center_y + half_h);

        dl->PrimWriteVtx(v_top, uv, col_top);
        dl->PrimWriteVtx(v_bot, uv, col_bot);
    }

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
    float total_w = std::min(screen_w * 0.88f, 780.0f);

    // 2. 贯穿两端的激光水平微光线
    dl->AddLine(ImVec2(center_x - total_w * 0.52f, center_y),
                ImVec2(center_x + total_w * 0.52f, center_y),
                IM_COL32(255, 255, 255, 45), 1.2f);

    // 基础静音待机波幅 ~12px，播放时随音乐大动态激荡至 45~95px
    float base_amp = 12.0f + smooth_energy_ * 72.0f;

    // 3. 渲染 3 层经典 Siri 多彩交错流体波浪 (Cyan / Magenta / Purple)
    // [波层 1: 电光青蓝 Cyan Ribbon]
    ImU32 col_cyan_top = IM_COL32(0, 242, 254, 130);
    ImU32 col_cyan_bot = IM_COL32(0, 180, 240, 45);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 0.95f, 2.2f, 0.0f, anim_time_ * 1.0f,
                   col_cyan_top, col_cyan_bot, 90.0f);

    // [波层 2: 鲜亮玫红 Magenta Ribbon]
    ImU32 col_mag_top = IM_COL32(255, 8, 68, 140);
    ImU32 col_mag_bot = IM_COL32(250, 45, 72, 45);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 1.12f, 2.8f, 1.85f, anim_time_ * 1.15f,
                   col_mag_top, col_mag_bot, 95.0f);

    // [波层 3: 极光紫罗兰 Violet Ribbon]
    ImU32 col_pur_top = IM_COL32(168, 85, 247, 125);
    ImU32 col_pur_bot = IM_COL32(121, 40, 202, 40);
    drawWaveRibbon(dl, center_x, center_y, total_w, base_amp * 0.82f, 1.8f, 3.42f, anim_time_ * 0.85f,
                   col_pur_top, col_pur_bot, 85.0f);

    // [波层 4: 中央聚核白光高亮核心]
    ImU32 col_core_top = IM_COL32(255, 255, 255, 210);
    ImU32 col_core_bot = IM_COL32(255, 255, 255, 90);
    drawWaveRibbon(dl, center_x, center_y, total_w * 0.75f, base_amp * 0.55f, 3.2f, 0.92f, anim_time_ * 1.25f,
                   col_core_top, col_core_bot, 45.0f);

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
