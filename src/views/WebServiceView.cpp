#include "views/WebServiceView.hpp"
#include "services/WebService.hpp"
#include "tools/NetworkTool.hpp"
#include "tools/MusicDatabase.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "public/Platform.hpp"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if (!defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE)
#if __has_include(<SDL.h>)
#include <SDL.h>
#define HIFI_HAS_SDL 1
#elif __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#define HIFI_HAS_SDL 1
#endif
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

WebServiceView::WebServiceView() {
    // 默认开启防休眠，确保旧手机或树莓派作为数播时屏幕与后台网络不被系统冻结
#if defined(HIFI_HAS_SDL)
    if (keep_screen_awake_) {
        SDL_DisableScreenSaver();
    }
#endif

    // 如果用户之前主动开启过，则在数播启动时自动拉起服务
    std::string auto_start = MusicDatabase::getInstance().getSetting("setting_web_service_auto_start", "0");
    if (auto_start == "1") {
        WebService::getInstance().start(8088);
    }
}

void WebServiceView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 顶级深空高密度毛玻璃主底板
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "web_service_main");

    // 2. 绘制标题栏「Web 远程遥控与跨设备中枢」
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "Web 远程控制与数播中枢");
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string platform_label = "当前设备: " + Platform::displayName() + " · 支持旧手机/平板当数播主机 · 双向毫秒级遥控";
    dl->AddText(ImVec2(title_pos.x + 200.0f, title_pos.y + 3.0f), UIConfig::Color::TextMuted, platform_label.c_str());
    if (Fonts::Small) ImGui::PopFont();

    anim_pulse_ += ImGui::GetIO().DeltaTime * 3.0f;
    if (toast_timer_ > 0.0f) {
        toast_timer_ -= ImGui::GetIO().DeltaTime;
    }
    if (ping_timer_ > 0.0f) {
        ping_timer_ -= ImGui::GetIO().DeltaTime;
    }

    float content_x = card_min.x + 16.0f;
    float content_y = card_min.y + 46.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    float cur_y = content_y;

    // 板块一：本机数播服务端核心启停控制与网络地址卡片 (高 112px)
    renderServerCard(dl, content_x, cur_y, content_w);
    cur_y += 112.0f + 10.0f;

    // 板块二：实时客户端同步与性能开销看板 (高 86px)
    renderClientStatsCard(dl, content_x, cur_y, content_w);
    cur_y += 86.0f + 10.0f;

    // 板块三：连接远端数播设备 (客户端模式卡片，高 88px)
    renderRemoteClientConnectCard(dl, content_x, cur_y, content_w);
    cur_y += 88.0f + 10.0f;

    // 板块四：旧手机变废为宝当数播与跨设备指南 (自适应高度)
    float guide_h = std::max(60.0f, card_max.y - cur_y - 10.0f);
    renderGuideCard(dl, content_x, cur_y, content_w, guide_h);
}

