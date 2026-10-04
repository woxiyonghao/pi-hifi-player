#include "tools/MusicScanManager.hpp"
#include "tools/MusicDatabase.hpp"
#include "public/AppConfig.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>

MusicScanManager::~MusicScanManager() {
    cancelScan();
}

bool MusicScanManager::startScan(const std::filesystem::path& root_path) {
    if (isScanning()) {
        std::cerr << "[MusicScanManager] 扫描任务已在运行中，请勿重复触发。" << std::endl;
        return false;
    }

    std::filesystem::path scan_target =
        root_path.empty() ? std::filesystem::path(AppConfig::Path::getMusicDir()) : root_path;

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
    worker_thread_ =
        std::jthread([this, scan_target](std::stop_token stop_token) { scanWorker(stop_token, scan_target); });
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
    {
        std::lock_guard lock(mutex_);
        scanned_tracks_.clear();
        state_ = ScanState::Idle;
    }
    MusicDatabase::getInstance().clearScannedTracks();
}

void MusicScanManager::loadFromDatabase() {
    auto cached = MusicDatabase::getInstance().loadScannedTracks();
    if (!cached.empty()) {
        std::lock_guard lock(mutex_);
        scanned_tracks_ = std::move(cached);
        state_ = ScanState::Completed;
        std::cout << "[MusicScanManager] 从本地数据库恢复已扫描曲目 " << scanned_tracks_.size() << " 首。" << std::endl;
    }
}

size_t MusicScanManager::getFoundCount() const {
    std::lock_guard lock(mutex_);
    return scanned_tracks_.size();
}

std::vector<Track> MusicScanManager::getScannedTracks() const {
    std::lock_guard lock(mutex_);
    return scanned_tracks_;
}

Track MusicScanManager::parseBasicMetadata(uint64_t id, const std::filesystem::directory_entry& entry,
                                           AudioFormat format) {
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
        track.title = stem.substr(pos + delimiter.length());
    } else {
        track.title = stem;
        track.artist = "未知艺术家";
    }

    // 将父目录名作为默认专辑名 (发烧无损通常按专辑目录整理归档)
    if (entry.path().has_parent_path()) {
        track.album = entry.path().parent_path().filename().string();
    }
    if (track.album.empty()) {
        track.album = "本地曲库";
    }

    // 初始发烧规格占位
    if (format == AudioFormat::DSD_DSF || format == AudioFormat::DSD_DFF) {
        track.sample_rate = 2822400; // DSD64 (1-bit / 2.8224MHz)
        track.bit_depth = 1;
    } else if (format == AudioFormat::FLAC) {
        track.sample_rate = 96000;
        track.bit_depth = 24;
        std::ifstream file(track.file_path, std::ios::binary);
        if (file) {
            char magic[4];
            if (file.read(magic, 4) && std::memcmp(magic, "fLaC", 4) == 0) {
                unsigned char hdr[4];
                if (file.read(reinterpret_cast<char*>(hdr), 4) && (hdr[0] & 0x7F) == 0) {
                    uint32_t len = (static_cast<uint32_t>(hdr[1]) << 16) |
                                   (static_cast<uint32_t>(hdr[2]) << 8) |
                                   static_cast<uint32_t>(hdr[3]);
                    if (len >= 18) {
                        std::vector<unsigned char> data(len);
                        if (file.read(reinterpret_cast<char*>(data.data()), len)) {
                            uint64_t b = 0;
                            for (int i = 10; i < 18; ++i) {
                                b = (b << 8) | data[i];
                            }
                            uint32_t sr = static_cast<uint32_t>(b >> 44);
                            uint8_t bps = static_cast<uint8_t>(((b >> 36) & 0x1F) + 1);
                            uint64_t total_samples = b & 0xFFFFFFFFF;
                            if (sr > 0) {
                                track.sample_rate = sr;
                                track.bit_depth = bps;
                                track.duration_sec = static_cast<uint32_t>(total_samples / sr);
                            }
                        }
                    }
                }
            }
        }
    } else if (format == AudioFormat::WAV) {
        track.sample_rate = 44100;
        track.bit_depth = 16;
        std::ifstream file(track.file_path, std::ios::binary);
        if (file) {
            char riff[4];
            if (file.read(riff, 4) && std::memcmp(riff, "RIFF", 4) == 0) {
                file.seekg(8);
                char wave[4];
                if (file.read(wave, 4) && std::memcmp(wave, "WAVE", 4) == 0) {
                    char chunk_id[4];
                    uint32_t chunk_sz = 0;
                    uint32_t byte_rate = 0;
                    uint32_t data_sz = 0;
                    while (file.read(chunk_id, 4) && file.read(reinterpret_cast<char*>(&chunk_sz), 4)) {
                        if (std::memcmp(chunk_id, "fmt ", 4) == 0 && chunk_sz >= 16) {
                            uint16_t audio_fmt = 0, ch = 0, bps = 0;
                            uint32_t sr = 0;
                            file.read(reinterpret_cast<char*>(&audio_fmt), 2);
                            file.read(reinterpret_cast<char*>(&ch), 2);
                            file.read(reinterpret_cast<char*>(&sr), 4);
                            file.read(reinterpret_cast<char*>(&byte_rate), 4);
                            file.seekg(2, std::ios::cur);
                            file.read(reinterpret_cast<char*>(&bps), 2);
                            track.sample_rate = sr;
                            track.bit_depth = bps;
                            if (chunk_sz > 16) file.seekg(chunk_sz - 16, std::ios::cur);
                        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
                            data_sz = chunk_sz;
                            break;
                        } else {
                            file.seekg(chunk_sz, std::ios::cur);
                        }
                    }
                    if (byte_rate > 0 && data_sz > 0) {
                        track.duration_sec = data_sz / byte_rate;
                    }
                }
            }
        }
    } else {
        track.sample_rate = 44100;
        track.bit_depth = 16;
    }

    return track;
}

