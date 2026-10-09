#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

// Web 远程控制服务运行统计状态快照
struct WebServiceStats {
    bool is_running = false;
    int port = 8088;
    int active_ws_clients = 0;
    std::string current_track_title;
    std::string current_artist;
    bool is_playing = false;
    float volume = 0.8f;
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
};

// ==============================================================================
// 纯音数播 Web 远程遥控服务 (WebService)
// 包含全平台通用的 HTTP 页面托管 + RFC 6455 原生 WebSocket 双向毫秒级状态同步
// 单例运行，仅在用户主动开启时创建套接字与线程，关闭时彻底释放资源零开销
// ==============================================================================
class WebService {
public:
    static WebService& getInstance();

    // 开启与停止 Web 远程控制与 WebSocket 服务 (按需启停，杜绝后台空转)
    bool start(int port = 8088);
    void stop();

    bool isRunning() const { return is_running_.load(); }
    int getPort() const { return port_; }
    int getConnectedClientsCount();

    WebServiceStats getStats();

    // 广播当前最新播放状态给所有已连接的 WebSocket 客户端
    void broadcastState();

private:
    WebService();
    ~WebService();

    WebService(const WebService&) = delete;
    WebService& operator=(const WebService&) = delete;

    void serverLoop();
    void broadcastLoop();
    void handleClient(int client_fd);
    void handleWebSocket(int client_fd, const std::string& key);

    bool sendWsTextFrame(int fd, const std::string& text);
    void broadcastWsText(const std::string& text);

    std::string buildStateJson();
    std::string buildQueueJson();
    std::string buildPlaylistsJson();
    std::string buildLibraryJson();
    std::string buildDacJson();
    std::string buildMsebJson();

    std::atomic<bool> is_running_{false};
    int port_ = 8088;
    int server_fd_ = -1;
    std::atomic<int64_t> sleep_timer_target_sec_{0};

    std::thread server_thread_;
    std::thread broadcast_thread_;

    std::mutex clients_mutex_;
    std::mutex send_mutex_;
    std::vector<int> ws_clients_;

    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
};
