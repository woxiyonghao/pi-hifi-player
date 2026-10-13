#include "tools/MusicScanManager.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/CueSheetParser.hpp"
#include "public/AppConfig.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unordered_map>

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

    // 重新扫描：先彻底物理删除旧数据库及其全部缓存，扫描完成后再全新创建写入
    MusicDatabase::getInstance().deleteDatabase();

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

#if defined(__ANDROID__)
std::vector<std::string> getAndroidMediaStoreAudioPaths();
#endif

// 统一路径规范化：在 Android 下坚决将 /sdcard/ 与 /storage/self/primary 软链接强制规约为物理唯一真实路径 /storage/emulated/0
static std::string normalizeAudioPath(const std::filesystem::path& p) {
    std::string s = p.string();
#if defined(__ANDROID__)
    if (s.rfind("/sdcard", 0) == 0) {
        s = "/storage/emulated/0" + s.substr(7);
    } else if (s.rfind("/storage/self/primary", 0) == 0) {
        s = "/storage/emulated/0" + s.substr(21);
    }
#endif
    std::error_code ec;
    std::string can = std::filesystem::weakly_canonical(s, ec).string();
    if (!ec && !can.empty()) {
#if defined(__ANDROID__)
        if (can.rfind("/sdcard", 0) == 0) {
            can = "/storage/emulated/0" + can.substr(7);
        } else if (can.rfind("/storage/self/primary", 0) == 0) {
            can = "/storage/emulated/0" + can.substr(21);
        }
#endif
        return can;
    }
    return s;
}

void MusicScanManager::loadFromDatabase() {
    auto cached = MusicDatabase::getInstance().loadScannedTracks();
    std::lock_guard lock(mutex_);
    if (!cached.empty()) {
        // 自愈过滤：若同一物理文件既有 CUE 分轨又有整轨大文件记录，坚决剔除整轨大文件，只保留 CUE 分轨！
        std::unordered_set<std::string> cue_audio_files;
        for (const auto& t : cached) {
            if (t.is_cue_track && !t.file_path.empty()) {
                cue_audio_files.insert(t.file_path);
                std::string norm = normalizeAudioPath(t.file_path);
                if (!norm.empty()) cue_audio_files.insert(norm);
            }
        }
        if (!cue_audio_files.empty()) {
            size_t before_cnt = cached.size();
            std::erase_if(cached, [&](const Track& t) {
                if (t.is_cue_track) return false;
                std::string norm = normalizeAudioPath(t.file_path);
                return cue_audio_files.count(t.file_path) > 0 || (!norm.empty() && cue_audio_files.count(norm) > 0);
            });
            if (cached.size() != before_cnt) {
                std::cout << "[MusicScanManager] 自动清洗剔除了 " << (before_cnt - cached.size())
                          << " 首被 CUE 拆解的同名整轨大文件记录。" << std::endl;
                MusicDatabase::getInstance().saveScannedTracks(cached);
            }
        }

        scanned_tracks_ = std::move(cached);
        state_ = ScanState::Completed;
        std::cout << "[MusicScanManager] 从本地数据库恢复已扫描曲目 " << scanned_tracks_.size() << " 首。" << std::endl;
    } else {
        std::cout << "[MusicScanManager] 本地数据库中暂无已保存的扫描曲目。" << std::endl;
    }
}

