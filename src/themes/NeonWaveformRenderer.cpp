#include "themes/NeonWaveformRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

NeonWaveformRenderer::NeonWaveformRenderer() = default;

void NeonWaveformRenderer::render(float screen_w, float screen_h, bool is_playing, const float* spectrum_levels, int num_levels) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 平滑插值更新频段能量
    int n_bands = std::min(num_levels, 16);
    float total_energy = 0.0f;
    for (int i = 0; i < n_bands; ++i) {
        float target = (is_playing && spectrum_levels) ? spectrum_levels[i] : 0.0f;
        smooth_levels_[i] += (target - smooth_levels_[i]) * (is_playing ? 14.0f : 5.0f) * dt;
        total_energy += smooth_levels_[i];
    }
    float avg_energy = (n_bands > 0) ? (total_energy / n_bands) : 0.0f;

    anim_time_ += (1.8f + avg_energy * 3.5f) * dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空黑底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    const ImU32 accent = UIConfig::Color::Accent;
    const uint32_t r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float center_y = screen_h * 0.48f;

    // 2. 背景层：垂直全景频谱律动光柱 (仅在有能量时绘制，彻底根除暗格低透明度色斑)
    int num_bars = 48;
    float bar_spacing = screen_w / static_cast<float>(num_bars);
    float bar_w = bar_spacing * 0.65f;

    for (int i = 0; i < num_bars; ++i) {
        float norm_i = static_cast<float>(i) / static_cast<float>(num_bars - 1);
        float band_pos = norm_i * static_cast<float>(std::max(1, n_bands - 1));
        int idx0 = static_cast<int>(band_pos);
        int idx1 = std::min(idx0 + 1, n_bands - 1);
        float frac = band_pos - static_cast<float>(idx0);
        float band_lvl = smooth_levels_[idx0] * (1.0f - frac) + smooth_levels_[idx1] * frac;

        if (band_lvl <= 0.04f) continue;

        float bar_h = 10.0f + std::pow(band_lvl, 0.9f) * (screen_h * 0.38f);
        float bx = i * bar_spacing + (bar_spacing - bar_w) * 0.5f;
        int bar_alpha = static_cast<int>(std::clamp(band_lvl * 110.0f, 0.0f, 150.0f));

        dl->AddRectFilled(
            ImVec2(bx, center_y - bar_h), ImVec2(bx + bar_w, center_y + bar_h),
            IM_COL32(r, g, b, bar_alpha), 2.0f
        );
    }

    // 4. 前景层：电光霓虹脉冲波浪核心 (高频密集抗锯齿多道光带)
    constexpr int wave_pts = 320;
    std::vector<ImVec2> pts(wave_pts);

    for (int k = 0; k < wave_pts; ++k) {
        float norm_x = static_cast<float>(k) / static_cast<float>(wave_pts - 1);
        float x = norm_x * screen_w;

        // 映射对应的频段能量
        float band_pos = norm_x * static_cast<float>(std::max(1, n_bands - 1));
        int b0 = static_cast<int>(band_pos);
        int b1 = std::min(b0 + 1, n_bands - 1);
        float f = band_pos - static_cast<float>(b0);
        float energy = smooth_levels_[b0] * (1.0f - f) + smooth_levels_[b1] * f;

        // 快速高频锐利音频震荡波
        float high_pulse = std::sin(norm_x * 95.0f - anim_time_ * 3.5f) * 0.45f
                         + std::sin(norm_x * 160.0f + anim_time_ * 5.0f) * 0.35f
                         + std::sin(norm_x * 40.0f - anim_time_ * 1.8f) * 0.55f;

        float amp = (3.0f + energy * 70.0f) * (0.35f + std::abs(high_pulse));
        float y = center_y + high_pulse * amp;

        pts[k] = ImVec2(x, y);
    }

    // 多通道抗锯齿发光波形 (由宽到窄，由浓至极亮)
    // Pass 1: 外部宽广霓虹外光晕 (6.0px)
    dl->AddPolyline(pts.data(), wave_pts, IM_COL32(r, g, b, 55), 0, 6.0f);

    // Pass 2: 中层饱和电光色彩 (2.8px)
    dl->AddPolyline(pts.data(), wave_pts, IM_COL32(r, g, b, 175), 0, 2.8f);

    // Pass 3: 中央高亮白热核心线 (1.0px)
    dl->AddPolyline(pts.data(), wave_pts, IM_COL32(255, 255, 255, 240), 0, 1.0f);

    // 5. 底部铭牌
    const char* footer_left = "Electric Neon Pulse Waveform · Real-Time Spectral Equalizer";
    const char* footer_right = "HIGH-VOLTAGE RESONANCE & VOLUMETRIC AURA";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), accent, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), accent, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
