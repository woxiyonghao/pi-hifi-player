#include "views/WifiTransferView.hpp"
#include "tools/WifiTransferServer.hpp"
#include "tools/NetworkTool.hpp"
#include "tools/MusicScanManager.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "public/Platform.hpp"
#include <cmath>
#include <cstdio>

namespace {

inline std::string formatFileSize(uint64_t bytes) {
    if (bytes == 0) return "0 B";
    const char* units[] = { "B", "KB", "MB", "GB" };
    int idx = 0;
    double d = static_cast<double>(bytes);
    while (d >= 1024.0 && idx < 3) {
        d /= 1024.0;
        idx++;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f %s", d, units[idx]);
    return std::string(buf);
}

} // namespace

WifiTransferView::WifiTransferView() {
}

void WifiTransferView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 顶级深空高密度毛玻璃主卡片底板
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "wifi_transfer_main");

    // 2. 绘制标题栏「WiFi 局域网传歌」
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "WiFi 局域网传歌");
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(title_pos.x + 160.0f, title_pos.y + 3.0f), UIConfig::Color::TextMuted,
                "同一局域网下电脑或手机免线拖拽传输 · 即传即播 · 自动扫描入库");
    if (Fonts::Small) ImGui::PopFont();

    anim_pulse_ += ImGui::GetIO().DeltaTime * 3.0f;

    float content_x = card_min.x + 16.0f;
    float content_y = card_min.y + 46.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    float cur_y = content_y;

    // 板块一：服务端状态与启停控制卡片 (紧凑高 96px)
    renderServerCard(dl, content_x, cur_y, content_w);
    cur_y += 96.0f + 10.0f;

    // 板块二：实时传输进度看板 (紧凑高 84px)
    renderLiveProgressCard(dl, content_x, cur_y, content_w);
    cur_y += 84.0f + 10.0f;

    // 板块三：已接收曲目历史清单看板 (自适应填满剩余高度，无外层滚动条)
    float history_h = std::max(60.0f, card_max.y - cur_y - 12.0f);
    renderHistoryCard(dl, content_x, cur_y, content_w, history_h);
}

void WifiTransferView::renderServerCard(ImDrawList* dl, float x0, float y0, float w) {
    float h = 96.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    bool running = WifiTransferServer::getInstance().isRunning();
    NetworkInfo net = NetworkTool::getNetworkInfo();
    int port = WifiTransferServer::getInstance().getPort();

    // 状态标识
    float state_x = x0 + 16.0f;
    float state_y = y0 + 12.0f;
    ImU32 dot_col = running ? IM_COL32(16, 185, 129, 255) : IM_COL32(148, 163, 184, 255);
    dl->AddCircleFilled(ImVec2(state_x + 5.0f, state_y + 8.0f), 5.0f, dot_col);

    if (running) {
        float glow_r = 7.0f + 2.0f * std::sin(anim_pulse_);
        dl->AddCircle(ImVec2(state_x + 5.0f, state_y + 8.0f), glow_r, IM_COL32(16, 185, 129, 100), 16, 1.5f);
    }

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    const char* status_title = running ? "无线传歌服务正在运行中" : "无线传歌服务已停止";
    dl->AddText(ImVec2(state_x + 18.0f, state_y), running ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted, status_title);
    if (Fonts::Regular) ImGui::PopFont();

    // URL 访问展示
    std::string url_str = net.is_connected ? ("http://" + net.ip + ":" + std::to_string(port)) : "网络未连接";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 38.0f), UIConfig::Color::TextMuted, "传歌网址：");
    dl->AddText(ImVec2(x0 + 92.0f, y0 + 38.0f), running ? IM_COL32(52, 211, 153, 255) : UIConfig::Color::TextMuted, url_str.c_str());
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string display_dir = WifiTransferServer::getInstance().getTargetDir();
    if (Platform::isIOS()) {
        display_dir = "App 文件 / Documents / music";
    }
    std::string env_str = "Wi-Fi: " + net.wifi_ssid + " · 存储: " + display_dir;
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 68.0f), UIConfig::Color::TextMuted, env_str.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 右侧核心启停控制大按键
    float btn_w = 120.0f;
    float btn_h = 36.0f;
    float btn_x = x0 + w - btn_w - 18.0f;
    float btn_y = y0 + 30.0f;

    ImGui::SetCursorScreenPos(ImVec2(btn_x, btn_y));
    ImGui::InvisibleButton("##WifiTransferToggleBtn", ImVec2(btn_w, btn_h));
    bool hov = ImGui::IsItemHovered();

    if (ImGui::IsItemClicked()) {
        if (running) {
            WifiTransferServer::getInstance().stop();
        } else {
            WifiTransferServer::getInstance().start(8080);
        }
    }

    ImU32 btn_bg = running ? (hov ? IM_COL32(239, 68, 68, 45) : IM_COL32(239, 68, 68, 25))
                           : (hov ? IM_COL32(16, 185, 129, 45) : IM_COL32(16, 185, 129, 25));
    ImU32 btn_border = running ? (hov ? IM_COL32(239, 68, 68, 180) : IM_COL32(239, 68, 68, 110))
                               : (hov ? IM_COL32(16, 185, 129, 180) : IM_COL32(16, 185, 129, 110));
    ImU32 btn_txt = running ? IM_COL32(248, 113, 113, 255) : IM_COL32(52, 211, 153, 255);

    dl->AddRectFilled(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h), btn_bg, 8.0f);
    dl->AddRect(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h), btn_border, 8.0f, 0, 1.2f);

    const char* btn_label = running ? "停止传歌" : "开始传歌";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 lbl_sz = ImGui::CalcTextSize(btn_label);
    dl->AddText(ImVec2(btn_x + (btn_w - lbl_sz.x) * 0.5f, btn_y + (btn_h - lbl_sz.y) * 0.5f), btn_txt, btn_label);
    if (Fonts::Regular) ImGui::PopFont();
}

