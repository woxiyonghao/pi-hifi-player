#include "widgets/VirtualKeyboard.hpp"
#include "public/Font.hpp"
#include "public/Platform.hpp"
#include "imgui_internal.h"
#include <algorithm>

VirtualKeyboard& VirtualKeyboard::getInstance() {
    static VirtualKeyboard instance;
    return instance;
}

VirtualKeyboard::VirtualKeyboard() {
    is_visible_ = false;
    was_want_text_input_ = false;
    mode_ = KeyboardMode::Lowercase;
    keyboard_height_ = 205.0f;
}

void VirtualKeyboard::render(float screen_w, float screen_h) {
    ImGuiIO& io = ImGui::GetIO();

    // 树莓派与纯触屏环境下，当文本框获取焦点时自动展开软键盘，失焦自动收起
    if (Platform::isRaspberryPi()) {
        if (io.WantTextInput && !was_want_text_input_) {
            is_visible_ = true;
        } else if (!io.WantTextInput && was_want_text_input_) {
            is_visible_ = false;
        }
    }
    was_want_text_input_ = io.WantTextInput;

    if (!is_visible_) return;

    // 使用 ForegroundDrawList 保证软键盘浮动于所有模态弹窗与页面之上
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float kb_w = std::min(screen_w - 20.0f, 740.0f);
    const float kb_x = (screen_w - kb_w) * 0.5f;
    const float kb_y = screen_h - keyboard_height_ - 8.0f;

    // 1. 绘制键盘底板背景 (深空亚克力磨砂质感 + 细腻发光边框)
    dl->AddRectFilled(
        ImVec2(kb_x, kb_y),
        ImVec2(kb_x + kb_w, kb_y + keyboard_height_),
        IM_COL32(18, 22, 32, 245),
        14.0f
    );
    dl->AddRect(
        ImVec2(kb_x, kb_y),
        ImVec2(kb_x + kb_w, kb_y + keyboard_height_),
        IM_COL32(75, 90, 120, 180),
        14.0f, 0, 1.2f
    );

    // 2. 顶部工具栏 (标题与状态提示)
    const float title_h = 24.0f;
    std::string mode_text = "触控软键盘 - ";
    if (mode_ == KeyboardMode::Lowercase) mode_text += "小写字母 [abc]";
    else if (mode_ == KeyboardMode::Uppercase) mode_text += "大写字母 [ABC]";
    else mode_text += "数字与标点 [123/符号]";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(
        ImVec2(kb_x + 14.0f, kb_y + 5.0f),
        IM_COL32(140, 160, 195, 220),
        mode_text.c_str()
    );
    if (Fonts::Small) ImGui::PopFont();

    // 3. 组织 5 行按键定义
    std::vector<std::vector<KeyDef>> rows;
    rows.resize(5);

    // 第 1 行：数字键 (1 ~ 0)
    const char* row0_chars[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    for (const char* ch : row0_chars) {
        rows[0].push_back({ch, ch, 1.0f, KeyDef::SpecialType::None});
    }

    // 第 2 行：字母上排 (QWERTY) 或 常用符号
    if (mode_ == KeyboardMode::Symbols) {
        const char* sym1[] = {"-", "/", ":", ";", "(", ")", "$", "&", "@", "\""};
        for (const char* ch : sym1) rows[1].push_back({ch, ch, 1.0f, KeyDef::SpecialType::None});
    } else {
        const char* qwer[] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
        for (const char* ch : qwer) {
            std::string label = (mode_ == KeyboardMode::Uppercase) ? std::string(1, std::toupper(ch[0])) : ch;
            rows[1].push_back({label, label, 1.0f, KeyDef::SpecialType::None});
        }
    }

    // 第 3 行：字母中排 (ASDFGHJKL) 或 标点符号
    if (mode_ == KeyboardMode::Symbols) {
        const char* sym2[] = {".", ",", "?", "!", "'", "_", "+", "=", "*", "#"};
        for (const char* ch : sym2) rows[2].push_back({ch, ch, 1.0f, KeyDef::SpecialType::None});
    } else {
        const char* asdf[] = {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
        for (const char* ch : asdf) {
            std::string label = (mode_ == KeyboardMode::Uppercase) ? std::string(1, std::toupper(ch[0])) : ch;
            rows[2].push_back({label, label, 1.0f, KeyDef::SpecialType::None});
        }
    }

    // 第 4 行：Shift / 符号切换 + 字母下排 + 退格
    if (mode_ == KeyboardMode::Symbols) {
        rows[3].push_back({"ABC", "", 1.4f, KeyDef::SpecialType::ModeToggle});
        const char* sym3[] = {"~", "%", "^", "\\", "|", "<", ">"};
        for (const char* ch : sym3) rows[3].push_back({ch, ch, 1.0f, KeyDef::SpecialType::None});
        rows[3].push_back({"退格", "", 1.5f, KeyDef::SpecialType::Backspace});
    } else {
        std::string shift_lbl = (mode_ == KeyboardMode::Uppercase) ? "大写" : "小写";
        rows[3].push_back({shift_lbl, "", 1.4f, KeyDef::SpecialType::Shift});
        const char* zxcv[] = {"z", "x", "c", "v", "b", "n", "m"};
        for (const char* ch : zxcv) {
            std::string label = (mode_ == KeyboardMode::Uppercase) ? std::string(1, std::toupper(ch[0])) : ch;
            rows[3].push_back({label, label, 1.0f, KeyDef::SpecialType::None});
        }
        rows[3].push_back({"退格", "", 1.5f, KeyDef::SpecialType::Backspace});
    }

    // 第 5 行：123/ABC 模式切换 + 空格 + 确认 + 收起
    if (mode_ == KeyboardMode::Symbols) {
        rows[4].push_back({"ABC", "", 1.5f, KeyDef::SpecialType::ModeToggle});
    } else {
        rows[4].push_back({"123/符", "", 1.5f, KeyDef::SpecialType::ModeToggle});
    }
    rows[4].push_back({"空 格", " ", 4.8f, KeyDef::SpecialType::Space});
    rows[4].push_back({"确认", "", 1.8f, KeyDef::SpecialType::Enter});
    rows[4].push_back({"收起", "", 1.3f, KeyDef::SpecialType::Close});

    // 4. 逐行计算坐标并渲染按键
    const float content_y = kb_y + title_h + 4.0f;
    const float available_h = keyboard_height_ - title_h - 12.0f;
    const float row_gap = 4.0f;
    const float key_h = (available_h - row_gap * 4.0f) / 5.0f;
    const float col_gap = 4.0f;
    const float margin_x = 8.0f;
    const float available_w = kb_w - margin_x * 2.0f;

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        float total_weight = 0.0f;
        for (const auto& k : row) total_weight += k.width_weight;

        const float total_gaps = static_cast<float>(row.size() - 1) * col_gap;
        const float unit_w = (available_w - total_gaps) / total_weight;

        float curr_x = kb_x + margin_x;
        const float curr_y = content_y + static_cast<float>(r) * (key_h + row_gap);

        for (const auto& k : row) {
            const float kw = k.width_weight * unit_w;
            ImVec2 k_min(curr_x, curr_y);
            ImVec2 k_max(curr_x + kw, curr_y + key_h);

            drawKey(dl, k, k_min, k_max, io);

            curr_x += kw + col_gap;
        }
    }
}

void VirtualKeyboard::drawKey(ImDrawList* dl, const KeyDef& key, const ImVec2& min_pos, const ImVec2& max_pos, ImGuiIO& io) {
    ImVec2 mouse = io.MousePos;
    bool is_hover = (mouse.x >= min_pos.x && mouse.x <= max_pos.x && mouse.y >= min_pos.y && mouse.y <= max_pos.y);
    bool is_down = is_hover && io.MouseDown[0];
    bool is_clicked = is_hover && io.MouseClicked[0];

    // 默认按键底色
    ImU32 bg_color = IM_COL32(36, 44, 58, 225);
    ImU32 border_color = IM_COL32(65, 80, 108, 160);
    ImU32 text_color = IM_COL32(240, 245, 255, 255);

    // 功能按键差异化样式
    if (key.type == KeyDef::SpecialType::Enter) {
        bg_color = IM_COL32(30, 110, 200, 230);
        border_color = IM_COL32(70, 160, 255, 220);
        text_color = IM_COL32(255, 255, 255, 255);
    } else if (key.type == KeyDef::SpecialType::Shift && mode_ == KeyboardMode::Uppercase) {
        bg_color = IM_COL32(55, 95, 160, 240);
        border_color = IM_COL32(110, 165, 255, 220);
    } else if (key.type != KeyDef::SpecialType::None && key.type != KeyDef::SpecialType::Space) {
        bg_color = IM_COL32(30, 36, 48, 230);
    }

    if (is_down) {
        bg_color = IM_COL32(80, 115, 170, 255);
        border_color = IM_COL32(130, 180, 255, 255);
    } else if (is_hover) {
        bg_color = IM_COL32(48, 60, 80, 240);
        border_color = IM_COL32(95, 125, 175, 200);
    }

    dl->AddRectFilled(min_pos, max_pos, bg_color, 6.0f);
    dl->AddRect(min_pos, max_pos, border_color, 6.0f, 0, 1.0f);

    // 绘制按键文字 (居中排版)
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 text_size = ImGui::CalcTextSize(key.label.c_str());
    ImVec2 text_pos(
        min_pos.x + (max_pos.x - min_pos.x - text_size.x) * 0.5f,
        min_pos.y + (max_pos.y - min_pos.y - text_size.y) * 0.5f
    );
    dl->AddText(text_pos, text_color, key.label.c_str());
    if (Fonts::Regular) ImGui::PopFont();

    // 触发点击逻辑 (直接操作 ImGuiIO，绝不扰乱当前激活的 InputText 焦点)
    if (is_clicked) {
        switch (key.type) {
            case KeyDef::SpecialType::Shift:
                mode_ = (mode_ == KeyboardMode::Lowercase) ? KeyboardMode::Uppercase : KeyboardMode::Lowercase;
                break;
            case KeyDef::SpecialType::ModeToggle:
                mode_ = (mode_ == KeyboardMode::Symbols) ? KeyboardMode::Lowercase : KeyboardMode::Symbols;
                break;
            case KeyDef::SpecialType::Backspace:
                io.AddKeyEvent(ImGuiKey_Backspace, true);
                io.AddKeyEvent(ImGuiKey_Backspace, false);
                break;
            case KeyDef::SpecialType::Space:
                io.AddInputCharactersUTF8(" ");
                break;
            case KeyDef::SpecialType::Enter:
                io.AddKeyEvent(ImGuiKey_Enter, true);
                io.AddKeyEvent(ImGuiKey_Enter, false);
                break;
            case KeyDef::SpecialType::Close:
                is_visible_ = false;
                ImGui::ClearActiveID();
                break;
            case KeyDef::SpecialType::None:
                if (!key.commit_char.empty()) {
                    io.AddInputCharactersUTF8(key.commit_char.c_str());
                }
                break;
        }
    }
}
