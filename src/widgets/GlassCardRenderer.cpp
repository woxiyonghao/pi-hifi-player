#include "widgets/GlassCardRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>

static GlassStyle s_glass_style = GlassStyle::FrostedGlass;

GlassStyle GlassCardRenderer::getStyle() {
    return s_glass_style;
}

void GlassCardRenderer::setStyle(GlassStyle style) {
    s_glass_style = style;
}

void GlassCardRenderer::drawFrosted(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    // 1. 底层高密度磨砂漫射深空基板 (Frosted Diffusion Base, ~86% 不透明度)
    // 将底层强烈的音频律动方块柔化为深邃朦胧的暗涌光芒，完全消除生硬割裂感
    dl->AddRectFilled(p_min, p_max, IM_COL32(18, 22, 32, 220), rounding);

    // 2. 漫射磨砂质感散射层 (Milky Matte Sheen)
    dl->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 12), rounding);

    // 3. 内部漫反射凹槽暗角 (Inner Thickness Occlusion)
    dl->AddRect(
        ImVec2(p_min.x + 1.0f, p_min.y + 1.0f),
        ImVec2(p_max.x - 1.0f, p_max.y - 1.0f),
        IM_COL32(0, 0, 0, 45),
        rounding - 1.0f, 0, 1.0f
    );

    // 4. 外层纯正哑光无眩光边框 (Matte Bevel Border)
    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 36), rounding, 0, 1.0f);
}

void GlassCardRenderer::drawLiquid(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding) {
    // 1. 清澈液态流动基底 (Clear Liquid Base, ~64% 不透明度)
    dl->AddRectFilled(p_min, p_max, IM_COL32(12, 16, 26, 165), rounding);

    // 2. 表面曲面镜面反光渐变 (Surface Specular Sheen Gradient)
    dl->AddRectFilledMultiColor(
        p_min, p_max,
        IM_COL32(255, 255, 255, 36),
        IM_COL32(255, 255, 255, 18),
        IM_COL32(255, 255, 255, 6),
        IM_COL32(255, 255, 255, 14)
    );

    // 3. 顶部液态晶莹高光反光弧 (Top Specular Highlight Line)
    float pad = std::max(rounding, 8.0f);
    dl->AddLine(
        ImVec2(p_min.x + pad, p_min.y + 0.8f),
        ImVec2(p_max.x - pad, p_min.y + 0.8f),
        IM_COL32(255, 255, 255, 130),
        1.5f
    );

    // 4. 底部微弱折射阴影 (Bottom Ambient Occlusion Line)
    dl->AddLine(
        ImVec2(p_min.x + pad, p_max.y - 0.8f),
        ImVec2(p_max.x - pad, p_max.y - 0.8f),
        IM_COL32(0, 0, 0, 80),
        1.2f
    );

    // 5. 晶莹微光折射双重轮廓边框 (Liquid Glass Refraction Border)
    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 52), rounding, 0, 1.0f);

    // 6. 微量主题色色散微光 (Chromatic Aberration Tint)
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;
    dl->AddRect(p_min, p_max, IM_COL32(r, g, b, 24), rounding, 0, 1.0f);
}

void GlassCardRenderer::drawCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, const char* /*panel_role*/) {
    if (s_glass_style == GlassStyle::LiquidGlass) {
        drawLiquid(dl, p_min, p_max, rounding);
    } else {
        // 默认统统使用用户最喜爱的顶级深空毛玻璃效果 (Frosted Glass)
        drawFrosted(dl, p_min, p_max, rounding);
    }
}
