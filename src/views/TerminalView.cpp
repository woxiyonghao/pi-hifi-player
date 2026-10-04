#include "views/TerminalView.hpp"
#include "tools/NetworkTool.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "themes/ThemeManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

TerminalView::TerminalView() {
    appendLine("[PiHiFi Terminal] 树莓派发烧级纯音系统 Linux Shell 直通控制台已就绪", TermLineType::SystemInfo);
    appendLine("内核环境：Linux / ARMv8.2-A Direct KMS 架构 · 支持触控与物理键盘输入", TermLineType::SystemInfo);
    NetworkInfo net = NetworkTool::getNetworkInfo();
    if (net.is_connected) {
        appendLine("网络接入：" + net.ip + " (" + net.interface_name + ") · " + net.wifi_ssid, TermLineType::SystemInfo);
    } else {
        appendLine("网络接入：未检测到有效局域网连接，请检查 Wi-Fi 或网线", TermLineType::SystemInfo);
    }
}

TerminalView::~TerminalView() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void TerminalView::clear() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    lines_.clear();
    scroll_to_bottom_ = true;
}

void TerminalView::appendLine(const std::string& line, TermLineType type) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    lines_.push_back({line, type});
    if (lines_.size() > 1000) {
        lines_.erase(lines_.begin(), lines_.begin() + 100);
    }
    scroll_to_bottom_ = true;
}

void TerminalView::executeCommand(const std::string& cmd) {
    if (cmd.empty()) return;

    if (cmd == "clear" || cmd == "cls") {
        clear();
        return;
    }

    if (is_running_) {
        appendLine("[提示] 前序命令正在执行中，请等待其完成后再试: " + running_cmd_, TermLineType::Error);
        return;
    }

    appendLine("$ " + cmd, TermLineType::Prompt);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    is_running_ = true;
    running_cmd_ = cmd;

    worker_thread_ = std::thread([this, cmd]() {
        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
        if (!pipe) {
            appendLine("❌ 无法启动子进程执行该指令", TermLineType::Error);
            is_running_ = false;
            return;
        }

        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            appendLine(line, TermLineType::Output);
        }

        int exit_code = pclose(pipe);
        if (exit_code != 0) {
            appendLine("[进程退出: 返回码 " + std::to_string(exit_code) + "]", TermLineType::SystemInfo);
        }
        is_running_ = false;
        scroll_to_bottom_ = true;
    });
}

void TerminalView::render(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 外部发烧级液态玻璃底板
    GlassCardRenderer::drawCard(dl, ImVec2(x, y), ImVec2(x + w, y + h), 16.0f, "terminal_main");

    float pad_x = 16.0f;
    float pad_y = 14.0f;
    float content_x = x + pad_x;
    float content_w = w - pad_x * 2.0f;

    // 2. 顶部标题栏与实时网络徽章 (约 34px)
    renderHeader(dl, content_x, y + pad_y, content_w);

    // 3. 快捷触控指令栏 (约 32px)
    renderQuickCommands(content_x, y + pad_y + 36.0f, content_w);

    // 4. 底部命令输入框 (固定高 36px，位于底部上方)
    float input_h = 34.0f;
    float input_y = y + h - pad_y - input_h;
    renderInputBar(content_x, input_y, content_w);

    // 5. 中间黑客终端视窗 (自适应高度)
    float console_y = y + pad_y + 36.0f + 36.0f;
    float console_h = input_y - console_y - 10.0f;
    renderConsoleWindow(content_x, console_y, content_w, console_h);
}

void TerminalView::renderHeader(ImDrawList* dl, float x0, float y0, float w) {
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0, y0), UIConfig::Color::TextActive, "系统终端 (Terminal)");
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 155.0f, y0 + 3.0f), UIConfig::Color::TextMuted, "Linux Shell 直通控制台 · 实时网络与运维管控");

    // 右侧实时网络徽章
    NetworkInfo net = NetworkTool::getNetworkInfo();
    std::string net_badge = net.is_connected ? ("IP: " + net.ip + " · " + net.wifi_ssid) : "网络未连接";
    ImVec2 badge_sz = ImGui::CalcTextSize(net_badge.c_str());

    float badge_x = x0 + w - badge_sz.x - 20.0f;
    float badge_y = y0;
    dl->AddRectFilled(ImVec2(badge_x - 8.0f, badge_y), ImVec2(badge_x + badge_sz.x + 8.0f, badge_y + 22.0f),
                      net.is_connected ? IM_COL32(16, 185, 129, 35) : IM_COL32(239, 68, 68, 35), 6.0f);
    dl->AddRect(ImVec2(badge_x - 8.0f, badge_y), ImVec2(badge_x + badge_sz.x + 8.0f, badge_y + 22.0f),
                net.is_connected ? IM_COL32(16, 185, 129, 140) : IM_COL32(239, 68, 68, 140), 6.0f, 0, 1.0f);
    dl->AddText(ImVec2(badge_x, badge_y + 3.0f), net.is_connected ? IM_COL32(52, 211, 153, 255) : IM_COL32(248, 113, 113, 255), net_badge.c_str());
    if (Fonts::Small) ImGui::PopFont();
}

