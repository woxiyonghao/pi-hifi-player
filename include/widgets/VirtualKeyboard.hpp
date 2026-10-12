#pragma once

#include "imgui.h"
#include <string>
#include <vector>

// ==============================================================================
// 发烧级触控屏幕软键盘 (VirtualKeyboard)
// 专为树莓派触摸屏 (Waveshare 7寸等)、触屏终端与嵌入式 Linux 设计
// 特点：
//   1. 基于 ImDrawList 直接绘制，敲击按键绝不抢夺 InputText 激活焦点 (光标持续闪烁输入)
//   2. 支持 QWERTY 字母、大小写切换 (Shift)、数字标点模式 (123/Symbols)、空格、退格与回车
//   3. 在树莓派或触屏环境下当 InputText 获取焦点时自动弹出，失焦或点击关闭时顺畅收起
// ==============================================================================
class VirtualKeyboard {
public:
    static VirtualKeyboard& getInstance();

    // 渲染软键盘 (在帧渲染最顶层调用)
    void render(float screen_w, float screen_h);

    // 显隐控制
    bool isVisible() const { return is_visible_; }
    void setVisible(bool visible) { is_visible_ = visible; }
    void toggle() { is_visible_ = !is_visible_; }

    // 软键盘占用的屏幕垂直高度 (用于上层弹窗自适应向上避让)
    float getHeight() const { return is_visible_ ? keyboard_height_ : 0.0f; }

private:
    VirtualKeyboard();
    ~VirtualKeyboard() = default;

    VirtualKeyboard(const VirtualKeyboard&) = delete;
    VirtualKeyboard& operator=(const VirtualKeyboard&) = delete;

    enum class KeyboardMode {
        Lowercase,
        Uppercase,
        Symbols
    };

    struct KeyDef {
        std::string label;
        std::string commit_char;
        float width_weight = 1.0f; // 相对标准按键宽度的倍数 (如空格为 4.0，Shift 为 1.5)
        enum class SpecialType {
            None,
            Shift,
            Backspace,
            ModeToggle,
            Space,
            Enter,
            Close
        } type = SpecialType::None;
    };

    void drawKey(ImDrawList* dl, const KeyDef& key, const ImVec2& min_pos, const ImVec2& max_pos, ImGuiIO& io);

private:
    bool is_visible_ = false;
    bool was_want_text_input_ = false;
    KeyboardMode mode_ = KeyboardMode::Lowercase;
    float keyboard_height_ = 205.0f;
};
