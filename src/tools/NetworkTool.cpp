#include "tools/NetworkTool.hpp"
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

namespace {
    std::mutex g_net_mutex;
    NetworkInfo g_cached_info;
    std::chrono::steady_clock::time_point g_last_fetch{};
    bool g_is_fetching = false;

    std::string execCommandQuick(const std::string& cmd) {
        std::string result;
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe) != nullptr) {
            result += buf;
        }
        pclose(pipe);
        // 去除尾部换行
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }

    NetworkInfo fetchRealNetworkInfo() {
        NetworkInfo info;
        struct ifaddrs* ifaddr = nullptr;

        if (getifaddrs(&ifaddr) == 0 && ifaddr != nullptr) {
            struct IfaceCandidate {
                std::string name;
                std::string ip;
                int priority = 0; // 越大约优先: wlan0=100, eth0/end0=80, en0=70, 其他=10
            };
            std::vector<IfaceCandidate> candidates;

            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
                if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP)) continue;

                auto* p = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                char ip_buf[INET_ADDRSTRLEN] = {0};
                if (!inet_ntop(AF_INET, &(p->sin_addr), ip_buf, INET_ADDRSTRLEN)) continue;

                std::string ip_str(ip_buf);
                if (ip_str.rfind("127.", 0) == 0 || ip_str.rfind("169.254.", 0) == 0) continue;
                // 忽略 macOS 内部虚拟通道
                if (ip_str.rfind("28.", 0) == 0) continue;

                std::string iface(ifa->ifa_name);
                int prio = 10;
                if (iface.rfind("wlan", 0) == 0) prio = 100;
                else if (iface.rfind("eth", 0) == 0 || iface.rfind("end", 0) == 0) prio = 80;
                else if (iface == "en0") prio = 70;
                else if (iface.rfind("en", 0) == 0) prio = 50;

                candidates.push_back({iface, ip_str, prio});
            }
            freeifaddrs(ifaddr);

            std::sort(candidates.begin(), candidates.end(), [](const IfaceCandidate& a, const IfaceCandidate& b) {
                return a.priority > b.priority;
            });

            if (!candidates.empty()) {
                info.ip = candidates.front().ip;
                info.interface_name = candidates.front().name;
                info.is_connected = true;
            }
        }

        // 读取 MAC 地址
        if (info.is_connected && !info.interface_name.empty()) {
            std::string sys_mac_path = "/sys/class/net/" + info.interface_name + "/address";
            std::ifstream mac_file(sys_mac_path);
            if (mac_file.is_open()) {
                std::string mac_line;
                if (std::getline(mac_file, mac_line) && !mac_line.empty()) {
                    while (!mac_line.empty() && (mac_line.back() == '\n' || mac_line.back() == '\r')) {
                        mac_line.pop_back();
                    }
                    info.mac = mac_line;
                }
            }
        }

        // 获取 Wi-Fi 连接状态与 SSID
#if defined(__linux__) || defined(HIFI_PLATFORM_RPI)
        // 尝试 nmcli 获取 active wifi
        std::string nmcli_out = execCommandQuick("nmcli -t -f ACTIVE,SSID,SIGNAL dev wifi 2>/dev/null | grep '^yes:' | head -n 1");
        if (!nmcli_out.empty()) {
            // 格式: yes:SSID:85
            std::stringstream ss(nmcli_out);
            std::string token;
            int idx = 0;
            std::string ssid;
            std::string signal;
            while (std::getline(ss, token, ':')) {
                if (idx == 1) ssid = token;
                else if (idx == 2) signal = token;
                idx++;
            }
            if (!ssid.empty()) {
                info.wifi_ssid = ssid;
                info.wifi_signal = signal.empty() ? "已连接" : (signal + "% 信号");
            }
        }
        if (info.wifi_ssid == "未连接" || info.wifi_ssid.empty()) {
            // 尝试 iwgetid
            std::string iw_ssid = execCommandQuick("iwgetid -r 2>/dev/null");
            if (!iw_ssid.empty()) {
                info.wifi_ssid = iw_ssid;
                info.wifi_signal = "已连接";
            }
        }
#else
        // macOS 平台快速读取当前 Wi-Fi
        std::string mac_wifi = execCommandQuick("/System/Library/PrivateFrameworks/Apple80211.framework/Resources/airport -I 2>/dev/null | awk '/ SSID/ {print $2}'");
        if (!mac_wifi.empty()) {
            info.wifi_ssid = mac_wifi;
            info.wifi_signal = "良好";
        }
#endif

        if (info.wifi_ssid == "未连接" && info.is_connected) {
            info.wifi_ssid = "有线直连 (Ethernet)";
            info.wifi_signal = "1000Mbps 全双工";
        }

        return info;
    }
}

NetworkInfo NetworkTool::getNetworkInfo(bool force_refresh) {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_last_fetch).count();

    if (force_refresh || elapsed > 3 || !g_cached_info.is_connected) {
        g_cached_info = fetchRealNetworkInfo();
        g_last_fetch = now;
    }
    return g_cached_info;
}

void NetworkTool::refreshAsync() {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    if (g_is_fetching) return;
    g_is_fetching = true;

    std::thread([]() {
        NetworkInfo res = fetchRealNetworkInfo();
        {
            std::lock_guard<std::mutex> inner_lock(g_net_mutex);
            g_cached_info = res;
            g_last_fetch = std::chrono::steady_clock::now();
            g_is_fetching = false;
        }
    }).detach();
}

std::string NetworkTool::getSummaryString() {
    NetworkInfo info = getNetworkInfo();
    if (!info.is_connected) {
        return "网络未连接";
    }
    return info.ip + " (" + info.interface_name + ") · " + info.wifi_ssid;
}