void MusicScanManager::removeTracks(const std::unordered_set<uint64_t>& ids,
                                    const std::unordered_set<std::string>& paths) {
    std::vector<Track> remaining;
    {
        std::lock_guard lock(mutex_);
        std::erase_if(scanned_tracks_, [&](const Track& t) {
            return (t.id != 0 && ids.count(t.id) > 0) ||
                   (!t.file_path.empty() && paths.count(t.file_path) > 0);
        });
        if (scanned_tracks_.empty()) {
            state_ = ScanState::Idle;
        }
        remaining = scanned_tracks_;
    }
    MusicDatabase::getInstance().saveScannedTracks(remaining);
    std::cout << "[MusicScanManager] 批量删除曲目完成，剩余曲目: " << remaining.size() << " 首。" << std::endl;
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
    track.file_path = normalizeAudioPath(entry.path());
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
        try {
            std::ifstream file(track.file_path, std::ios::binary);
            if (file) {
                char magic[4];
                if (file.read(magic, 4) && std::memcmp(magic, "fLaC", 4) == 0) {
                    unsigned char hdr[4];
                    if (file.read(reinterpret_cast<char*>(hdr), 4) && (hdr[0] & 0x7F) == 0) {
                        uint32_t len = (static_cast<uint32_t>(hdr[1]) << 16) |
                                       (static_cast<uint32_t>(hdr[2]) << 8) |
                                       static_cast<uint32_t>(hdr[3]);
                        if (len >= 18 && len <= 65536) {
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
        } catch (...) {}
    } else if (format == AudioFormat::WAV) {
        track.sample_rate = 44100;
        track.bit_depth = 16;
        try {
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
                                file.seekg(chunk_sz + (chunk_sz & 1), std::ios::cur);
                            }
                        }
                        if (byte_rate > 0 && data_sz > 0) {
                            track.duration_sec = data_sz / byte_rate;
                        }
                    }
                }
            }
        } catch (...) {}
    } else {
        track.sample_rate = 44100;
        track.bit_depth = 16;
    }

    return track;
}

