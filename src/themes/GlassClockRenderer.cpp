#include "themes/GlassClockRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <string>

GlassClockRenderer::GlassClockRenderer() = default;

void GlassClockRenderer::render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 动态音乐低频能量阻尼
    float raw_energy = (raw_level_l + raw_level_r) * 0.5f;
    float target_energy = is_playing ? raw_energy : 0.0f;
    smooth_energy_ += (target_energy - smooth_energy_) * (is_playing ? 14.0f : 5.0f) * dt;

    anim_time_ += dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空黑底板 (用户要求：背景色不用，保持纯净播放器主舞台底色)
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    const ImU32 accent = UIConfig::Color::Accent;
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 2. 获取当前系统真实时间 (HH:MM)
    std::time_t raw_t = std::time(nullptr);
    std::tm* now = std::localtime(&raw_t);
    char hour_buf[8];
    char min_buf[8];
    std::snprintf(hour_buf, sizeof(hour_buf), "%02d", now ? now->tm_hour : 12);
    std::snprintf(min_buf, sizeof(min_buf), "%02d", now ? now->tm_min : 0);

    ImFont* clock_font = Fonts::GiantClock ? Fonts::GiantClock : (Fonts::Large ? Fonts::Large : Fonts::Regular);
    float font_size = 145.0f;

    // 测量字符宽度与排版
    ImVec2 hour_sz = clock_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, hour_buf);
    ImVec2 colon_sz = clock_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ":");
    ImVec2 min_sz = clock_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, min_buf);

    float spacing = 18.0f;
    float total_w = hour_sz.x + spacing + colon_sz.x + spacing + min_sz.x;
    float start_x = (screen_w - total_w) * 0.5f;
    float start_y = (screen_h - hour_sz.y) * 0.44f;

    ImVec2 hour_pos(start_x, start_y);
    ImVec2 colon_pos(start_x + hour_sz.x + spacing, start_y);
    ImVec2 min_pos(start_x + hour_sz.x + spacing + colon_sz.x + spacing, start_y);

    // 3. 液态玻璃背后主题色漫射氛围辉光 (随音乐低频呼吸膨胀)
    float glow_cx = screen_w * 0.5f;
    float glow_cy = start_y + hour_sz.y * 0.5f;
    float glow_radius = (total_w * 0.48f) * (1.0f + smooth_energy_ * 0.20f);
    int glow_alpha = static_cast<int>(std::clamp(28.0f + smooth_energy_ * 55.0f, 0.0f, 255.0f));

    dl->AddCircleFilled(ImVec2(glow_cx, glow_cy), glow_radius, IM_COL32(ar, ag, ab, glow_alpha), 64);
    dl->AddCircleFilled(ImVec2(glow_cx, glow_cy), glow_radius * 1.45f, IM_COL32(ar, ag, ab, glow_alpha / 3), 64);

    // 冒号呼吸透光率 (平滑正弦闪烁)
    float colon_pulse = 0.55f + 0.45f * std::cos(anim_time_ * 3.14159265f);
    int colon_alpha = static_cast<int>(colon_pulse * 255.0f);

    // 4. 辅助渲染多通道 3D 液态玻璃字模渲染函数
    auto drawLiquidGlassText = [&](const char* text, ImVec2 pos, int extra_alpha) {
        // Pass 1: 底部柔和立体投影 (Drop Shadow)
        dl->AddText(clock_font, font_size, ImVec2(pos.x + 3.0f, pos.y + 7.0f),
                    IM_COL32(0, 0, 0, (90 * extra_alpha) / 255), text);

        // Pass 2: 右下方深度折射暗色边 (Refraction Shadow Bevel)
        dl->AddText(clock_font, font_size, ImVec2(pos.x + 2.2f, pos.y + 2.4f),
                    IM_COL32(ar / 3, ag / 3, ab / 3, (160 * extra_alpha) / 255), text);

        // Pass 3: 主题色半透明磨砂液态玻璃本体 (Liquid Frosted Glass Body)
        dl->AddText(clock_font, font_size, pos,
                    IM_COL32(ar, ag, ab, (75 * extra_alpha) / 255), text);

        // Pass 4: 左上方 3D 玻璃镜面弧度高光轮廓 (Specular Glass Crescent Highlight)
        dl->AddText(clock_font, font_size, ImVec2(pos.x - 1.6f, pos.y - 1.6f),
                    IM_COL32(255, 255, 255, (175 * extra_alpha) / 255), text);

        // Pass 5: 表面透光清澈微光晕 (Glass Surface Sheen)
        dl->AddText(clock_font, font_size, pos,
                    IM_COL32(255, 255, 255, (50 * extra_alpha) / 255), text);
    };

    // 渲染小时与分钟
    drawLiquidGlassText(hour_buf, hour_pos, 255);
    drawLiquidGlassText(":", colon_pos, colon_alpha);
    drawLiquidGlassText(min_buf, min_pos, 255);

    // 5. 底部发烧铭牌
    const char* footer_left = "Apple iOS StandBy · Liquid Glass Clock Dynamics";
    const char* footer_right = "REAL-TIME ACOUSTIC CAUSTIC REFRACTION";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), accent, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), accent, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
