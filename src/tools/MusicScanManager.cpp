#include "tools/MusicScanManager.hpp"
#include <iostream>
#include <algorithm>

MusicScanManager::~MusicScanManager() {
    cancelScan();
}

bool MusicScanManager::startScan(const std::filesystem::path& root_path) {
    if (isScanning()) {
        std::cerr << "[MusicScanManager] 扫描任务已在运行中，请勿重复触发。" << std::endl;
        return false;
    }

    // 停止并回收上一次的工作线程
    if (worker_thread_.joinable()) {
        worker_thread_.request_stop();
        worker_thread_.join();
    }

    state_ = ScanState::Scanning;
    {
        std::lock_guard lock(mutex_);
        scanned_tracks_.clear();
    }

    // C++20 std::jthread 使用 lambda 完美接收 stop_token 并调用成员函数
    worker_thread_ = std::jthread([this, root_path](std::stop_token stop_token) {
        scanWorker(stop_token, root_path);
    });
    return true;
}

void MusicScanManager::cancelScan() {
    if (worker_thread_.joinable()) {
        worker_thread_.request_stop();
        worker_thread_.join();
    }
    if (state_ == ScanState::Scanning) {
        state_ = ScanState::Cancelled;
    }
}

void MusicScanManager::clear() {
    cancelScan();
    std::lock_guard lock(mutex_);
    scanned_tracks_.clear();
    state_ = ScanState::Idle;
}

size_t MusicScanManager::getFoundCount() const {
    std::lock_guard lock(mutex_);
    return scanned_tracks_.size();
}

std::vector<Track> MusicScanManager::getScannedTracks() const {
    std::lock_guard lock(mutex_);
    return scanned_tracks_;
}

Track MusicScanManager::parseBasicMetadata(uint64_t id, const std::filesystem::directory_entry& entry, AudioFormat format) {
    Track track;
    track.id = id;
    track.file_path = entry.path().string();
    track.format = format;

    // 文件名主干 (去除扩展名)
    std::string stem = entry.path().stem().string();

    // 智能切分 "歌手 - 歌名" (发烧友经典文件命名规范)
    constexpr std::string_view delimiter = " - ";
    if (auto pos = stem.find(delimiter); pos != std::string::npos) {
        track.artist = stem.substr(0, pos);
        track.title  = stem.substr(pos + delimiter.length());
    } else {
        track.title  = stem;
        track.artist = "未知艺术家";
    }

    // 将父目录名作为默认专辑名 (发烧无损通常按专辑目录整理归档)
    if (entry.path().has_parent_path()) {
        track.album = entry.path().parent_path().filename().string();
    }
    if (track.album.empty()) {
        track.album = "本地曲库";
    }

    // 初始发烧规格占位 (待后续接入解码器后获取真实比特率与采样率)
    if (format == AudioFormat::DSD_DSF || format == AudioFormat::DSD_DFF) {
        track.sample_rate = 2822400; // DSD64 (1-bit / 2.8224MHz)
        track.bit_depth = 1;
    } else if (format == AudioFormat::FLAC || format == AudioFormat::WAV) {
        track.sample_rate = 96000;   // 默认 Hi-Res 24bit/96kHz 规格占位
        track.bit_depth = 24;
    } else {
        track.sample_rate = 44100;
        track.bit_depth = 16;
    }

    return track;
}

