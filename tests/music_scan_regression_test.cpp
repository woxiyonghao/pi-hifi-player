#include "public/AppConfig.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sqlite3.h>

namespace {

class TempTree {
public:
    TempTree() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
               ("hifi_music_scan_regression_" + std::to_string(stamp));
        std::filesystem::create_directories(root);
    }

    ~TempTree() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    std::filesystem::path root;
};

void writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

bool waitForScan(MusicScanManager& scanner) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (scanner.isScanning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return scanner.getState() == ScanState::Completed;
}

bool containsPhysicalTrack(const std::vector<Track>& tracks, const std::filesystem::path& path,
                           AudioFormat format) {
    for (const auto& track : tracks) {
        if (!track.is_cue_track && track.file_path == path.string() && track.format == format) {
            return true;
        }
    }
    return false;
}

int fail(const std::string& message) {
    std::cerr << "[music_scan_regression] " << message << '\n';
    return 1;
}

} // namespace

int main() {
    TempTree temp;
    const auto plain_dir = temp.root / "plain";
    const auto cue_dir = temp.root / "cue_album";
    const auto config_dir = temp.root / "config";
    std::filesystem::create_directories(plain_dir);
    std::filesystem::create_directories(cue_dir);

    // 扫描器只需要文件头即可识别格式；测试不依赖外部音频样本。
    const std::vector<unsigned char> minimal_flac = {'f', 'L', 'a', 'C'};
    const std::vector<unsigned char> minimal_wav = {
        'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'
    };

    const auto plain_flac = plain_dir / "ordinary.flac";
    const auto plain_wav = plain_dir / "ordinary.wav";
    writeBytes(plain_flac, minimal_flac);
    writeBytes(plain_wav, minimal_wav);

    AppConfig::Path::setConfigDir(config_dir.string());
    AppConfig::Path::setMusicDir(plain_dir.string());

    auto& database = MusicDatabase::getInstance();
    database.close();
    auto& scanner = MusicScanManager::getInstance();

    if (!scanner.startScan(plain_dir) || !waitForScan(scanner)) {
        return fail("普通 FLAC/WAV 扫描未完成");
    }

    auto tracks = scanner.getScannedTracks();
    if (!containsPhysicalTrack(tracks, plain_flac, AudioFormat::FLAC) ||
        !containsPhysicalTrack(tracks, plain_wav, AudioFormat::WAV)) {
        return fail("普通 FLAC/WAV 被 CUE 去重逻辑误过滤");
    }

    scanner.clear();

    const auto album_wav = cue_dir / "album.wav";
    const auto bonus_flac = cue_dir / "bonus.flac";
    writeBytes(album_wav, minimal_wav);
    writeBytes(bonus_flac, minimal_flac);
    writeText(cue_dir / "album.cue",
              "PERFORMER \"Regression Artist\"\n"
              "TITLE \"Regression Album\"\n"
              "FILE \"album.wav\" WAVE\n"
              "  TRACK 01 AUDIO\n"
              "    TITLE \"Part One\"\n"
              "    INDEX 01 00:00:00\n"
              "  TRACK 02 AUDIO\n"
              "    TITLE \"Part Two\"\n"
              "    INDEX 01 01:00:00\n");

    AppConfig::Path::setMusicDir(cue_dir.string());
    if (!scanner.startScan(cue_dir) || !waitForScan(scanner)) {
        return fail("CUE + FLAC/WAV 扫描未完成");
    }

    tracks = scanner.getScannedTracks();
    size_t cue_count = 0;
    size_t physical_wav_count = 0;
    for (const auto& track : tracks) {
        if (track.is_cue_track && track.cue_path == (cue_dir / "album.cue").string()) {
            ++cue_count;
        }
        if (!track.is_cue_track && track.file_path == album_wav.string()) {
            ++physical_wav_count;
        }
    }

    if (cue_count != 2) {
        return fail("WAV 切片未产生预期的 2 首 CUE 虚拟音轨");
    }
    if (physical_wav_count != 0) {
        return fail("已分轨 WAV 仍以整轨文件重复入库");
    }
    // 3. 测试冷启动恢复 (模拟退出 App 重新启动直接 loadFromDatabase)
    database.close();
    database.init();
    scanner.loadFromDatabase();

    auto loaded_tracks = scanner.getScannedTracks();
    if (loaded_tracks.size() != tracks.size()) {
        return fail("冷启动 loadFromDatabase 恢复曲目数不一致: 恢复 " +
                    std::to_string(loaded_tracks.size()) + " 首, 预期 " +
                    std::to_string(tracks.size()) + " 首");
    }
    if (!containsPhysicalTrack(loaded_tracks, bonus_flac, AudioFormat::FLAC)) {
        return fail("冷启动恢复后独立 FLAC 丢失");
    }

    // 4. 专项验证：旧版本数据库带有 UNIQUE(file_path) 约束时无损自愈升级
    database.close();
    {
        // 模拟早期旧版 SQLite 数据库
        std::filesystem::path legacy_db_path = config_dir / "library.db";
        sqlite3* raw_db = nullptr;
        sqlite3_open(legacy_db_path.string().c_str(), &raw_db);
        sqlite3_exec(raw_db, "DROP TABLE IF EXISTS scanned_tracks;", nullptr, nullptr, nullptr);
        const char* legacy_ddl = R"(
            CREATE TABLE scanned_tracks (
                id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                artist TEXT NOT NULL,
                album TEXT NOT NULL,
                file_path TEXT NOT NULL UNIQUE,
                format INTEGER NOT NULL,
                sample_rate INTEGER NOT NULL,
                bit_depth INTEGER NOT NULL,
                duration_sec INTEGER NOT NULL
            );
            INSERT INTO scanned_tracks VALUES (1, 'Old Track', 'Old Artist', 'Old Album', '/old/path.mp3', 6, 44100, 16, 200);
        )";
        sqlite3_exec(raw_db, legacy_ddl, nullptr, nullptr, nullptr);
        sqlite3_close(raw_db);
    }

    // 用新代码重新打开，验证其平滑迁移且能写入多个同源 CUE 分轨
    if (!database.init()) {
        return fail("旧版带 UNIQUE 数据库初始化迁移失败");
    }
    // 尝试写入包含相同 file_path 的 CUE 分轨
    std::vector<Track> cue_multi_tracks;
    Track trk1; trk1.id = 1; trk1.file_path = album_wav.string(); trk1.is_cue_track = true; trk1.title = "Part 1"; trk1.format = AudioFormat::WAV;
    Track trk2; trk2.id = 2; trk2.file_path = album_wav.string(); trk2.is_cue_track = true; trk2.title = "Part 2"; trk2.format = AudioFormat::WAV;
    Track trk3; trk3.id = 3; trk3.file_path = bonus_flac.string(); trk3.is_cue_track = false; trk3.title = "Bonus"; trk3.format = AudioFormat::FLAC;
    cue_multi_tracks.push_back(trk1);
    cue_multi_tracks.push_back(trk2);
    cue_multi_tracks.push_back(trk3);

    if (!database.saveScannedTracks(cue_multi_tracks)) {
        return fail("向迁移后的数据库写入多个同路径 CUE 分轨失败 (UNIQUE 约束未解除)");
    }

    auto restored = database.loadScannedTracks();
    if (restored.size() != 3) {
        return fail("迁移后持久化曲目数量不匹配: " + std::to_string(restored.size()) + " vs 3");
    }

    scanner.clear();
    database.close();
    std::cout << "[music_scan_regression] PASS\n";
    return 0;
}
