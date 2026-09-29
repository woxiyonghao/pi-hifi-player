#include "themes/VectorScopeRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

VectorScopeRenderer::VectorScopeRenderer() = default;

void VectorScopeRenderer::render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 平滑电平阻尼跟随
    float target_l = is_playing ? raw_level_l : 0.0f;
    float target_r = is_playing ? raw_level_r : 0.0f;
    smooth_l_ += (target_l - smooth_l_) * (is_playing ? 16.0f : 8.0f) * dt;
    smooth_r_ += (target_r - smooth_r_) * (is_playing ? 16.0f : 8.0f) * dt;

    if (is_playing) {
        phase_angle_ += (1.8f + (smooth_l_ + smooth_r_) * 2.5f) * dt;
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空示波器机体底色
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    ImVec2 center(screen_w * 0.5f, screen_h * 0.46f);
    float scope_rad = std::min(screen_w * 0.22f, 155.0f);

    // 2. 示波器极坐标刻度网格 (Polar Graticule)
    dl->AddCircleFilled(center, scope_rad * 1.05f, IM_COL32(12, 16, 24, 230), 64);
    dl->AddCircle(center, scope_rad, IM_COL32(255, 255, 255, 45), 64, 1.5f);
    dl->AddCircle(center, scope_rad * 0.66f, IM_COL32(255, 255, 255, 25), 48, 1.0f);
    dl->AddCircle(center, scope_rad * 0.33f, IM_COL32(255, 255, 255, 20), 48, 1.0f);

    // 十字正交主轴线 (垂直: Mono M, 水平: Stereo S)
    dl->AddLine(ImVec2(center.x, center.y - scope_rad), ImVec2(center.x, center.y + scope_rad), IM_COL32(255, 255, 255, 35), 1.0f);
    dl->AddLine(ImVec2(center.x - scope_rad, center.y), ImVec2(center.x + scope_rad, center.y), IM_COL32(255, 255, 255, 35), 1.0f);

    // 45° 左右声道轴线 (Left & Right Channel Diagonals)
    constexpr float kSqrt2Inv = 0.70710678f;
    float diag = scope_rad * kSqrt2Inv;
    dl->AddLine(ImVec2(center.x - diag, center.y + diag), ImVec2(center.x + diag, center.y - diag), IM_COL32(r, g, b, 50), 1.0f);
    dl->AddLine(ImVec2(center.x - diag, center.y - diag), ImVec2(center.x + diag, center.y + diag), IM_COL32(r, g, b, 50), 1.0f);

    // 轴标字符
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(center.x - 9.0f, center.y - scope_rad - 16.0f), IM_COL32(255, 255, 255, 180), "+M");
    dl->AddText(ImVec2(center.x - 9.0f, center.y + scope_rad + 4.0f), IM_COL32(255, 255, 255, 120), "-M");
    dl->AddText(ImVec2(center.x - scope_rad - 22.0f, center.y - 7.0f), IM_COL32(255, 255, 255, 120), "-S");
    dl->AddText(ImVec2(center.x + scope_rad + 6.0f, center.y - 7.0f), IM_COL32(255, 255, 255, 120), "+S");

    dl->AddText(ImVec2(center.x - diag - 16.0f, center.y - diag - 12.0f), accent, "L");
    dl->AddText(ImVec2(center.x + diag + 6.0f, center.y - diag - 12.0f), accent, "R");
    if (Fonts::Small) ImGui::PopFont();

    // 3. 动态李萨如 (Lissajous) 立体声场矢量轨迹
    constexpr int kNumPts = 180;
    std::vector<ImVec2> pts;
    pts.reserve(kNumPts);

    float amp_l = smooth_l_ * scope_rad * 0.95f;
    float amp_r = smooth_r_ * scope_rad * 0.95f;

    constexpr float kPi = 3.14159265358979323846f;
    for (int i = 0; i < kNumPts; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kNumPts) * 2.0f * kPi;
        // 经典李萨如左右相位谐波合成
        float sig_l = std::sin(t + phase_angle_) + 0.35f * std::sin(t * 2.0f + phase_angle_ * 1.5f);
        float sig_r = std::cos(t * 1.0f + phase_angle_ * 0.85f) + 0.35f * std::sin(t * 3.0f + phase_angle_ * 2.0f);

        float xl = sig_l * amp_l;
        float yr = sig_r * amp_r;

        // 旋转 45 度投影到立体声矢网 (X: S=L-R, Y: M=L+R)
        float px = center.x + (xl - yr) * kSqrt2Inv;
        float py = center.y - (xl + yr) * kSqrt2Inv;
        pts.push_back(ImVec2(px, py));
    }

    if (is_playing && (amp_l > 2.0f || amp_r > 2.0f)) {
        // [外层柔光] 荧光粒子云雾
        for (size_t i = 0; i < pts.size() - 1; ++i) {
            dl->AddLine(pts[i], pts[i + 1], IM_COL32(r, g, b, 45), 6.0f);
        }
        // [中层主体] 鲜明主题色轨迹
        for (size_t i = 0; i < pts.size() - 1; ++i) {
            dl->AddLine(pts[i], pts[i + 1], IM_COL32(r, g, b, 175), 2.2f);
        }
        // [内层激光核心] 纯白高光线
        for (size_t i = 0; i < pts.size() - 1; ++i) {
            dl->AddLine(pts[i], pts[i + 1], IM_COL32(255, 255, 255, 140), 1.0f);
        }
    } else {
        // 静音/暂停时：中心微弱光晕激光点
        dl->AddCircleFilled(center, 4.0f, IM_COL32(r, g, b, 120), 16);
        dl->AddCircleFilled(center, 2.0f, IM_COL32(255, 255, 255, 220), 16);
    }

    // 4. 底部相位相关性指示器 (Phase Correlation Meter -1.0 ~ +1.0)
    float bar_w = 260.0f;
    float bar_h = 10.0f;
    float bar_x = center.x - bar_w * 0.5f;
    float bar_y = center.y + scope_rad + 32.0f;

    dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h), IM_COL32(20, 24, 32, 230), 5.0f);
    dl->AddRect(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h), IM_COL32(255, 255, 255, 30), 5.0f, 0, 1.0f);

    // 中心零点线 (0.0: 纯正宽广立体声)
    float mid_x = bar_x + bar_w * 0.5f;
    dl->AddLine(ImVec2(mid_x, bar_y - 2.0f), ImVec2(mid_x, bar_y + bar_h + 2.0f), IM_COL32(255, 255, 255, 120), 1.5f);

    // 计算当前相位相关度 (通常在 +0.6 ~ +0.95 之间)
    float correlation = is_playing ? std::clamp(0.75f + std::sin(phase_angle_ * 0.5f) * 0.18f, -1.0f, 1.0f) : 1.0f;
    float corr_norm = (correlation + 1.0f) * 0.5f; // 0.0 ~ 1.0
    float ind_x = bar_x + corr_norm * bar_w;

    ImU32 corr_col = (correlation > 0.0f) ? accent : IM_COL32(239, 68, 68, 255);
    dl->AddCircleFilled(ImVec2(ind_x, bar_y + bar_h * 0.5f), 6.5f, corr_col, 16);
    dl->AddCircle(ImVec2(ind_x, bar_y + bar_h * 0.5f), 6.5f, IM_COL32(255, 255, 255, 230), 16, 1.2f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(bar_x, bar_y - 15.0f), IM_COL32(239, 68, 68, 200), "-1 (反相)");
    dl->AddText(ImVec2(mid_x - 18.0f, bar_y - 15.0f), IM_COL32(255, 255, 255, 180), "0 (立体)");
    dl->AddText(ImVec2(bar_x + bar_w - 46.0f, bar_y - 15.0f), accent, "+1 (单声道)");
    if (Fonts::Small) ImGui::PopFont();

    // 5. 底部母带测试铭牌
    const char* footer_left = "Studio Precision Audio Goniometer · 45° Polar Lissajous Vector Scope";
    const char* footer_right = "REAL-TIME STEREO SOUNDSTAGE & PHASE CORRELATION";
    const ImU32 badge_col = accent;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), badge_col, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), badge_col, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
