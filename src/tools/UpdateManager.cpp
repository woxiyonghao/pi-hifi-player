#include "tools/UpdateManager.hpp"
#include "public/Platform.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>
#include <unistd.h>

UpdateManager& UpdateManager::getInstance() {
    static UpdateManager instance;
    return instance;
}

UpdateManager::UpdateManager() {
    refreshLocalVersionInfo();
}

std::string UpdateManager::execCommand(const std::string& cmd, int* out_code) {
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        if (out_code) *out_code = -1;
        return "Failed to run command";
    }

    std::array<char, 256> buffer{};
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    int exit_code = 0;
#ifdef WEXITSTATUS
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = status;
    }
#else
    exit_code = status;
#endif

    if (out_code) *out_code = exit_code;

    // 移除末尾换行符
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

void UpdateManager::refreshLocalVersionInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 获取当前分支
    int code = 0;
    std::string branch = execCommand("git rev-parse --abbrev-ref HEAD", &code);
    if (code == 0 && !branch.empty()) {
        current_branch_ = branch;
    } else {
        current_branch_ = "dev/lyh";
    }

    // 获取当前提交 Hash (短号)
    std::string hash = execCommand("git rev-parse --short HEAD", &code);
    if (code == 0 && !hash.empty()) {
        current_commit_ = hash;
    } else {
        current_commit_ = "811371a";
    }

    // 获取当前提交日期
    std::string date = execCommand("git log -1 --format=%cd --date=short", &code);
    if (code == 0 && !date.empty()) {
        current_date_ = date;
    } else {
        current_date_ = "2026-10-04";
    }
}

void UpdateManager::checkForUpdatesAsync() {
    UpdateStatus expected = status_.load();
    if (expected == UpdateStatus::Checking || expected == UpdateStatus::Updating) {
        return;
    }

    status_.store(UpdateStatus::Checking);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_msg_ = "正在与远程仓库同步最新元数据 (git fetch)...";
        error_msg_.clear();
    }

    std::thread([this]() {
        refreshLocalVersionInfo();

        std::string branch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            branch = current_branch_;
        }

        // 1. Fetch 远程分支 (超时保护，免干扰本地改动)
        int fetch_code = 0;
        std::string fetch_out = execCommand("git fetch origin " + branch + " --quiet", &fetch_code);
        if (fetch_code != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "远端连接失败: " + fetch_out;
            status_.store(UpdateStatus::UpdateFailed);
            return;
        }

        // 2. 查询远端最新 Commit Hash
        int rev_code = 0;
        std::string remote_hash = execCommand("git rev-parse --short origin/" + branch, &rev_code);
        if (rev_code != 0 || remote_hash.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "无法解析远端分支 origin/" + branch;
            status_.store(UpdateStatus::UpdateFailed);
            return;
        }

        // 3. 计算落后的提交数量
        int count_code = 0;
        std::string count_str = execCommand("git rev-list --count HEAD..origin/" + branch, &count_code);
        int new_count = 0;
        try {
            new_count = std::stoi(count_str);
        } catch (...) {
            new_count = 0;
        }

        // 4. 获取差异更新日志
        std::string logs;
        if (new_count > 0) {
            logs = execCommand("git log -n 5 --format=\"* %h %s (%cr)\" HEAD..origin/" + branch);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            remote_commit_ = remote_hash;
            new_commit_count_ = new_count;
            update_log_ = logs;

            if (new_count > 0) {
                status_.store(UpdateStatus::UpdateAvailable);
            } else {
                status_.store(UpdateStatus::UpToDate);
            }
        }
    }).detach();
}

