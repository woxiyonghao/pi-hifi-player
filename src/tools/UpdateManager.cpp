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
#include <filesystem>
#include <climits>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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

std::string UpdateManager::findRepositoryRoot() {
    // 1. 测试当前工作目录是否在 Git 仓库内
    int code = 0;
    std::string top = execCommand("git rev-parse --show-toplevel 2>/dev/null", &code);
    if (code == 0 && !top.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(top) / ".git", ec)) {
            return top;
        }
    }

    // 2. 从当前进程可执行文件向上溯源查找 .git 仓库 (支持 build/PiHifiPlayer.app 结构)
#if defined(__APPLE__)
    char exe_buf[PATH_MAX];
    uint32_t size = sizeof(exe_buf);
    if (_NSGetExecutablePath(exe_buf, &size) == 0) {
        char real_exe[PATH_MAX];
        if (realpath(exe_buf, real_exe)) {
            std::filesystem::path p(real_exe);
            for (int i = 0; i < 6; ++i) {
                p = p.parent_path();
                std::error_code ec;
                if (std::filesystem::exists(p / ".git", ec)) {
                    return p.string();
                }
            }
        }
    }
#elif defined(__linux__)
    char exe_buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (len > 0) {
        exe_buf[len] = '\0';
        std::filesystem::path p(exe_buf);
        for (int i = 0; i < 6; ++i) {
            p = p.parent_path();
            std::error_code ec;
            if (std::filesystem::exists(p / ".git", ec)) {
                return p.string();
            }
        }
    }
#endif

    // 3. 测试编译期源码根目录
#if defined(HIFI_SOURCE_DIR)
    {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(HIFI_SOURCE_DIR) / ".git", ec)) {
            return HIFI_SOURCE_DIR;
        }
    }
#endif

    return "";
}

