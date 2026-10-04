#include "widgets/SidebarFeatureWidget.hpp"
#include "public/Platform.hpp"
#include "Font.hpp"
#include "UIConfig.hpp"
#include <string>
#include <cmath>

// 通透平滑的液态玻璃材质 (半透明底板 + 1px 折射微光边框)
static void DrawLiquidGlass(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, ImU32 fill_color) {
    dl->AddRectFilled(p_min, p_max, fill_color, rounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, rounding, 0, 1.0f);
}

SidebarFeatureWidget::SidebarFeatureWidget() {
    // 预装核心功能项
    features_ = {
        { SidebarTab::ScanMusic,      "扫描音乐" },
        { SidebarTab::Equalizer,      "均衡器" },
        { SidebarTab::MSEBTuning,     "调音魔棒" },
        { SidebarTab::DACSettings,    "DAC" },
        { SidebarTab::ThemeSettings,  "主题" },
        { SidebarTab::SystemSettings, "设置" }
    };

    // WiFi 无线传歌仅在树莓派等无头/便携 Linux 部署环境下启用，macOS 上不予显示
    if (Platform::isRaspberryPi()) {
        features_.push_back({ SidebarTab::WifiTransfer, "WiFi传歌" });
    }
}

void SidebarFeatureWidget::drawFeatureIcon(ImDrawList* dl, ImVec2 center, SidebarTab tab, ImU32 color) {
    switch (tab) {
        case SidebarTab::ScanMusic: {
            // 放大镜：镜框圆圈 + 45 度手柄
            ImVec2 c(center.x - 2.0f, center.y - 2.0f);
            dl->AddCircle(c, 5.0f, color, 16, 1.5f);
            dl->AddLine(ImVec2(c.x + 3.5f, c.y + 3.5f), ImVec2(c.x + 7.5f, c.y + 7.5f), color, 1.8f);
            break;
        }
        case SidebarTab::Equalizer: {
            // 均衡器：三根频率推子轨道 + 可调滑块旋钮
            float xs[3] = { center.x - 5.0f, center.x, center.x + 5.0f };
            float knobs[3] = { center.y - 2.0f, center.y + 3.0f, center.y - 4.0f };
            for (int i = 0; i < 3; ++i) {
                dl->AddLine(ImVec2(xs[i], center.y - 6.0f), ImVec2(xs[i], center.y + 6.0f), color, 1.2f);
                dl->AddCircleFilled(ImVec2(xs[i], knobs[i]), 2.2f, color);
            }
            break;
        }
        case SidebarTab::MSEBTuning: {
            // 调音魔棒：倾斜魔杖杖身 + 杖尖四角星芒
            ImVec2 wand_b(center.x - 5.5f, center.y + 5.5f);
            ImVec2 wand_t(center.x + 2.5f, center.y - 2.5f);
            dl->AddLine(wand_b, wand_t, color, 1.8f);
            // 杖尖星芒 (四向十字星芒 + 核心光点)
            ImVec2 star_c(center.x + 4.5f, center.y - 4.5f);
            dl->AddLine(ImVec2(star_c.x - 3.5f, star_c.y), ImVec2(star_c.x + 3.5f, star_c.y), color, 1.3f);
            dl->AddLine(ImVec2(star_c.x, star_c.y - 3.5f), ImVec2(star_c.x, star_c.y + 3.5f), color, 1.3f);
            dl->AddCircleFilled(star_c, 1.2f, color);
            break;
        }
        case SidebarTab::DACSettings: {
            // DAC 解码器：微型芯片封装底座 + 内部核心点
            ImVec2 m1(center.x - 6.0f, center.y - 5.0f);
            ImVec2 m2(center.x + 6.0f, center.y + 5.0f);
            dl->AddRect(m1, m2, color, 2.0f, 0, 1.4f);
            dl->AddCircleFilled(center, 1.8f, color);
            break;
        }
        case SidebarTab::ThemeSettings: {
            // 调色盘：经典画盘轮廓 + 两个调色小孔
            dl->AddCircle(center, 6.0f, color, 16, 1.4f);
            dl->AddCircleFilled(ImVec2(center.x - 2.0f, center.y - 2.0f), 1.2f, color);
            dl->AddCircleFilled(ImVec2(center.x + 2.0f, center.y - 1.0f), 1.2f, color);
            break;
        }
        case SidebarTab::SystemSettings: {
            // 系统设置：经典精工齿轮图标 (中心圆环 + 6 颗径向齿)
            dl->AddCircle(center, 4.0f, color, 16, 1.4f);
            constexpr float kPi = 3.14159265f;
            for (int i = 0; i < 6; ++i) {
                float a = i * (kPi / 3.0f);
                float ca = std::cos(a);
                float sa = std::sin(a);
                ImVec2 p0(center.x + ca * 4.0f, center.y + sa * 4.0f);
                ImVec2 p1(center.x + ca * 6.8f, center.y + sa * 6.8f);
                dl->AddLine(p0, p1, color, 1.6f);
            }
            break;
        }
        case SidebarTab::WifiTransfer: {
            // 无线传歌：经典 Wi-Fi 辐射波纹矢量图标 (中心点 + 双层同心弧度波)
            constexpr float kPi = 3.14159265f;
            ImVec2 origin(center.x, center.y + 4.0f);
            dl->AddCircleFilled(origin, 1.8f, color);
            dl->PathArcTo(origin, 4.5f, -kPi * 0.75f, -kPi * 0.25f, 16);
            dl->PathStroke(color, 0, 1.4f);
            dl->PathArcTo(origin, 8.0f, -kPi * 0.75f, -kPi * 0.25f, 16);
            dl->PathStroke(color, 0, 1.5f);
            break;
        }
        default:
            break;
    }
}

