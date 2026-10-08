#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

// ==============================================================================
// 固件与系统在线更新状态枚举
// ==============================================================================
enum class UpdateStatus {
    Idle,            // 就绪待命
    Checking,        // 正在检测远端仓库
    UpToDate,        // 已是最新版本
    UpdateAvailable, // 发现新版本更新
    Updating,        // 正在拉取源码并多核并发编译
    UpdateSuccess,   // 更新与编译成功
    UpdateFailed     // 更新或编译失败
};

// ==============================================================================
// 纯音数播系统固件 OTA 自动更新管理器 (UpdateManager)
// 负责后台异步轮询 Git 远端、抓取提交记录、增量拉取以及 CMake 原生并发重新编译
// ==============================================================================
class UpdateManager {
public:
    static UpdateManager& getInstance();

    // 检查是否有可用更新 (在独立后台工作线程执行，100% 零阻塞 UI 与发烧音频重放)
    void checkForUpdatesAsync();

    // 执行一键拉取并重新编译 (在后台异步线程执行)
    void executeUpdateAsync(std::function<void(bool success, const std::string& msg)> callback = nullptr);

    // 状态查询
    UpdateStatus getStatus() const;
    std::string getStatusText() const;
    std::string getCurrentCommitHash() const;
    std::string getCurrentCommitDate() const;
    std::string getCurrentBranch() const;
    std::string getRemoteCommitHash() const;
    int getNewCommitCount() const;
    std::string getUpdateLog() const;
    std::string getProgressMessage() const;
    std::string getErrorMessage() const;
    float getUpdateProgress() const; // 0.0f ~ 1.0f

    // 触发系统服务重启或应用程序重载
    void restartApplication();

private:
    UpdateManager();
    ~UpdateManager() = default;

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    void refreshLocalVersionInfo();
    static std::string execCommand(const std::string& cmd, int* out_code = nullptr);

private:
    mutable std::mutex mutex_;
    std::atomic<UpdateStatus> status_{UpdateStatus::Idle};
    std::string current_commit_{"Unknown"};
    std::string current_date_{""};
    std::string current_branch_{"dev/lyh"};
    std::string remote_commit_{""};
    int new_commit_count_{0};
    std::string update_log_{""};
    std::string progress_msg_{""};
    std::string error_msg_{""};
    float update_progress_{0.0f};
};
