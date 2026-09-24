#include "widgets/SidebarFeatureWidget.hpp"
#include "Font.hpp"
#include "UIConfig.hpp"
#include <string>

// 通透平滑的液态玻璃材质 (半透明底板 + 1px 折射微光边框)
static void DrawLiquidGlass(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, ImU32 fill_color) {
    dl->AddRectFilled(p_min, p_max, fill_color, rounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, rounding, 0, 1.0f);
}

SidebarFeatureWidget::SidebarFeatureWidget() {
    // 预装 5 大核心功能项
    features_ = {
        { SidebarTab::ScanMusic,      "扫描音乐" },
        { SidebarTab::Equalizer,      "Equalizer (EQ)" },
        { SidebarTab::DACSettings,    "DACSettings" },
        { SidebarTab::ThemeSettings,  "ThemeSettings" },
        { SidebarTab::SystemSettings, "SystemSettings" }
    };
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
            // 系统设置：微型同心环与中心节点
            dl->AddCircle(center, 5.5f, color, 16, 1.4f);
            dl->AddCircleFilled(center, 2.0f, color);
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