#include "themes/AudioBubblesRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

AudioBubblesRenderer::AudioBubblesRenderer() = default;

void AudioBubblesRenderer::initBubbles(float screen_w, float screen_h) {
    bubbles_.clear();
    const int count = 38;
    bubbles_.reserve(count);

    for (int i = 0; i < count; ++i) {
        Bubble b;
        b.x = static_cast<float>(std::rand() % static_cast<int>(std::max(10.0f, screen_w)));
        b.y = static_cast<float>(std::rand() % static_cast<int>(std::max(10.0f, screen_h)));

        // 半径分阶：多数精致小气泡 (10~25px)，少量中气泡 (28~45px)，数个大光晕球 (50~72px)
        int r_class = i % 10;
        if (r_class < 6) {
            b.base_radius = 12.0f + static_cast<float>(std::rand() % 16);
            b.freq_band = 2 + (i % 2); // 中高频
            b.vy = 28.0f + static_cast<float>(std::rand() % 22);
            b.alpha = 0.45f + static_cast<float>(std::rand() % 30) * 0.01f;
        } else if (r_class < 9) {
            b.base_radius = 28.0f + static_cast<float>(std::rand() % 18);
            b.freq_band = 1; // 中低频
            b.vy = 18.0f + static_cast<float>(std::rand() % 16);
            b.alpha = 0.35f + static_cast<float>(std::rand() % 25) * 0.01f;
        } else {
            b.base_radius = 52.0f + static_cast<float>(std::rand() % 22);
            b.freq_band = 0; // 超重低音宏大气泡
            b.vy = 12.0f + static_cast<float>(std::rand() % 10);
            b.alpha = 0.22f + static_cast<float>(std::rand() % 18) * 0.01f;
        }

        b.current_radius = b.base_radius;
        b.vx_phase = static_cast<float>(std::rand() % 628) * 0.01f;
        b.vx_speed = 0.6f + static_cast<float>(std::rand() % 12) * 0.1f;
        b.vx_amp = 10.0f + static_cast<float>(std::rand() % 20);

        bubbles_.push_back(b);
    }
    initialized_ = true;
}

void AudioBubblesRenderer::updatePhysics(float dt, float screen_w, float screen_h, bool is_playing, const float* spectrum_12) {
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;

    if (is_playing && spectrum_12 != nullptr) {
        bass = (spectrum_12[0] + spectrum_12[1] + spectrum_12[2]) / 3.0f;
        mid = (spectrum_12[3] + spectrum_12[4] + spectrum_12[5] + spectrum_12[6]) / 4.0f;
        treble = (spectrum_12[7] + spectrum_12[8] + spectrum_12[9] + spectrum_12[10]) / 4.0f;
    }

    float total_energy = bass * 0.5f + mid * 0.3f + treble * 0.2f;

    for (auto& b : bubbles_) {
        // 浮力与音乐节奏共振加速
        float speed_factor = is_playing ? (1.0f + total_energy * 2.2f) : 1.0f;
        b.y -= b.vy * speed_factor * dt;

        // 横向柔和波浪摆动
        b.vx_phase += b.vx_speed * dt;

        // 触顶循环回到底部，并重新随机 X 轴
        if (b.y < -b.current_radius * 1.5f) {
            b.y = screen_h + b.current_radius + static_cast<float>(std::rand() % 40);
            b.x = static_cast<float>(std::rand() % static_cast<int>(std::max(10.0f, screen_w)));
        }

        // 声学能量驱动气泡膨胀呼吸
        float target_r = b.base_radius;
        if (is_playing) {
            if (b.freq_band == 0) {
                target_r = b.base_radius * (1.0f + bass * 0.45f);
            } else if (b.freq_band == 1) {
                target_r = b.base_radius * (1.0f + bass * 0.30f + mid * 0.15f);
            } else if (b.freq_band == 2) {
                target_r = b.base_radius * (1.0f + mid * 0.35f);
            } else {
                target_r = b.base_radius * (1.0f + treble * 0.30f);
            }
        }

        // 弹性物理平滑跟随
        b.current_radius += (target_r - b.current_radius) * (12.0f * dt);
    }
}

void AudioBubblesRenderer::render(float screen_w, float screen_h, bool is_playing, const float* spectrum_12) {
    if (!initialized_ || bubbles_.empty()) {
        initBubbles(screen_w, screen_h);
    }

    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    updatePhysics(dt, screen_w, screen_h, is_playing, spectrum_12);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 发烧深空纯净暗色底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    // 获取当前全局主题色
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 底部低频氛围光晕 (随低音呼吸涌动)
    float bass_glow = (is_playing && spectrum_12 != nullptr) ?
                      ((spectrum_12[0] + spectrum_12[1] + spectrum_12[2]) / 3.0f) : 0.0f;
    int bg_alpha = static_cast<int>(bass_glow * 35.0f);
    if (bg_alpha > 0) {
        dl->AddRectFilledMultiColor(
            ImVec2(0.0f, screen_h * 0.5f), ImVec2(screen_w, screen_h),
            IM_COL32(r, g, b, 0), IM_COL32(r, g, b, 0),
            IM_COL32(r, g, b, bg_alpha), IM_COL32(r, g, b, bg_alpha)
        );
    }

    // 2. 绘制每个液态半透明微光气泡
    for (const auto& bubble : bubbles_) {
        float draw_x = bubble.x + std::sin(bubble.vx_phase) * bubble.vx_amp;
        float draw_y = bubble.y;
        float rad = bubble.current_radius;

        // 剔除屏幕视口外的绘制
        if (draw_y < -rad * 2.0f || draw_y > screen_h + rad * 2.0f) continue;

        ImVec2 center(draw_x, draw_y);

        // [层级 1] 柔和外层漫反射辉光
        int glow_a = static_cast<int>(bubble.alpha * 38.0f);
        dl->AddCircleFilled(center, rad * 1.35f, IM_COL32(r, g, b, glow_a), 32);

        // [层级 2] 半透明液态玻璃主体
        int body_a = static_cast<int>(bubble.alpha * 70.0f);
        dl->AddCircleFilled(center, rad, IM_COL32(r, g, b, body_a), 32);

        // [层级 3] 微光边缘轮廓线 (液态透镜感)
        int rim_a = static_cast<int>(bubble.alpha * 190.0f);
        dl->AddCircle(center, rad, IM_COL32(r, g, b, rim_a), 32, 1.4f);

        // [层级 4] 左上角高光反光 (Crescent Specular Highlight)
        ImVec2 hl_center(draw_x - rad * 0.32f, draw_y - rad * 0.32f);
        float hl_rad = rad * 0.28f;
        int hl_a = static_cast<int>(bubble.alpha * 210.0f);
        dl->AddCircleFilled(hl_center, hl_rad, IM_COL32(255, 255, 255, hl_a), 16);

        // [层级 5] 右下角微弱次级边缘反光
        ImVec2 sub_center(draw_x + rad * 0.35f, draw_y + rad * 0.35f);
        int sub_a = static_cast<int>(bubble.alpha * 65.0f);
        dl->AddCircleFilled(sub_center, rad * 0.16f, IM_COL32(255, 255, 255, sub_a), 12);
    }

    // 3. 底部极简发烧铭牌
    const char* footer_left = "Liquid Audio Dynamic Glow · Ambient Floating Bubbles Visualizer";
    const char* footer_right = "60 FPS REAL-TIME FLUID RESONANCE";
    ImU32 badge_col = IM_COL32(r, g, b, 170);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), badge_col, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), badge_col, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
