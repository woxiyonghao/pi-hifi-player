#include "widgets/SidebarDacWidget.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "UIConfig.hpp"

void SidebarDacWidget::render(float width, float y, float height, float offset_x, float offset_y) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float left_x = UIConfig::Layout::ContainerMarginX + offset_x;
    float right_x = width - UIConfig::Layout::ContainerMarginX + offset_x;
    float top_y = y + offset_y;
    float bot_y = y + height + offset_y;
    float rounding = UIConfig::Layout::ContainerRounding;

    // 交互悬停与点击检测
    ImGui::SetCursorScreenPos(ImVec2(left_x, top_y));
    bool clicked = ImGui::InvisibleButton("##DacContainerButton", ImVec2(right_x - left_x, bot_y - top_y));
    bool hovered = ImGui::IsItemHovered();

    if (clicked && on_click_) {
        on_click_();
    }

    // 1. 绘制玻璃卡片底板 (自适应液态玻璃或毛玻璃)
    GlassCardRenderer::drawCard(dl, ImVec2(left_x, top_y), ImVec2(right_x, bot_y), rounding, "sidebar");

    if (hovered) {
        dl->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                          UIConfig::Color::GlassHover, rounding);
    }

    // 2. 内部状态指示灯与文字排版
    float center_y = (top_y + bot_y) * 0.5f;
    float led_x = left_x + 18.0f;

    if (connected_) {
        // [已连接]：发光绿灯 + 高亮设备名称
        dl->AddCircleFilled(ImVec2(led_x, center_y), 5.5f, IM_COL32(52, 199, 89, 70));
        dl->AddCircleFilled(ImVec2(led_x, center_y), 3.0f, UIConfig::Color::DacConnected);
        dl->AddText(ImVec2(led_x + 12.0f, center_y - 7.0f), UIConfig::Color::TextNormal, dac_name_.c_str());
    } else {
        // [未连接]：微光灰点 + 次级提示文本
        dl->AddCircleFilled(ImVec2(led_x, center_y), 3.0f, UIConfig::Color::DacDisconnected);
        dl->AddText(ImVec2(led_x + 12.0f, center_y - 7.0f), UIConfig::Color::TextMuted, "DAC: 未连接");
    }
}