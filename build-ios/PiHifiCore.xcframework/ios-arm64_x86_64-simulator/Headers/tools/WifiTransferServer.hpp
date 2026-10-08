#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <cstdint>

// 传输进度状态快照模型
struct TransferProgress {
    bool is_running = false;
    bool is_uploading = false;
    std::string current_filename;
    uint64_t bytes_received = 0;
    uint64_t total_bytes = 0;
    float speed_mbps = 0.0f;
    int completed_count = 0;
    std::vector<std::pair<std::string, uint64_t>> completed_files; // 文件名与大小(字节)
    std::string error_message;
};

// ==============================================================================
// 纯音数播 WiFi 局域网高速无线传歌服务端 (WifiTransferServer)
// 单例模式运行，支持后台持续流式接收、H5 拖拽上传单页、进度实时上报与自动曲库入库
// ==============================================================================
class WifiTransferServer {
public:
    static WifiTransferServer& getInstance();

    // 启动/停止 HTTP 传歌服务
    bool start(int port = 8080, const std::string& music_dir = "");
    void stop();

    bool isRunning() const { return is_running_.load(); }
    int getPort() const { return port_; }
    std::string getTargetDir() const { return target_music_dir_; }

    // 获取当前传输进度快照 (UI 线程安全)
    TransferProgress getProgress();

    // 注册歌曲上传完成回调 (用于自动触发 MusicScanManager 扫描)
    using FileReceivedCallback = std::function<void(const std::string& filepath)>;
    void setOnFileReceivedCallback(FileReceivedCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        on_file_received_ = std::move(cb);
    }

private:
    WifiTransferServer();
    ~WifiTransferServer();

    WifiTransferServer(const WifiTransferServer&) = delete;
    WifiTransferServer& operator=(const WifiTransferServer&) = delete;

    void serverLoop();
    void handleClient(int client_fd);
    void registerClient(int fd);
    void unregisterClient(int fd);
    static std::string urlDecode(const std::string& in);
    static std::string sanitizeFilename(const std::string& in);

    std::atomic<bool> is_running_{false};
    int port_ = 8080;
    int server_fd_ = -1;
    std::string target_music_dir_;

    std::mutex clients_mutex_;
    std::vector<int> active_clients_;

    std::thread worker_thread_;
    std::mutex mutex_;
    TransferProgress progress_;
    FileReceivedCallback on_file_received_;
};