void MusicScanManager::scanWorker(std::stop_token stop_token, std::filesystem::path root_path) {
    std::error_code ec;

    // 1. 基础路径有效性检查 (若目录不存在则自动尝试创建，确保跨机器兼容)
    if (!std::filesystem::exists(root_path, ec)) {
        std::filesystem::create_directories(root_path, ec);
    }

    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        std::cerr << "[MusicScanManager] 目标路径不存在或非目录: " << root_path << std::endl;
        state_ = ScanState::Failed;
        if (progress_callback_) {
            progress_callback_({root_path.string(), 0, ScanState::Failed});
        }
        return;
    }

    std::cout << "[MusicScanManager] 开始扫描发烧音乐目录: " << root_path << std::endl;
    auto start_time = std::chrono::steady_clock::now();

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

        // 智能剪枝：若为目录且属于隐藏目录或已知系统/开发大仓目录，直接跳过其子目录递归
        if (entry.is_directory(ec)) {
            std::string dir_name = entry.path().filename().string();
            if ((!dir_name.empty() && dir_name.front() == '.') || dir_name == "Library" || dir_name == "node_modules" ||
                dir_name == "Applications" || dir_name == "VirtualBox VMs") {
                iter.disable_recursion_pending();
                continue;
            }
        } else if (entry.is_regular_file(ec)) {
            std::string ext = entry.path().extension().string();

            // 利用 SupportedFormats 大小写不敏感比对
            if (AudioFormatUtils::isSupported(ext)) {
                auto format = AudioFormatUtils::getAudioFormat(ext).value_or(AudioFormat::UNKNOWN);
                Track track = parseBasicMetadata(current_id++, entry, format);

                {
                    std::lock_guard lock(mutex_);
                    scanned_tracks_.push_back(track);
                }

                std::cout << "[MusicScanManager] 发现曲目 #" << track.id << " | 歌名: " << track.title
                          << " | 歌手: " << track.artist << " | 专辑: " << track.album
                          << " | 规格: " << track.getFormatBadge() << " | 路径: " << track.file_path << std::endl;

                // 触发实时进度回调
                if (progress_callback_) {
                    progress_callback_({entry.path().filename().string(), scanned_tracks_.size(), ScanState::Scanning});
                }
            }
        }
    }

    // =========================================================================
    // 3. 动态时间约束：若实际扫描耗时低于 3 秒，保持扫描动效至满 3 秒
    // =========================================================================
    constexpr auto MIN_SCAN_DURATION = std::chrono::milliseconds(3000);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

    if (elapsed < MIN_SCAN_DURATION) {
        auto remaining = MIN_SCAN_DURATION - elapsed;
        auto sleep_until = std::chrono::steady_clock::now() + remaining;
        while (std::chrono::steady_clock::now() < sleep_until) {
            if (stop_token.stop_requested()) {
                std::cout << "[MusicScanManager] 扫描被用户中断。" << std::endl;
                state_ = ScanState::Cancelled;
                if (progress_callback_) {
                    std::lock_guard lock(mutex_);
                    progress_callback_({"", scanned_tracks_.size(), ScanState::Cancelled});
                }
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    // 4. 扫描顺利完成并持久化写入数据库
    state_ = ScanState::Completed;
    std::cout << "[MusicScanManager] 扫描完成！共发现 " << scanned_tracks_.size() << " 首发烧曲目。" << std::endl;

    std::vector<Track> copy;
    {
        std::lock_guard lock(mutex_);
        copy = scanned_tracks_;
    }

    // 持久化保存至 SQLite
    MusicDatabase::getInstance().saveScannedTracks(copy);

    if (progress_callback_) {
        progress_callback_({"扫描完成", copy.size(), ScanState::Completed});
    }

    if (complete_callback_) {
        complete_callback_(copy);
    }
}