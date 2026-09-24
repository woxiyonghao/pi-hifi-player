#pragma once

#include "types/MusicModel.hpp"
#include "types/SupportedFormats.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <filesystem>
#include <mutex>
#include <optional>

// ==============================================================================
// 1. 扫描状态枚举
// ==============================================================================
enum class ScanState {
    Idle,       // 空闲就绪
    Scanning,   // 正在扫描中
    Completed,  // 扫描圆满完成
    Cancelled,  // 扫描被用户手动中断
    Failed      // 扫描失败 (路径不存在或不可读)
};

// ==============================================================================
// 2. 实时扫描进度数据包 (供 UI 刷新进度条与当前探测路径)
// ==============================================================================
struct ScanProgress {
    std::string current_file;   // 当前探测到的文件或目录名
    size_t found_count = 0;     // 已发现的有效发烧音轨数
    ScanState state = ScanState::Idle;
};

// ==============================================================================
// 3. 本地发烧曲库扫描管理中心 (单例)
// ==============================================================================
class MusicScanManager {
public:
    using ProgressCallback = std::function<void(const ScanProgress&)>;
    using CompleteCallback = std::function<void(const std::vector<Track>&)>;

    // 全局唯一访问入口
    static MusicScanManager& getInstance() {
        static MusicScanManager instance;
        return instance;
    }

    // 禁用拷贝与移动
    MusicScanManager(const MusicScanManager&) = delete;
    MusicScanManager& operator=(const MusicScanManager&) = delete;
    MusicScanManager(MusicScanManager&&) = delete;
    MusicScanManager& operator=(MusicScanManager&&) = delete;

    ~MusicScanManager();

    // -------------------------------------------------------------------------
    // 异步扫描控制核心接口
    // -------------------------------------------------------------------------
    // 启动非阻塞扫描任务 (若正在扫描中则返回 false)
    bool startScan(const std::filesystem::path& root_path);

    // 请求中止扫描 (利用 C++20 stop_token 协作取消)
    void cancelScan();

    // 清空上次扫描数据
    void clear();

    // -------------------------------------------------------------------------
    // 状态查询接口 (线程安全)
    // -------------------------------------------------------------------------
    bool isScanning() const { return state_ == ScanState::Scanning; }
    ScanState getState() const { return state_; }
    size_t getFoundCount() const;
    std::vector<Track> getScannedTracks() const;

    // -------------------------------------------------------------------------
    // 回调注册接口
    // -------------------------------------------------------------------------
    void setProgressCallback(ProgressCallback cb) { progress_callback_ = std::move(cb); }
    void setCompleteCallback(CompleteCallback cb) { complete_callback_ = std::move(cb); }

private:
    MusicScanManager() = default;

    // 后台工作线程函数 (利用 C++20 stop_token 协作式安全退出)
    void scanWorker(std::stop_token stop_token, std::filesystem::path root_path);

    // 辅助工具：从文件名和路径智能解析基础元数据 (如 "蔡琴 - 渡口.flac")
    static Track parseBasicMetadata(uint64_t id, const std::filesystem::directory_entry& entry, AudioFormat format);

private:
    std::atomic<ScanState> state_{ScanState::Idle};
    std::jthread worker_thread_;

    mutable std::mutex mutex_;
    std::vector<Track> scanned_tracks_;

    ProgressCallback progress_callback_;
    CompleteCallback complete_callback_;
};