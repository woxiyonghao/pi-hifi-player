#pragma once

#include "imgui.h"
#include <string>

// ==============================================================================
// 纯音数播 Web 远程遥控与跨设备中枢 (WebServiceView)
// 全平台支持 (Mac / Linux / 树莓派 / iPad / iPhone / Android):
// 1. 本机作为数播主机 (Host Service): 开启 WebSocket 服务，旧手机/平板变废为宝打造发烧数播
// 2. 本机作为遥控端 (Remote Client): 连接并接管局域网内任意数播设备 (树莓派或旧手机)
// ==============================================================================
class WebServiceView {
public:
    WebServiceView();
    ~WebServiceView() = default;

    // 渲染 Web 远程遥控服务管理面板 (位于主舞台中央)
    void render(float x, float y, float w, float h);

private:
    void renderServerCard(ImDrawList* dl, float x0, float y0, float w);
    void renderClientStatsCard(ImDrawList* dl, float x0, float y0, float w);
    void renderRemoteClientConnectCard(ImDrawList* dl, float x0, float y0, float w);
    void renderGuideCard(ImDrawList* dl, float x0, float y0, float w, float h);

    float anim_pulse_ = 0.0f;
    std::string toast_message_;
    float toast_timer_ = 0.0f;

    // 旧手机当数播专项：屏幕常亮防息眠
    bool keep_screen_awake_ = true;

    // 遥控客户端输入：目标数播 IP 与端口
    char target_remote_ip_[64] = "192.168.1.100";
    int target_remote_port_ = 8088;
    std::string ping_result_;
    float ping_timer_ = 0.0f;
};