void WifiTransferView::renderLiveProgressCard(ImDrawList* dl, float x0, float y0, float w) {
    float h = 84.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    TransferProgress prog = WifiTransferServer::getInstance().getProgress();

    ImU32 bg_col = prog.is_uploading ? IM_COL32(16, 36, 30, 200) : IM_COL32(20, 26, 36, 175);
    ImU32 border_col = prog.is_uploading ? IM_COL32(16, 185, 129, 120) : IM_COL32(255, 255, 255, 20);

    dl->AddRectFilled(p0, p1, bg_col, 10.0f);
    dl->AddRect(p0, p1, border_col, 10.0f, 0, 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "实时传输看板");
    if (Fonts::Regular) ImGui::PopFont();

    if (prog.is_uploading) {
        // 正在流式接收文件
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        std::string fn_str = "[正在接收] " + prog.current_filename;
        dl->AddText(ImVec2(x0 + 16.0f, y0 + 34.0f), IM_COL32(52, 211, 153, 255), fn_str.c_str());

        // 进度条渲染
        float bar_x = x0 + 16.0f;
        float bar_y = y0 + 54.0f;
        float bar_w = w - 32.0f;
        float bar_h = 8.0f;

        float ratio = (prog.total_bytes > 0) ? (static_cast<float>(prog.bytes_received) / static_cast<float>(prog.total_bytes)) : 0.0f;
        ratio = std::clamp(ratio, 0.0f, 1.0f);

        dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w, bar_y + bar_h), IM_COL32(15, 23, 42, 255), 4.0f);
        if (ratio > 0.001f) {
            dl->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_w * ratio, bar_y + bar_h), IM_COL32(16, 185, 129, 255), 4.0f);
        }

        // 详细指标描述
        char stats_buf[128];
        int pct = static_cast<int>(ratio * 100.0f);
        std::snprintf(stats_buf, sizeof(stats_buf), "进度: %d%% · 已接收: %s / %s · 速率: %.1f MB/s",
                      pct,
                      formatFileSize(prog.bytes_received).c_str(),
                      formatFileSize(prog.total_bytes).c_str(),
                      prog.speed_mbps);
        dl->AddText(ImVec2(bar_x, bar_y + 12.0f), UIConfig::Color::TextMuted, stats_buf);
        if (Fonts::Small) ImGui::PopFont();
    } else {
        // 空闲等待中
        bool running = WifiTransferServer::getInstance().isRunning();
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        if (running) {
            dl->AddText(ImVec2(x0 + 16.0f, y0 + 36.0f), UIConfig::Color::TextNormal,
                        "等待电脑或手机接入上传中... 随时可在网页端批量拖拽音频文件传输");
            dl->AddText(ImVec2(x0 + 16.0f, y0 + 58.0f), UIConfig::Color::TextMuted,
                        "支持格式: FLAC, WAV, DSF, DFF, APE, MP3, M4A, AAC, OGG (传输完成自动入库)");
        } else {
            dl->AddText(ImVec2(x0 + 16.0f, y0 + 36.0f), UIConfig::Color::TextMuted,
                        "WiFi 传歌服务尚未启动，启动后同一局域网设备可免插拔 U 盘秒级传输歌曲");
            dl->AddText(ImVec2(x0 + 16.0f, y0 + 58.0f), UIConfig::Color::TextMuted,
                        "传输完成后曲库将自动同步刷新，无需重启或重新扫描");
        }
        if (Fonts::Small) ImGui::PopFont();
    }
}