void WebServiceView::renderServerCard(ImDrawList* dl, float x0, float y0, float w) {
    float h = 112.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    bool running = WebService::getInstance().isRunning();
    NetworkInfo net = NetworkTool::getNetworkInfo();
    int port = WebService::getInstance().getPort();

    // 状态呼吸指示灯
    float state_x = x0 + 16.0f;
    float state_y = y0 + 12.0f;
    ImU32 dot_col = running ? IM_COL32(16, 185, 129, 255) : IM_COL32(148, 163, 184, 255);
    dl->AddCircleFilled(ImVec2(state_x + 5.0f, state_y + 8.0f), 5.0f, dot_col);

    if (running) {
        float glow_r = 7.0f + 2.0f * std::sin(anim_pulse_);
        dl->AddCircle(ImVec2(state_x + 5.0f, state_y + 8.0f), glow_r, IM_COL32(16, 185, 129, 100), 16, 1.5f);
    }

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    const char* status_title = running ? "【本机数播服务运行中】已对外开放遥控端口" : "【本机数播服务已停止】0 线程 0 端口开销";
    dl->AddText(ImVec2(state_x + 18.0f, state_y), running ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted, status_title);
    if (Fonts::Regular) ImGui::PopFont();

    // URL 访问展示
    std::string url_str = net.is_connected ? ("http://" + net.ip + ":" + std::to_string(port)) : "网络未连接 (建议开启Wi-Fi或手机热点)";
    std::string ws_str = net.is_connected ? ("ws://" + net.ip + ":" + std::to_string(port) + "/ws") : "未联网";

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 38.0f), UIConfig::Color::TextMuted, "遥控访问网址：");
    dl->AddText(ImVec2(x0 + 116.0f, y0 + 38.0f), running ? IM_COL32(52, 211, 153, 255) : UIConfig::Color::TextMuted, url_str.c_str());
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string net_detail = "WebSocket 通道: " + ws_str + " · 网络: " + (net.wifi_ssid.empty() ? "局域网/热点" : net.wifi_ssid);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 64.0f), UIConfig::Color::TextMuted, net_detail.c_str());

    // 状态提示或反馈
    if (toast_timer_ > 0.0f) {
        dl->AddText(ImVec2(x0 + 16.0f, y0 + 86.0f), IM_COL32(52, 211, 153, 255), toast_message_.c_str());
    } else {
        const char* tip = running ? "旧手机模式：已防休眠，任意设备打开上方网址即可无感遥控切歌"
                                  : "提示：仅在开启服务后才占用系统端口和后台线程，平时开启 0 损耗";
        dl->AddText(ImVec2(x0 + 16.0f, y0 + 86.0f), UIConfig::Color::TextMuted, tip);
    }
    if (Fonts::Small) ImGui::PopFont();

    // 右侧核心启停控制大按键
    float btn_w = 110.0f;
    float btn_h = 34.0f;
    float btn_x = x0 + w - btn_w - 18.0f;
    float btn_y = y0 + 16.0f;

    ImGui::SetCursorScreenPos(ImVec2(btn_x, btn_y));
    ImGui::InvisibleButton("##WebServiceToggleBtn", ImVec2(btn_w, btn_h));
    bool hov = ImGui::IsItemHovered();

    if (ImGui::IsItemClicked()) {
        if (running) {
            WebService::getInstance().stop();
            MusicDatabase::getInstance().setSetting("setting_web_service_auto_start", "0");
            toast_message_ = "数播服务已停止，后台资源已彻底释放";
            toast_timer_ = 3.0f;
        } else {
            WebService::getInstance().start(8088);
            MusicDatabase::getInstance().setSetting("setting_web_service_auto_start", "1");
            toast_message_ = "数播服务已就绪！用其他设备浏览器访问上方网址即可遥控";
            toast_timer_ = 3.0f;
        }
    }

    ImU32 btn_bg = running ? (hov ? IM_COL32(239, 68, 68, 45) : IM_COL32(239, 68, 68, 25))
                           : (hov ? IM_COL32(16, 185, 129, 45) : IM_COL32(16, 185, 129, 25));
    ImU32 btn_border = running ? (hov ? IM_COL32(239, 68, 68, 180) : IM_COL32(239, 68, 68, 110))
                               : (hov ? IM_COL32(16, 185, 129, 180) : IM_COL32(16, 185, 129, 110));
    ImU32 btn_txt = running ? IM_COL32(248, 113, 113, 255) : IM_COL32(52, 211, 153, 255);

    dl->AddRectFilled(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h), btn_bg, 8.0f);
    dl->AddRect(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h), btn_border, 8.0f, 0, 1.2f);

    const char* btn_label = running ? "停止服务" : "开启服务";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 lbl_sz = ImGui::CalcTextSize(btn_label);
    dl->AddText(ImVec2(btn_x + (btn_w - lbl_sz.x) * 0.5f, btn_y + (btn_h - lbl_sz.y) * 0.5f), btn_txt, btn_label);
    if (Fonts::Regular) ImGui::PopFont();

    // 辅助动作：复制网址 / 浏览器打开
    if (running) {
        float sub_btn_w = 110.0f;
        float sub_btn_h = 26.0f;
        float sub_btn_y = btn_y + btn_h + 8.0f;

        ImGui::SetCursorScreenPos(ImVec2(btn_x, sub_btn_y));
        ImGui::InvisibleButton("##WebServiceCopyBtn", ImVec2(sub_btn_w, sub_btn_h));
        bool sub_hov = ImGui::IsItemHovered();

        if (ImGui::IsItemClicked()) {
            ImGui::SetClipboardText(url_str.c_str());
            toast_message_ = "已复制遥控网址到剪贴板！";
            toast_timer_ = 2.5f;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
            // iOS 沙盒禁止 std::system，已由上方 SetClipboardText 将链接复制至剪贴板
#elif defined(__APPLE__)
            std::string cmd = "open \"" + url_str + "\"";
            (void)std::system(cmd.c_str());
#elif defined(_WIN32)
            std::string cmd = "start \"\" \"" + url_str + "\"";
            (void)std::system(cmd.c_str());
#else
            std::string cmd = "xdg-open \"" + url_str + "\" >/dev/null 2>&1 &";
            (void)std::system(cmd.c_str());
#endif
        }

        ImU32 sub_bg = sub_hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12);
        dl->AddRectFilled(ImVec2(btn_x, sub_btn_y), ImVec2(btn_x + sub_btn_w, sub_btn_y + sub_btn_h), sub_bg, 6.0f);
        dl->AddRect(ImVec2(btn_x, sub_btn_y), ImVec2(btn_x + sub_btn_w, sub_btn_y + sub_btn_h), IM_COL32(255, 255, 255, 30), 6.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        const char* sub_lbl = "打开 / 复制";
        ImVec2 sub_sz = ImGui::CalcTextSize(sub_lbl);
        dl->AddText(ImVec2(btn_x + (sub_btn_w - sub_sz.x) * 0.5f, sub_btn_y + (sub_btn_h - sub_sz.y) * 0.5f),
                    UIConfig::Color::TextNormal, sub_lbl);
        if (Fonts::Small) ImGui::PopFont();
    }
}