void TerminalView::renderQuickCommands(float x0, float y0, float w) {
    struct QuickCmd {
        const char* label;
        const char* cmd;
    };

    static const QuickCmd cmds[] = {
        { "[IP] 当前IP",     "hostname -I 2>/dev/null || ifconfig" },
        { "[WiFi] WiFi状态", "nmcli -t -f ACTIVE,SSID,SIGNAL,DEVICE dev wifi 2>/dev/null || iwconfig" },
        { "[CPU] CPU温度",   "vcgencmd measure_temp 2>/dev/null || cat /sys/class/thermal/thermal_zone0/temp" },
        { "[磁盘] 磁盘空间",   "df -h /" },
        { "[内存] 内存状态",   "free -h" },
        { "[声卡] ALSA声卡",   "aplay -l" },
        { "[网络] 外网测试",   "ping -c 3 223.5.5.5" },
        { "[清屏] 清屏",       "clear" }
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cur_x = x0;
    float cur_y = y0;
    float btn_h = 24.0f;
    float spacing = 6.0f;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    for (int i = 0; i < 8; ++i) {
        ImVec2 text_sz = ImGui::CalcTextSize(cmds[i].label);
        float btn_w = text_sz.x + 16.0f;

        if (cur_x + btn_w > x0 + w) {
            break; // 防止超出屏幕
        }

        ImVec2 b_min(cur_x, cur_y);
        ImVec2 b_max(cur_x + btn_w, cur_y + btn_h);

        std::string id = std::string("##QuickCmd_") + std::to_string(i);
        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            executeCommand(cmds[i].cmd);
        }

        ImU32 bg = hov ? IM_COL32(255, 255, 255, 45) : IM_COL32(255, 255, 255, 18);
        ImU32 border = hov ? IM_COL32(255, 255, 255, 120) : IM_COL32(255, 255, 255, 35);
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        dl->AddText(ImVec2(cur_x + (btn_w - text_sz.x) * 0.5f, cur_y + (btn_h - text_sz.y) * 0.5f),
                    IM_COL32(230, 238, 250, 240), cmds[i].label);

        cur_x += btn_w + spacing;
    }

    if (Fonts::Small) ImGui::PopFont();
}

void TerminalView::renderConsoleWindow(float x0, float y0, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 终端深色极客黑底
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);
    dl->AddRectFilled(p0, p1, IM_COL32(11, 15, 23, 245), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 25), 10.0f, 0, 1.0f);

    // 内部文字滚动区域
    float pad = 10.0f;
    ImGui::SetCursorScreenPos(ImVec2(x0 + pad, y0 + pad));

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_None;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 40));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);

    if (ImGui::BeginChild("##TerminalConsoleScroll", ImVec2(w - pad * 2.0f, h - pad * 2.0f), false, child_flags)) {
        // 触控屏上下拖拽滑屏
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = std::clamp(ImGui::GetIO().MouseDelta.y, -40.0f, 40.0f);
            if (drag_dy != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
            }
        }

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);

        for (const auto& line : lines_) {
            ImVec4 col;
            switch (line.type) {
                case TermLineType::Prompt:
                    col = ImVec4(0.20f, 0.83f, 0.60f, 1.0f); // 鲜亮绿
                    break;
                case TermLineType::Output:
                    col = ImVec4(0.88f, 0.92f, 0.96f, 1.0f); // 纯净浅灰蓝
                    break;
                case TermLineType::Error:
                    col = ImVec4(0.97f, 0.44f, 0.44f, 1.0f); // 醒目柔红
                    break;
                case TermLineType::SystemInfo:
                    col = ImVec4(0.96f, 0.62f, 0.04f, 1.0f); // 金色提示
                    break;
            }
            ImGui::TextColored(col, "%s", line.text.c_str());
        }

        if (is_running_) {
            ImGui::TextColored(ImVec4(0.38f, 0.65f, 0.98f, 1.0f), "[执行中] 正在执行: %s ...", running_cmd_.c_str());
        }

        if (scroll_to_bottom_) {
            ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom_ = false;
        }

        if (Fonts::Small) ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void TerminalView::renderInputBar(float x0, float y0, float w) {
    float run_btn_w = 70.0f;
    float clear_btn_w = 64.0f;
    float spacing = 8.0f;
    float input_w = w - run_btn_w - clear_btn_w - spacing * 2.0f;
    float bar_h = 32.0f;

    ImGui::SetCursorScreenPos(ImVec2(x0, y0));

    // 输入框样式
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 26, 38, 220));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextActive);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    ImGui::SetNextItemWidth(input_w);
    bool enter_pressed = ImGui::InputTextWithHint("##TerminalInputBox",
                                                  "输入 Shell 指令 (如 ip a, reboot, df -h, ping)...",
                                                  input_buf_, sizeof(input_buf_),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, spacing);

    // 执行按钮
    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    ImGui::PushStyleColor(ImGuiCol_Button, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    bool run_clicked = ImGui::Button("执行", ImVec2(run_btn_w, bar_h));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, spacing);

    // 清屏按钮
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 22));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 60));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    bool clear_clicked = ImGui::Button("清屏", ImVec2(clear_btn_w, bar_h));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (Fonts::Small) ImGui::PopFont();

    if (enter_pressed || run_clicked) {
        std::string cmd(input_buf_);
        // 清理前后空格
        size_t first = cmd.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) {
            size_t last = cmd.find_last_not_of(" \t\r\n");
            cmd = cmd.substr(first, (last - first + 1));
            executeCommand(cmd);
        }
        input_buf_[0] = '\0';
    }

    if (clear_clicked) {
        clear();
    }
}