void MusicScanManager::scanWorker(std::stop_token stop_token, std::filesystem::path root_path) {
    std::error_code ec;

    // 1. 基础路径有效性检查
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        std::cerr << "[MusicScanManager] 目标路径不存在或非目录: " << root_path << std::endl;
        state_ = ScanState::Failed;
        if (progress_callback_) {
            progress_callback_({root_path.string(), 0, ScanState::Failed});
        }
        return;
    }

    std::cout << "[MusicScanManager] 开始扫描发烧音乐目录: " << root_path << std::endl;

    // 2. 递归遍历 (跳过无权限目录，避免抛出异常)
    auto options = std::filesystem::directory_options::skip_permission_denied;
    auto iter = std::filesystem::recursive_directory_iterator(root_path, options, ec);
    auto end_iter = std::filesystem::recursive_directory_iterator();

    uint64_t current_id = 1;

    for (; iter != end_iter; iter.increment(ec)) {
        // [C++20 协作取消] 响应外部中断信号
        if (stop_token.stop_requested()) {
            std::cout << "[MusicScanManager] 扫描被用户中断。" << std::endl;
            state_ = ScanState::Cancelled;
            if (progress_callback_) {
                std::lock_guard lock(mutex_);
                progress_callback_({"", scanned_tracks_.size(), ScanState::Cancelled});
            }
            return;
        }

        // 跳过无法读取的坏文件
        if (ec) {
            ec.clear();
            continue;
        }

        const auto& entry = *iter;

        // 仅处理常规文件
        if (entry.is_regular_file(ec)) {
            std::string ext = entry.path().extension().string();

            // 利用 SupportedFormats 大小写不敏感比对
            if (AudioFormatUtils::isSupported(ext)) {
                auto format = AudioFormatUtils::getAudioFormat(ext).value_or(AudioFormat::UNKNOWN);
                Track track = parseBasicMetadata(current_id++, entry, format);

                {
                    std::lock_guard lock(mutex_);
                    scanned_tracks_.push_back(track);
                }

                // 触发实时进度回调
                if (progress_callback_) {
                    progress_callback_({entry.path().filename().string(), scanned_tracks_.size(), ScanState::Scanning});
                }

                // 适度非阻塞步进延时，使高能激光扫描动效与计数器得以连贯呈现
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
        }
    }

    // =========================================================================
    // TODO: 调试阶段写死 15 秒扫描时长，后续接入真实曲库规模自适应耗时
    // =========================================================================
    constexpr int DEBUG_SCAN_SECONDS = 15;
    for (int step = 0; step < DEBUG_SCAN_SECONDS * 10; ++step) {
        if (stop_token.stop_requested()) {
            std::cout << "[MusicScanManager] 扫描被用户中断。" << std::endl;
            state_ = ScanState::Cancelled;
            if (progress_callback_) {
                std::lock_guard lock(mutex_);
                progress_callback_({"", scanned_tracks_.size(), ScanState::Cancelled});
            }
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 每隔 2.5 秒模拟探测到一批发烧母带，让曲目计数与激光动画生动配合
        if (step > 0 && step % 25 == 0) {
            Track t;
            t.id = current_id++;
            if (t.id == 1) { t.artist = "蔡琴"; t.title = "渡口"; t.album = "民歌蔡琴"; t.format = AudioFormat::FLAC; t.sample_rate = 96000; t.bit_depth = 24; }
            else if (t.id == 2) { t.artist = "Eagles"; t.title = "Hotel California"; t.album = "Hotel California (Hi-Res)"; t.format = AudioFormat::DSD_DSF; t.sample_rate = 2822400; t.bit_depth = 1; }
            else if (t.id == 3) { t.artist = "维瓦尔第"; t.title = "四季 - 春 (第一乐章)"; t.album = "小提琴协奏曲"; t.format = AudioFormat::WAV; t.sample_rate = 192000; t.bit_depth = 24; }
            else if (t.id == 4) { t.artist = "Diana Krall"; t.title = "The Look of Love"; t.album = "The Look of Love"; t.format = AudioFormat::DSD_DFF; t.sample_rate = 5644800; t.bit_depth = 1; }
            else if (t.id == 5) { t.artist = "Bill Evans Trio"; t.title = "Autumn Leaves"; t.album = "Portrait in Jazz"; t.format = AudioFormat::FLAC; t.sample_rate = 192000; t.bit_depth = 24; }
            else { t.artist = "发烧试音母带"; t.title = "Track " + std::to_string(t.id); t.album = "Reference DSD Collection"; t.format = AudioFormat::DSD_DSF; t.sample_rate = 2822400; t.bit_depth = 1; }
            t.duration_sec = 260;
            {
                std::lock_guard lock(mutex_);
                scanned_tracks_.push_back(t);
            }
            if (progress_callback_) {
                progress_callback_({t.title, scanned_tracks_.size(), ScanState::Scanning});
            }
        }
    }

    // 3. 扫描顺利完成
    state_ = ScanState::Completed;
    std::cout << "[MusicScanManager] 扫描完成！共发现 " << scanned_tracks_.size() << " 首发烧曲目。" << std::endl;

    if (progress_callback_) {
        std::lock_guard lock(mutex_);
        progress_callback_({"扫描完成", scanned_tracks_.size(), ScanState::Completed});
    }

    if (complete_callback_) {
        std::vector<Track> copy;
        {
            std::lock_guard lock(mutex_);
            copy = scanned_tracks_;
        }
        complete_callback_(copy);
    }
}