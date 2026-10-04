#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

// 终端日志行类型
enum class TermLineType {
    Prompt,     // 用户敲击的命令行 ($ command)
    Output,     // 命令正常输出
    Error,      // 错误信息
    SystemInfo  // 系统提示或连接变动
};

struct TermLine {
    std::string text;
    TermLineType type = TermLineType::Output;
};

// ==============================================================================
// 系统内置 Linux 终端控制台 (TerminalView)
// 专为 7 寸触摸屏与发烧纯音系统设计，支持触控快捷指令与 Shell 异步交互
// ==============================================================================
class TerminalView {
public:
    TerminalView();
    ~TerminalView();

    // 渲染主终端面板
    void render(float x, float y, float w, float h);

    // 执行指定命令 (非阻塞异步执行，不卡死 UI 和音频重放)
    void executeCommand(const std::string& cmd);

    // 清屏
    void clear();

private:
    void renderHeader(ImDrawList* dl, float x0, float y0, float w);
    void renderQuickCommands(float x0, float y0, float w);
    void renderConsoleWindow(float x0, float y0, float w, float h);
    void renderInputBar(float x0, float y0, float w);

    void appendLine(const std::string& line, TermLineType type = TermLineType::Output);

private:
    std::vector<TermLine> lines_;
    std::mutex log_mutex_;
    char input_buf_[512] = {0};

    std::atomic<bool> is_running_{false};
    std::string running_cmd_;
    std::thread worker_thread_;

    bool scroll_to_bottom_{false};
};