void WebServiceView::renderClientStatsCard(ImDrawList* dl, float x0, float y0, float w) {
    float h = 86.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(18, 24, 34, 150), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 15), 10.0f, 0, 1.0f);

    WebServiceStats stats = WebService::getInstance().getStats();

    // 第一列：在线客户端连接数
    float col1_x = x0 + 16.0f;
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(col1_x, y0 + 12.0f), UIConfig::Color::TextMuted, "在线遥控设备");
    if (Fonts::Small) ImGui::PopFont();

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    std::string clients_txt = std::to_string(stats.active_ws_clients) + " 台设备";
    ImU32 clients_col = (stats.active_ws_clients > 0) ? IM_COL32(52, 211, 153, 255) : UIConfig::Color::TextNormal;
    dl->AddText(ImVec2(col1_x, y0 + 32.0f), clients_col, clients_txt.c_str());
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(col1_x, y0 + 60.0f), UIConfig::Color::TextMuted, "WebSocket 长连接保活");
    if (Fonts::Small) ImGui::PopFont();

    // 分隔线 1
    float div1_x = col1_x + 140.0f;
    dl->AddLine(ImVec2(div1_x, y0 + 14.0f), ImVec2(div1_x, y0 + h - 14.0f), IM_COL32(255, 255, 255, 15));

    // 第二列：当前在播曲目与同步
    float col2_x = div1_x + 16.0f;
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(col2_x, y0 + 12.0f), UIConfig::Color::TextMuted, "当前同步在播曲目");
    if (Fonts::Small) ImGui::PopFont();

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    std::string track_txt = stats.current_track_title.empty() ? "暂无正在播放曲目" : stats.current_track_title;
    if (!stats.current_artist.empty()) {
        track_txt += " - " + stats.current_artist;
    }
    dl->AddText(ImVec2(col2_x, y0 + 34.0f), UIConfig::Color::TextActive, track_txt.c_str());
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string play_state = stats.is_playing ? "状态: 正在播放中 · 实时推流" : "状态: 暂停/空闲就绪";
    dl->AddText(ImVec2(col2_x, y0 + 60.0f), UIConfig::Color::TextMuted, play_state.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 分隔线 2
    float div2_x = x0 + w - 180.0f;
    if (div2_x > col2_x + 180.0f) {
        dl->AddLine(ImVec2(div2_x, y0 + 14.0f), ImVec2(div2_x, y0 + h - 14.0f), IM_COL32(255, 255, 255, 15));

        // 第三列：旧手机防休眠常亮状态
        float col3_x = div2_x + 16.0f;
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(col3_x, y0 + 12.0f), UIConfig::Color::TextMuted, "旧手机防休眠模式");
        if (Fonts::Small) ImGui::PopFont();

        if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
        dl->AddText(ImVec2(col3_x, y0 + 34.0f), IM_COL32(52, 211, 153, 255), "已开启 (屏幕常亮)");
        if (Fonts::Regular) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(col3_x, y0 + 60.0f), UIConfig::Color::TextMuted, "网络心跳不中断");
        if (Fonts::Small) ImGui::PopFont();
    }
}