void MusicScanManager::scanWorker(std::stop_token stop_token, std::filesystem::path root_path) {
    std::error_code ec;

    // 1. 跨平台多根目录智能收集
    std::vector<std::filesystem::path> candidate_roots;

    if (!root_path.empty()) {
        std::string norm_root = normalizeAudioPath(root_path);
        std::error_code dummy_ec;
        std::filesystem::path rp(norm_root);
        if (!std::filesystem::exists(rp, dummy_ec)) {
            std::filesystem::create_directories(rp, dummy_ec);
        }
        if (std::filesystem::is_directory(rp, dummy_ec)) {
            candidate_roots.push_back(rp);
        }
    }

    std::string default_music_dir = AppConfig::Path::getMusicDir();
    if (!default_music_dir.empty()) {
        std::string norm_def = normalizeAudioPath(default_music_dir);
        std::error_code dummy_ec;
        std::filesystem::path dp(norm_def);
        if (!std::filesystem::exists(dp, dummy_ec)) {
            std::filesystem::create_directories(dp, dummy_ec);
        }
        if (std::filesystem::is_directory(dp, dummy_ec)) {
            candidate_roots.push_back(dp);
        }
    }

#if defined(__ANDROID__)
    // 覆盖常见音乐公共目录、大小写变体、用户自定义目录与整机存储根目录 (彻底剔除 /sdcard 软链接别名)
    const char* android_candidates[] = {
        "/storage/emulated/0/Music",
        "/storage/emulated/0/music",
        "/storage/emulated/0/Download",
        "/storage/emulated/0/download",
        "/storage/emulated/0/HiFi",
        "/storage/emulated/0/hifi",
        "/storage/emulated/0/Audio",
        "/storage/emulated/0/audio",
        "/storage/emulated/0"
    };
    for (const char* p : android_candidates) {
        std::error_code dummy_ec;
        std::filesystem::path ap(normalizeAudioPath(p));
        if (std::filesystem::exists(ap, dummy_ec) && std::filesystem::is_directory(ap, dummy_ec)) {
            candidate_roots.push_back(ap);
        }
    }

    // 动态探测外置 TF/SD 卡或 USB OTG 存储设备挂载点 (/storage/XXXX-XXXX)
    {
        std::error_code dummy_ec;
        std::filesystem::path storage_root("/storage");
        if (std::filesystem::exists(storage_root, dummy_ec) && std::filesystem::is_directory(storage_root, dummy_ec)) {
            for (const auto& dev : std::filesystem::directory_iterator(storage_root, dummy_ec)) {
                if (dev.is_directory(dummy_ec)) {
                    std::string dname = dev.path().filename().string();
                    if (dname != "emulated" && dname != "self" && !dname.empty() && dname.front() != '.') {
                        candidate_roots.push_back(normalizeAudioPath(dev.path()));
                    }
                }
            }
        }
    }
#elif defined(HIFI_PLATFORM_RPI) || defined(__linux__)
    const char* linux_candidates[] = {
        "/home/pi/Music",
        "/media/pi",
        "/media",
        "/mnt"
    };
    for (const char* p : linux_candidates) {
        std::error_code dummy_ec;
        std::filesystem::path lp(p);
        if (std::filesystem::exists(lp, dummy_ec) && std::filesystem::is_directory(lp, dummy_ec)) {
            candidate_roots.push_back(lp);
        }
    }
#elif defined(__APPLE__)
#if !TARGET_OS_IPHONE
    const char* home = std::getenv("HOME");
    if (home) {
        std::error_code dummy_ec;
        std::filesystem::path p_music = std::filesystem::path(home) / "Music";
        std::filesystem::path p_down = std::filesystem::path(home) / "Downloads";
        if (std::filesystem::exists(p_music, dummy_ec) && std::filesystem::is_directory(p_music, dummy_ec)) candidate_roots.push_back(p_music);
        if (std::filesystem::exists(p_down, dummy_ec) && std::filesystem::is_directory(p_down, dummy_ec)) candidate_roots.push_back(p_down);
    }
#endif
#endif

    // 目录去重与规范化
    const char* home_env = std::getenv("HOME");
    std::string home_norm = (home_env && home_env[0] != '\0') ? normalizeAudioPath(home_env) : "";

    std::vector<std::filesystem::path> unique_roots;
    std::unordered_set<std::string> seen_root_paths;
    for (const auto& r : candidate_roots) {
        std::string can = normalizeAudioPath(r);
#if !TARGET_OS_IPHONE && defined(__APPLE__)
        if (!home_norm.empty() && can == home_norm) {
            // macOS 严禁遍历整个用户根目录，由已添加的 Music 与 Downloads 代替
            continue;
        }
#endif
        if (seen_root_paths.insert(can).second) {
            unique_roots.push_back(can);
        }
    }

    if (unique_roots.empty()) {
        std::cerr << "[MusicScanManager] 目标扫描路径不存在或不可读。" << std::endl;
        state_ = ScanState::Failed;
        if (progress_callback_) {
            progress_callback_({root_path.string(), 0, ScanState::Failed});
        }
        return;
    }

    std::cout << "[MusicScanManager] 开始扫描发烧音乐目录 (共 " << unique_roots.size() << " 个根目录)" << std::endl;
    auto start_time = std::chrono::steady_clock::now();

    // 2. 收集 .cue 分轨描述单与常规音频文件 (双通道：MediaStore + 安全队列遍历)
    std::vector<std::filesystem::directory_entry> cue_entries;
    std::vector<std::filesystem::directory_entry> audio_entries;
    std::unordered_set<std::string> visited_file_paths;

#if defined(__ANDROID__)
    // 2.1 Android 专属通道：从系统 MediaStore 查询全部已收录音频 (确保即使 POSIX 权限受限也能一网打尽)
    {
        std::vector<std::string> ms_paths = getAndroidMediaStoreAudioPaths();
        std::cout << "[MusicScanManager] MediaStore 原生通道获取到 " << ms_paths.size() << " 个音频项" << std::endl;
        for (const auto& raw_p : ms_paths) {
            if (stop_token.stop_requested()) break;
            std::string norm_p = normalizeAudioPath(raw_p);
            if (visited_file_paths.count(norm_p) > 0 || visited_file_paths.count(raw_p) > 0) {
                continue;
            }
            std::filesystem::path fp(norm_p);
            std::string ext = fp.extension().string();
            std::string ext_lower = ext;
            std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
            if (AudioFormatUtils::isSupported(ext_lower)) {
                std::error_code chk_ec;
                if (std::filesystem::exists(fp, chk_ec) && std::filesystem::is_regular_file(fp, chk_ec)) {
                    visited_file_paths.insert(norm_p);
                    visited_file_paths.insert(raw_p);
                    audio_entries.emplace_back(fp, chk_ec);
                }
            }
        }
    }
#endif

    // 2.2 鲁棒的文件系统队列递归扫描：逐级容错，遇到受限目录安全跳过，绝不提前中断退出！
    std::unordered_set<std::string> visited_dirs;

    for (const auto& current_root : unique_roots) {
        if (stop_token.stop_requested()) break;

        std::vector<std::filesystem::path> dir_queue = { current_root };

        while (!dir_queue.empty()) {
            if (stop_token.stop_requested()) {
                std::cout << "[MusicScanManager] 扫描被用户中断。" << std::endl;
                state_ = ScanState::Cancelled;
                if (progress_callback_) {
                    std::lock_guard lock(mutex_);
                    progress_callback_({"", scanned_tracks_.size(), ScanState::Cancelled});
                }
                return;
            }

            std::filesystem::path cur_dir = dir_queue.back();
            dir_queue.pop_back();

            std::string norm_cur = normalizeAudioPath(cur_dir);
            if (!visited_dirs.insert(norm_cur).second) {
                continue;
            }

            std::error_code dir_open_ec;
            auto dir_iter = std::filesystem::directory_iterator(
                cur_dir,
                std::filesystem::directory_options::skip_permission_denied,
                dir_open_ec
            );
            if (dir_open_ec) {
                // 单个受保护子目录打开失败，跳过它，继续队列中的其它目录！绝不中断！
                continue;
            }

            for (auto it = std::filesystem::begin(dir_iter); it != std::filesystem::end(dir_iter); it.increment(dir_open_ec)) {
                if (stop_token.stop_requested()) break;
                if (dir_open_ec) {
                    dir_open_ec.clear();
                    continue;
                }

                const auto& entry = *it;
                std::error_code status_ec;

                if (entry.is_directory(status_ec)) {
                    std::string dname = entry.path().filename().string();
                    if ((!dname.empty() && dname.front() == '.') ||
                        dname == "Android" || dname == "DCIM" || dname == "Pictures" ||
                        dname == "Movies" || dname == "Library" || dname == "node_modules" ||
                        dname == "Applications" || dname == "VirtualBox VMs" ||
                        dname == "Alarms" || dname == "Ringtones" || dname == "Notifications" ||
                        dname == "Podcasts" || dname == "MIUI" || dname == "tencent" || dname == "cache") {
                        continue;
                    }
                    dir_queue.push_back(entry.path());
                } else if (entry.is_regular_file(status_ec)) {
                    std::string norm_file = normalizeAudioPath(entry.path());
                    if (visited_file_paths.count(norm_file) > 0 || visited_file_paths.count(entry.path().string()) > 0) {
                        continue;
                    }
                    visited_file_paths.insert(norm_file);
                    visited_file_paths.insert(entry.path().string());

                    std::string ext = entry.path().extension().string();
                    std::string ext_lower = ext;
                    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);

                    if (ext_lower == ".cue") {
                        cue_entries.push_back(entry);
                    } else if (AudioFormatUtils::isSupported(ext_lower)) {
                        audio_entries.push_back(entry);
                    }
                }
            }
        }
    }

    uint64_t current_id = 1;
    std::unordered_set<std::string> cue_handled_audio_paths;

    // 2.3 优先解析 CUE 分轨表，按音轨拆分虚拟单曲
    for (const auto& cue_entry : cue_entries) {
        if (stop_token.stop_requested()) return;
        auto cue_opt = CueSheetParser::parse(cue_entry.path());
        if (!cue_opt.has_value() || cue_opt->tracks.empty()) continue;

        const auto& sheet = *cue_opt;
        std::cout << "[MusicScanManager] 成功解析 CUE 分轨表: " << cue_entry.path().filename().string()
                  << " (专辑: " << sheet.album_title << ", 艺术家: " << sheet.album_performer
                  << ", 包含 " << sheet.tracks.size() << " 个音轨)" << std::endl;

        std::error_code dummy_ec;

        std::unordered_map<std::string, Track> audio_meta_cache;

        for (const auto& cue_trk : sheet.tracks) {
            if (cue_trk.resolved_audio_path.empty()) continue;

            cue_handled_audio_paths.insert(cue_trk.resolved_audio_path);
            std::string norm_audio = normalizeAudioPath(cue_trk.resolved_audio_path);
            if (!norm_audio.empty()) {
                cue_handled_audio_paths.insert(norm_audio);
            }

            if (audio_meta_cache.find(cue_trk.resolved_audio_path) == audio_meta_cache.end()) {
                std::filesystem::path audio_p(cue_trk.resolved_audio_path);
                std::string a_ext = audio_p.extension().string();
                std::string a_ext_lower = a_ext;
                std::transform(a_ext_lower.begin(), a_ext_lower.end(), a_ext_lower.begin(), ::tolower);
                auto fmt = AudioFormatUtils::getAudioFormat(a_ext_lower).value_or(AudioFormat::UNKNOWN);
                std::filesystem::directory_entry d_entry(audio_p, dummy_ec);
                Track base_meta = parseBasicMetadata(0, d_entry, fmt);
                audio_meta_cache[cue_trk.resolved_audio_path] = base_meta;
            }

            const auto& base = audio_meta_cache[cue_trk.resolved_audio_path];

            Track t;
            t.id = current_id++;
            t.title = cue_trk.title;
            t.artist = cue_trk.performer;
            t.album = sheet.album_title;
            t.file_path = cue_trk.resolved_audio_path;
            t.format = base.format;
            t.sample_rate = base.sample_rate;
            t.bit_depth = base.bit_depth;

            t.is_cue_track = true;
            t.start_time_sec = cue_trk.start_time_sec;
            t.end_time_sec = cue_trk.end_time_sec;
            t.cue_path = sheet.cue_path;
            t.track_number = cue_trk.track_number;

            if (cue_trk.duration_sec > 0) {
                t.duration_sec = cue_trk.duration_sec;
            } else if (base.duration_sec > 0 && base.duration_sec > t.start_time_sec) {
                t.end_time_sec = static_cast<double>(base.duration_sec);
                t.duration_sec = static_cast<uint32_t>(std::max(0.0, std::round(t.end_time_sec - t.start_time_sec)));
            } else {
                t.duration_sec = 0;
            }

            {
                std::lock_guard lock(mutex_);
                scanned_tracks_.push_back(t);
            }

            std::cout << "[MusicScanManager] 发现 CUE 分轨 #" << t.id << " [Track " << t.track_number << "] | 歌名: "
                      << t.title << " | 歌手: " << t.artist << " | 专辑: " << t.album
                      << " | 时长: " << (t.duration_sec / 60) << "分" << (t.duration_sec % 60) << "秒"
                      << " | 区间: " << t.start_time_sec << "s -> " << t.end_time_sec << "s"
                      << " | 路径: " << t.file_path << std::endl;

            if (progress_callback_) {
                progress_callback_({t.title, scanned_tracks_.size(), ScanState::Scanning});
            }
        }
    }

    // 2.4 解析普通音频文件。只跳过 CUE 已成功解析到的那个物理源文件，
    // 不再按目录或文件主干名模糊过滤，避免误伤普通 FLAC/WAV。
    for (const auto& entry : audio_entries) {
        if (stop_token.stop_requested()) {
            std::cout << "[MusicScanManager] 扫描被用户中断。" << std::endl;
            state_ = ScanState::Cancelled;
            if (progress_callback_) {
                std::lock_guard lock(mutex_);
                progress_callback_({"", scanned_tracks_.size(), ScanState::Cancelled});
            }
            return;
        }

        std::string full_path = entry.path().string();
        std::string norm_entry = normalizeAudioPath(entry.path());
        if (cue_handled_audio_paths.count(full_path) > 0 ||
            (!norm_entry.empty() && cue_handled_audio_paths.count(norm_entry) > 0)) {
            std::cout << "[MusicScanManager] 精准屏蔽已被 CUE 拆解的整轨大文件: " << full_path << std::endl;
            continue;
        }

        std::string ext = entry.path().extension().string();
        std::string ext_lower = ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        auto format = AudioFormatUtils::getAudioFormat(ext_lower).value_or(AudioFormat::UNKNOWN);
        Track track = parseBasicMetadata(current_id++, entry, format);

        {
            std::lock_guard lock(mutex_);
            scanned_tracks_.push_back(track);
        }

        std::cout << "[MusicScanManager] 发现曲目 #" << track.id << " | 歌名: " << track.title
                  << " | 歌手: " << track.artist << " | 专辑: " << track.album
                  << " | 规格: " << track.getFormatBadge() << " | 路径: " << track.file_path << std::endl;

        if (progress_callback_) {
            progress_callback_({entry.path().filename().string(), scanned_tracks_.size(), ScanState::Scanning});
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
