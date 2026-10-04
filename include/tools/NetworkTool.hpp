#pragma once

#include <string>

struct NetworkInfo {
    std::string ip = "未连接";
    std::string interface_name = "--";
    std::string mac = "--";
    std::string wifi_ssid = "未连接";
    std::string wifi_signal = "--";
    bool is_connected = false;
};

class NetworkTool {
public:
    // 获取当前网络信息 (带轻量级内存缓存，force_refresh=true 强制重新抓取)
    static NetworkInfo getNetworkInfo(bool force_refresh = false);

    // 触发后台异步刷新 (避免阻塞主渲染帧率)
    static void refreshAsync();

    // 格式化输出简要单行描述
    static std::string getSummaryString();
};