void UpdateManager::refreshLocalVersionInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string repo = findRepositoryRoot();
    if (!repo.empty()) {
        int code = 0;
        std::string branch = execCommand("git -C \"" + repo + "\" rev-parse --abbrev-ref HEAD", &code);
        if (code == 0 && !branch.empty()) {
            current_branch_ = branch;
        }
        std::string hash = execCommand("git -C \"" + repo + "\" rev-parse --short HEAD", &code);
        if (code == 0 && !hash.empty()) {
            current_commit_ = hash;
        }
        std::string date = execCommand("git -C \"" + repo + "\" log -1 --format=%cd --date=short", &code);
        if (code == 0 && !date.empty()) {
            current_date_ = date;
        }
    } else {
#if defined(APP_GIT_BRANCH)
        current_branch_ = APP_GIT_BRANCH;
#else
        current_branch_ = "dev/iphone";
#endif
#if defined(APP_GIT_COMMIT)
        current_commit_ = APP_GIT_COMMIT;
#else
        current_commit_ = "7e967b9";
#endif
#if defined(APP_GIT_DATE)
        current_date_ = APP_GIT_DATE;
#else
        current_date_ = "2026-10-12";
#endif
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
        std::string local_hash;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            branch = current_branch_;
            local_hash = current_commit_;
        }
        if (branch.empty()) branch = "dev/iphone";

        std::string repo = findRepositoryRoot();

        // 场景 A: 存在本地源码仓库（树莓派原生系统或本地开发调试）
        if (!repo.empty()) {
            int fetch_code = 0;
            std::string fetch_cmd = "git -C \"" + repo + "\" fetch https://github.com/woxiyonghao/pi-hifi-player.git " + branch + " --quiet";
            std::string fetch_out = execCommand(fetch_cmd, &fetch_code);
            if (fetch_code != 0) {
                fetch_cmd = "git -C \"" + repo + "\" fetch origin " + branch + " --quiet";
                fetch_out = execCommand(fetch_cmd, &fetch_code);
            }

            if (fetch_code != 0) {
                std::lock_guard<std::mutex> lock(mutex_);
                error_msg_ = "远端连接失败: " + fetch_out;
                status_.store(UpdateStatus::UpdateFailed);
                return;
            }

            int rev_code = 0;
            std::string remote_hash = execCommand("git -C \"" + repo + "\" rev-parse --short FETCH_HEAD", &rev_code);
            if (rev_code != 0 || remote_hash.empty()) {
                std::lock_guard<std::mutex> lock(mutex_);
                error_msg_ = "无法解析远端更新标记 FETCH_HEAD";
                status_.store(UpdateStatus::UpdateFailed);
                return;
            }

            int count_code = 0;
            std::string count_str = execCommand("git -C \"" + repo + "\" rev-list --count HEAD..FETCH_HEAD", &count_code);
            int new_count = 0;
            try {
                new_count = std::stoi(count_str);
            } catch (...) {
                new_count = 0;
            }

            std::string logs;
            if (new_count > 0) {
                logs = execCommand("git -C \"" + repo + "\" log -n 5 --format=\"* %h %s (%cr)\" HEAD..FETCH_HEAD");
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
            return;
        }

        // 场景 B: 无本地 Git 仓库（如独立安装包、DMG 镜像交付场景）
        // 通过 git ls-remote 直连 GitHub 获取分支远端最新 Commit Hash (免本地仓库运行)
        int ls_code = 0;
        std::string ls_cmd = "git ls-remote --exit-code -h https://github.com/woxiyonghao/pi-hifi-player.git " + branch;
        std::string ls_out = execCommand(ls_cmd, &ls_code);

        if (ls_code != 0 || ls_out.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "连接远端仓库失败，请检查网络连接";
            status_.store(UpdateStatus::UpdateFailed);
            return;
        }

        std::string remote_full_hash;
        std::istringstream iss(ls_out);
        iss >> remote_full_hash;

        std::string remote_short = remote_full_hash.substr(0, 7);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            remote_commit_ = remote_short;

            if (!local_hash.empty() && local_hash != "Unknown" && remote_short.rfind(local_hash, 0) == 0) {
                new_commit_count_ = 0;
                update_log_.clear();
                status_.store(UpdateStatus::UpToDate);
            } else {
                new_commit_count_ = 1;
                update_log_ = "* " + remote_short + " 最新云端固件版本";
                status_.store(UpdateStatus::UpdateAvailable);
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

        std::string repo = findRepositoryRoot();
        if (repo.empty()) {
            // 如果本地无源码仓库，尝试自动克隆至专用运行缓存目录并编译
            const char* home = std::getenv("HOME");
            std::string cache_root = home ? (std::string(home) + "/Library/Application Support/PiHifiPlayer") : "/tmp/pi_hifi_ota";
            std::string clone_dir = cache_root + "/source";

            std::error_code ec;
            std::filesystem::create_directories(cache_root, ec);
            if (!std::filesystem::exists(std::filesystem::path(clone_dir) / ".git", ec)) {
                int clone_code = 0;
                execCommand("git clone --depth 1 -b " + branch + " https://github.com/woxiyonghao/pi-hifi-player.git \"" + clone_dir + "\"", &clone_code);
            }
            if (std::filesystem::exists(std::filesystem::path(clone_dir) / ".git", ec)) {
                repo = clone_dir;
            } else {
                std::lock_guard<std::mutex> lock(mutex_);
                error_msg_ = "当前环境无编译源码，请前往 GitHub Releases 下载最新 DMG 安装包";
                status_.store(UpdateStatus::UpdateFailed);
                if (cb) cb(false, error_msg_);
                return;
            }
        }

        // 步骤 1: 暂存保护本地修改并拉取最新提交
        execCommand("git -C \"" + repo + "\" stash --quiet 2>/dev/null || true");
        int pull_code = 0;
        std::string pull_out = execCommand("git -C \"" + repo + "\" pull --rebase https://github.com/woxiyonghao/pi-hifi-player.git " + branch, &pull_code);
        if (pull_code != 0) {
            pull_out = execCommand("git -C \"" + repo + "\" pull --rebase origin " + branch, &pull_code);
        }
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
        std::string build_dir = repo + "/build";
        std::string cfg_cmd = "cmake -B \"" + build_dir + "\" -S \"" + repo + "\" -DCMAKE_BUILD_TYPE=Release";
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
        std::string build_cmd = "cmake --build \"" + build_dir + "\" -j" + std::to_string(num_cores);
        std::string build_out = execCommand(build_cmd, &build_code);
        if (build_code != 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            error_msg_ = "编译构建失败:\n" + build_out;
            status_.store(UpdateStatus::UpdateFailed);
            if (cb) cb(false, error_msg_);
            return;
        }

#if defined(__APPLE__)
        // macOS 下若正在运行 /Applications/PiHifiPlayer.app，自动将最新编译产物同步覆盖
        if (Platform::isMac()) {
            char exe_buf[PATH_MAX];
            uint32_t size = sizeof(exe_buf);
            if (_NSGetExecutablePath(exe_buf, &size) == 0) {
                std::string current_exe(exe_buf);
                if (current_exe.find("/Applications/PiHifiPlayer.app") != std::string::npos) {
                    std::string new_bin = build_dir + "/PiHifiPlayer";
                    std::error_code ec;
                    if (std::filesystem::exists(new_bin, ec)) {
                        execCommand("cp -f \"" + new_bin + "\" \"" + current_exe + "\"");
                    }
                }
            }
        }
#endif

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
        std::exit(42);
    }

#if defined(__APPLE__)
    if (Platform::isMac()) {
        char exe_buf[PATH_MAX];
        uint32_t size = sizeof(exe_buf);
        if (_NSGetExecutablePath(exe_buf, &size) == 0) {
            std::string exe(exe_buf);
            size_t app_pos = exe.find(".app");
            if (app_pos != std::string::npos) {
                std::string app_path = exe.substr(0, app_pos + 4);
                std::string restart_cmd = "(sleep 0.5 && open -n \"" + app_path + "\") &";
                system(restart_cmd.c_str());
            } else {
                std::string restart_cmd = "(sleep 0.5 && \"" + exe + "\") &";
                system(restart_cmd.c_str());
            }
        }
        std::exit(0);
    }
#endif

    std::exit(0);
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
