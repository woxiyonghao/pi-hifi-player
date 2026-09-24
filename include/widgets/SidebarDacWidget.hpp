#pragma once

#include "imgui.h"
#include <functional>
#include <string>

class SidebarDacWidget {
public:
    using ClickCallback = std::function<void()>;

    SidebarDacWidget() = default;
    ~SidebarDacWidget() = default;

    // 渲染 DAC 状态胶囊卡片
    // @param width 侧边栏总宽度 (用于计算 Xcode 风格左右 16px 边距)
    // @param y 控件起始 Y 坐标
    // @param height 卡片固定高度 (UIConfig::Layout::DacCardHeight)
    void render(float width, float y, float height);

    // 硬件连接状态与设备名称
    void setConnected(bool connected, const std::string& name = "ES9038PRO") {
        connected_ = connected;
        dac_name_ = name;
    }
    [[nodiscard]] bool isConnected() const { return connected_; }
    [[nodiscard]] const std::string& getDacName() const { return dac_name_; }

    // 点击事件回调 (例如点击后可直接跳转至 DACSettings 调音页)
    void setOnClick(ClickCallback cb) {
        on_click_ = cb;
    }

private:
    bool connected_ = false;
    std::string dac_name_ = "ES9038PRO";
    ClickCallback on_click_;
};