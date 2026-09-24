#include "widgets/SidebarDacWidget.hpp"
#include "UIConfig.hpp"

void SidebarDacWidget::render(float width, float y, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float left_x = UIConfig::Layout::ContainerMarginX;
    float right_x = width - UIConfig::Layout::ContainerMarginX;
    float top_y = y;
    float bot_y = y + height;
    float rounding = UIConfig::Layout::ContainerRounding;

    // 交互悬停与点击检测
    ImGui::SetCursorScreenPos(ImVec2(left_x, top_y));
    bool clicked = ImGui::InvisibleButton("##DacContainerButton", ImVec2(right_x - left_x, bot_y - top_y));
    bool hovered = ImGui::IsItemHovered();

    if (clicked && on_click_) {
        on_click_();
    }

    // 1. 绘制 Xcode 风格液态玻璃底板与平滑边框
    dl->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                      UIConfig::Color::ContainerBg, rounding);

    if (hovered) {
        dl->AddRectFilled(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                          UIConfig::Color::GlassHover, rounding);
    }

    dl->AddRect(ImVec2(left_x, top_y), ImVec2(right_x, bot_y), 
                UIConfig::Color::ContainerBorder, rounding, 0, 1.0f);

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