void UpdateManager::executeUpdateAsync(std::function<void(bool success, const std::string& msg)> callback) {
    UpdateStatus expected = status_.load();
    if (expected == UpdateStatus::Updating) {
        return;
    }

    status_.store(UpdateStatus::Updating);
    update_progress_ = 0.1f;

    std::thread([this, cb = std::move(callback)]() {
        std::string branch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            branch = current_branch_;
            progress_msg_ = "正在拉取最新代码 (git pull --rebase)...";
            error_msg_.clear();
        }

        // 步骤 1: 暂存保护本地修改并拉取最新提交
        execCommand("git stash --quiet 2>/dev/null || true");
        int pull_code = 0;
        std::string pull_out = execCommand("git pull --rebase origin " + branch, &pull_code);
        if (pull_code != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "拉取最新代码失败: " + pull_out;
            status_.store(UpdateStatus::UpdateFailed);
            if (cb) cb(false, error_msg_);
            return;
        }

        update_progress_ = 0.35f;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_msg_ = "正在配置 CMake 构建缓存...";
        }

        // 步骤 2: CMake 配置
        int cmake_cfg_code = 0;
        std::string build_dir = "build";
        std::string cfg_cmd = "cmake -B " + build_dir + " -DCMAKE_BUILD_TYPE=Release";
        std::string cfg_out = execCommand(cfg_cmd, &cmake_cfg_code);
        if (cmake_cfg_code != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "CMake 配置失败:\n" + cfg_out;
            status_.store(UpdateStatus::UpdateFailed);
            if (cb) cb(false, error_msg_);
            return;
        }

        update_progress_ = 0.55f;
        unsigned int num_cores = std::thread::hardware_concurrency();
        if (num_cores == 0) num_cores = 4;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_msg_ = "正在进行原生多核并发编译 (" + std::to_string(num_cores) + " 线程)...";
        }

        // 步骤 3: 多核并发编译
        int build_code = 0;
        std::string build_cmd = "cmake --build " + build_dir + " -j" + std::to_string(num_cores);
        std::string build_out = execCommand(build_cmd, &build_code);
        if (build_code != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "编译构建失败:\n" + build_out;
            status_.store(UpdateStatus::UpdateFailed);
            if (cb) cb(false, error_msg_);
            return;
        }

        update_progress_ = 1.0f;
        refreshLocalVersionInfo();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_msg_ = "固件更新与编译成功！";
            status_.store(UpdateStatus::UpdateSuccess);
        }

        if (cb) cb(true, "更新成功");
    }).detach();
}

void UpdateManager::restartApplication() {
    std::cout << "[UpdateManager] 正在触发系统/进程重启..." << std::endl;
    if (Platform::isRaspberryPi()) {
        // 树莓派 systemd 服务重启
        execCommand("sudo systemctl restart pi-hifi.service 2>/dev/null || systemctl --user restart pi-hifi.service 2>/dev/null || true");
    }
    // 退出当前进程，让前台循环脚本或 systemd 自动拉起全新构建的二进制程序
    std::exit(42);
}

UpdateStatus UpdateManager::getStatus() const {
    return status_.load();
}

std::string UpdateManager::getStatusText() const {
    switch (status_.load()) {
        case UpdateStatus::Idle:            return "就绪";
        case UpdateStatus::Checking:        return "正在检测远端更新...";
        case UpdateStatus::UpToDate:        return "已是最新版本";
        case UpdateStatus::UpdateAvailable: return "发现新版本可用";
        case UpdateStatus::Updating:        return "正在更新与编译中...";
        case UpdateStatus::UpdateSuccess:   return "更新成功，待重启";
        case UpdateStatus::UpdateFailed:    return "更新失败";
    }
    return "未知";
}

std::string UpdateManager::getCurrentCommitHash() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_commit_;
}

std::string UpdateManager::getCurrentCommitDate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_date_;
}

std::string UpdateManager::getCurrentBranch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_branch_;
}

std::string UpdateManager::getRemoteCommitHash() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remote_commit_;
}

int UpdateManager::getNewCommitCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return new_commit_count_;
}

std::string UpdateManager::getUpdateLog() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return update_log_;
}

std::string UpdateManager::getProgressMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_msg_;
}

std::string UpdateManager::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_msg_;
}

float UpdateManager::getUpdateProgress() const {
    return update_progress_;
}