void WifiTransferView::renderHistoryCard(ImDrawList* dl, float x0, float y0, float w, float h) {
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);

    TransferProgress prog = WifiTransferServer::getInstance().getProgress();

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    char hdr_buf[64];
    std::snprintf(hdr_buf, sizeof(hdr_buf), "本次传输清单 (已自动入库: %d 首)", prog.completed_count);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, hdr_buf);
    if (Fonts::Regular) ImGui::PopFont();

    float list_y = y0 + 36.0f;
    float list_h = std::max(h - 44.0f, 30.0f);

    ImGui::SetCursorScreenPos(ImVec2(x0 + 12.0f, list_y));
    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    if (ImGui::BeginChild("##HistoryItemsList", ImVec2(w - 24.0f, list_h), false, child_flags)) {

        // 触控拖拽平滑滑动
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = std::clamp(ImGui::GetIO().MouseDelta.y, -40.0f, 40.0f);
            if (drag_dy != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
            }
        }

        if (prog.completed_files.empty()) {
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.65f, 1.0f), "暂无通过 WiFi 接收的音乐文件记录");
            if (Fonts::Small) ImGui::PopFont();
        } else {
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            for (size_t i = 0; i < prog.completed_files.size(); ++i) {
                const auto& item = prog.completed_files[prog.completed_files.size() - 1 - i];
                ImGui::PushID(static_cast<int>(i));

                ImVec2 row_pos = ImGui::GetCursorScreenPos();
                float row_w = ImGui::GetContentRegionAvail().x;
                float row_h = 24.0f;

                ImDrawList* cdl = ImGui::GetWindowDrawList();
                cdl->AddRectFilled(row_pos, ImVec2(row_pos.x + row_w, row_pos.y + row_h),
                                  (i % 2 == 0) ? IM_COL32(255, 255, 255, 6) : IM_COL32(0, 0, 0, 0), 4.0f);

                // 文件名
                cdl->AddText(ImVec2(row_pos.x + 8.0f, row_pos.y + 4.0f), UIConfig::Color::TextActive, item.first.c_str());

                // 大小与入库标记
                std::string size_str = formatFileSize(item.second) + "  ·  已入库";
                ImVec2 sz_dim = ImGui::CalcTextSize(size_str.c_str());
                cdl->AddText(ImVec2(row_pos.x + row_w - sz_dim.x - 8.0f, row_pos.y + 4.0f), IM_COL32(52, 211, 153, 255), size_str.c_str());

                ImGui::Dummy(ImVec2(row_w, row_h));
                ImGui::PopID();
            }
            if (Fonts::Small) ImGui::PopFont();
        }
    }
    ImGui::EndChild();
}