void SidebarFeatureWidget::drawHeader(const char* title) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextMuted);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

    if (Fonts::Small) {
        ImGui::PushFont(Fonts::Small);
    }
    ImGui::TextUnformatted(title);
    if (Fonts::Small) {
        ImGui::PopFont();
    }

    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

bool SidebarFeatureWidget::drawFeatureItem(SidebarTab tab, const char* label, bool is_selected, FeatureIndicatorTarget& out_target) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = UIConfig::Layout::NavItemHeight;
    float rounding = UIConfig::Layout::NavItemRounding;

    std::string btn_id = std::string("##feature_") + label;
    bool clicked = ImGui::InvisibleButton(btn_id.c_str(), ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + width, pos.y + height);

    // 记录被选中项的坐标，供外层通道绘制滑动发光胶囊
    if (is_selected) {
        out_target.x = p_min.x;
        out_target.y = pos.y;
        out_target.width = width;
        out_target.height = height;
    } else if (hovered) {
        // 未选中项在悬停时，绘制浅色临时毛玻璃
        DrawLiquidGlass(dl, p_min, p_max, rounding, UIConfig::Color::GlassHover);
    }

    // 颜色状态：激活高亮纯白，普通态柔灰
    ImU32 icon_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::IconNormal;
    ImU32 text_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal;

    // 绘制矢量图标与文字
    drawFeatureIcon(dl, ImVec2(pos.x + 18.0f, pos.y + height * 0.5f), tab, icon_col);
    dl->AddText(ImVec2(pos.x + 36.0f, pos.y + 8.0f), text_col, label);

    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    return clicked;
}

std::optional<FeatureIndicatorTarget> SidebarFeatureWidget::render(SidebarTab current_tab, TabSelectCallback on_select) {
    drawHeader("功能");

    std::optional<FeatureIndicatorTarget> active_target = std::nullopt;

    for (const auto& item : features_) {
        bool is_selected = (current_tab == item.tab);
        FeatureIndicatorTarget target{};

        if (drawFeatureItem(item.tab, item.label.c_str(), is_selected, target)) {
            if (on_select) {
                on_select(item.tab);
            }
        }

        if (is_selected) {
            active_target = target;
        }
    }

    return active_target;
}