void WebServiceView::renderRemoteClientConnectCard(ImDrawList* dl, float x0, float y0, float w) {
    float h = 88.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(18, 24, 34, 150), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 15), 10.0f, 0, 1.0f);

    float cx = x0 + 16.0f;
    float cy = y0 + 12.0f;

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(cx, cy), UIConfig::Color::TextActive, "【客户端模式】连接局域网内的其他数播设备 (树莓派 / 旧手机数播)");
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(cx, cy + 22.0f), UIConfig::Color::TextMuted, "输入对方设备的 IP 地址，即可在浏览器或当前界面中发起遥控：");
    if (Fonts::Small) ImGui::PopFont();

    float input_x = cx;
    float input_y = cy + 42.0f;
    float input_w = 180.0f;
    float input_h = 28.0f;

    ImGui::SetCursorScreenPos(ImVec2(input_x, input_y));
    ImGui::SetNextItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(15, 21, 30, 220));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 30));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

    ImGui::InputText("##RemoteTargetIp", target_remote_ip_, sizeof(target_remote_ip_));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // 连接按钮
    float btn_connect_x = input_x + input_w + 12.0f;
    float btn_connect_w = 140.0f;
    ImGui::SetCursorScreenPos(ImVec2(btn_connect_x, input_y));
    ImGui::InvisibleButton("##OpenRemoteClientBtn", ImVec2(btn_connect_w, input_h));
    bool hov = ImGui::IsItemHovered();

    if (ImGui::IsItemClicked()) {
        std::string remote_url = "http://" + std::string(target_remote_ip_) + ":" + std::to_string(target_remote_port_);
        ImGui::SetClipboardText(remote_url.c_str());
        toast_message_ = "正在调起系统浏览器连接目标数播: " + remote_url;
        toast_timer_ = 3.0f;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        // iOS 沙盒禁止 std::system，已由上方 SetClipboardText 将链接复制至剪贴板
#elif defined(__APPLE__)
        std::string cmd = "open \"" + remote_url + "\"";
        (void)std::system(cmd.c_str());
#elif defined(_WIN32)
        std::string cmd = "start \"\" \"" + remote_url + "\"";
        (void)std::system(cmd.c_str());
#else
        std::string cmd = "xdg-open \"" + remote_url + "\" >/dev/null 2>&1 &";
        (void)std::system(cmd.c_str());
#endif
    }

    ImU32 btn_col = hov ? IM_COL32(16, 185, 129, 45) : IM_COL32(16, 185, 129, 25);
    dl->AddRectFilled(ImVec2(btn_connect_x, input_y), ImVec2(btn_connect_x + btn_connect_w, input_y + input_h), btn_col, 6.0f);
    dl->AddRect(ImVec2(btn_connect_x, input_y), ImVec2(btn_connect_x + btn_connect_w, input_y + input_h), IM_COL32(16, 185, 129, 140), 6.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    const char* lbl = "在浏览器中接管遥控";
    ImVec2 sz = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(btn_connect_x + (btn_connect_w - sz.x) * 0.5f, input_y + (input_h - sz.y) * 0.5f),
                IM_COL32(52, 211, 153, 255), lbl);
    if (Fonts::Small) ImGui::PopFont();
}

void WebServiceView::renderGuideCard(ImDrawList* dl, float x0, float y0, float w, float h) {
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(18, 24, 34, 150), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 15), 10.0f, 0, 1.0f);

    float cx = x0 + 16.0f;
    float cy = y0 + 12.0f;

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(cx, cy), UIConfig::Color::TextActive, "【旧手机当数播实战秘籍】变废为宝打造发烧纯音转盘");
    if (Fonts::Regular) ImGui::PopFont();
    cy += 24.0f;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    const char* items[] = {
        "1. 硬件连接直通：旧 iPhone / iPad (Lightning/Type-C 转接线) 或 Android (OTG 线) 直接连接外置 USB DAC 或发烧耳放小尾巴。",
        "2. 极致纯净电池供电：旧手机自带锂电池供电，天然隔绝 220V 市电高频杂波，底噪比普通 PC 和劣质开关电源更低！",
        "3. 免路由器热点直连：即使在展会或无 Wi-Fi 环境，旧手机开启「个人热点」，主力机连入即可通过 172.20.10.1:8088 畅快遥控。",
        "4. 免安装 PWA 体验：在主力手机 Safari 或 Chrome 打开网址，点击「添加到主屏幕」，即可秒变全屏独立 App，体验丝滑。"
    };

    for (const char* item : items) {
        if (cy + 16.0f > y0 + h - 6.0f) break;
        dl->AddText(ImVec2(cx, cy), UIConfig::Color::TextMuted, item);
        cy += 18.0f;
    }
    if (Fonts::Small) ImGui::PopFont();
}
