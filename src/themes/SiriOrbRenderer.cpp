#include "themes/SiriOrbRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>

SiriOrbRenderer::SiriOrbRenderer() = default;

namespace {

void DrawRadialGradientBlob(ImDrawList* dl, ImVec2 center, float radius, ImU32 col_in, ImU32 col_out, int segments = 36) {
    if ((col_in & IM_COL32_A_MASK) == 0 && (col_out & IM_COL32_A_MASK) == 0) return;

    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    dl->PrimReserve(segments * 3, segments + 1);

    ImDrawIdx center_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    dl->PrimWriteVtx(center, uv, col_in);

    constexpr float kPi = 3.14159265358979323846f;
    for (int i = 0; i < segments; ++i) {
        float a = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * kPi;
        ImVec2 pos(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
        dl->PrimWriteVtx(pos, uv, col_out);
    }

    for (int i = 0; i < segments; ++i) {
        dl->PrimWriteIdx(center_idx);
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + i));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + ((i + 1) % segments)));
    }
}

} // namespace

void SiriOrbRenderer::render(float screen_w, float screen_h, bool is_playing, float raw_level_l, float raw_level_r) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    float raw_energy = (raw_level_l + raw_level_r) * 0.5f;
    float target_energy = is_playing ? raw_energy : 0.0f;
    smooth_energy_ += (target_energy - smooth_energy_) * (is_playing ? 14.0f : 5.0f) * dt;

    anim_time_ += (1.4f + smooth_energy_ * 3.0f) * dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空黑底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    ImVec2 center(screen_w * 0.5f, screen_h * 0.46f);
    float orb_radius = std::min(screen_w * 0.17f, 130.0f) * (1.0f + smooth_energy_ * 0.12f);

    // 获取当前用户设置的主题色 (外层玻璃材质使用主题色)
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // [外层效果 1: 主题色漫射微光晕]
    int halo_a = static_cast<int>(35.0f + smooth_energy_ * 45.0f);
    DrawRadialGradientBlob(dl, center, orb_radius * 1.55f,
                           IM_COL32(r, g, b, halo_a), IM_COL32(r, g, b, 0), 48);

    // 开启精确圆形裁剪区域，让里层经典多彩光斑完美包裹在玻璃球体内
    dl->PushClipRect(ImVec2(center.x - orb_radius, center.y - orb_radius),
                     ImVec2(center.x + orb_radius, center.y + orb_radius), true);

    // [外层玻璃材质暗基底]
    dl->AddCircleFilled(center, orb_radius, IM_COL32(10, 14, 22, 230), 64);

    // ==============================================================================
    // [里层保留经典 Siri 颜色]: 电光青、热力粉红、极光紫罗兰、琥珀金光团旋转交融
    // ==============================================================================
    float t = anim_time_;
    float orbit_r = orb_radius * (0.35f + smooth_energy_ * 0.15f);

    // 1. 电光青蓝光团 (Cyan Blob: #00F2FE)
    ImVec2 c_cyan(center.x + std::cos(t * 1.0f) * orbit_r,
                  center.y + std::sin(t * 1.0f) * orbit_r);
    DrawRadialGradientBlob(dl, c_cyan, orb_radius * 0.75f,
                           IM_COL32(0, 242, 254, 180), IM_COL32(0, 180, 240, 0), 32);

    // 2. 鲜亮玫红光团 (Magenta/Pink Blob: #FF0844)
    ImVec2 c_mag(center.x + std::cos(t * 1.25f + 2.1f) * orbit_r,
                 center.y + std::sin(t * 1.25f + 2.1f) * orbit_r);
    DrawRadialGradientBlob(dl, c_mag, orb_radius * 0.72f,
                           IM_COL32(255, 8, 68, 185), IM_COL32(250, 45, 72, 0), 32);

    // 3. 极光紫罗兰光团 (Purple Blob: #A855F7)
    ImVec2 c_pur(center.x + std::cos(t * 0.9f + 4.2f) * orbit_r,
                 center.y + std::sin(t * 0.9f + 4.2f) * orbit_r);
    DrawRadialGradientBlob(dl, c_pur, orb_radius * 0.70f,
                           IM_COL32(168, 85, 247, 175), IM_COL32(121, 40, 202, 0), 32);

    // 4. 温暖琥珀金次要光晕 (Amber Accent: #F59E0B)
    ImVec2 c_amb(center.x + std::cos(-t * 1.4f + 1.2f) * (orbit_r * 0.8f),
                 center.y + std::sin(-t * 1.4f + 1.2f) * (orbit_r * 0.8f));
    DrawRadialGradientBlob(dl, c_amb, orb_radius * 0.55f,
                           IM_COL32(245, 158, 11, 140), IM_COL32(245, 158, 11, 0), 28);

    // 5. 核心白光聚核 (Luminous Core)
    DrawRadialGradientBlob(dl, center, orb_radius * 0.45f,
                           IM_COL32(255, 255, 255, 220), IM_COL32(255, 255, 255, 0), 28);

    dl->PopClipRect();

    // ==============================================================================
    // [外层玻璃材质使用主题色]: 菲涅尔折射轮廓、3D液态微棱光与高光倒角
    // ==============================================================================
    // 1. 主题色半透明磨砂玻璃罩 (Frosted Theme Glass Tint)
    dl->AddCircleFilled(center, orb_radius, IM_COL32(r, g, b, 45), 64);

    // 2. 主题色菲涅尔边缘微光圈 (Fresnel Rim Glow)
    dl->AddCircle(center, orb_radius, IM_COL32(r, g, b, 190), 64, 2.0f);
    dl->AddCircle(center, orb_radius - 2.0f, IM_COL32(r, g, b, 95), 64, 1.2f);

    // 3. 左上角 3D 玻璃镜面弧度高光 (Specular Glass Crescent Highlight)
    ImVec2 hl_center(center.x - orb_radius * 0.38f, center.y - orb_radius * 0.38f);
    float hl_r = orb_radius * 0.28f;
    DrawRadialGradientBlob(dl, hl_center, hl_r,
                           IM_COL32(255, 255, 255, 200), IM_COL32(255, 255, 255, 0), 24);

    // 4. 右下角次级内部折射高光 (Secondary Caustic Glow)
    ImVec2 sub_center(center.x + orb_radius * 0.40f, center.y + orb_radius * 0.40f);
    float sub_r = orb_radius * 0.22f;
    DrawRadialGradientBlob(dl, sub_center, sub_r,
                           IM_COL32(r, g, b, 120), IM_COL32(r, g, b, 0), 20);

    // 5. 底部发烧流体光球铭牌
    const char* footer_left = "Apple Siri 3D Frosted Glass Orb · Theme-Adaptive Caustic Sphere";
    const char* footer_right = "MULTI-FREQUENCY HARMONIC PLASMA RESONANCE";
    ImU32 badge_col = accent;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), badge_col, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), badge_col, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
