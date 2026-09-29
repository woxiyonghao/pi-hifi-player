#include "tools/MusicDatabase.hpp"
#include "public/AppConfig.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>

MusicDatabase::~MusicDatabase() {
    close();
}

bool MusicDatabase::init(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_initialized_ && db_) {
        return true;
    }

    std::string path = db_path;
    if (path.empty()) {
        std::string config_dir = AppConfig::Path::getConfigDir();
        std::error_code ec;
        if (!std::filesystem::exists(config_dir, ec)) {
            std::filesystem::create_directories(config_dir, ec);
        }
        path = config_dir + "/library.db";
    }

    int rc = sqlite3_open_v2(path.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicDatabase] 打开数据库失败: " << (db_ ? sqlite3_errmsg(db_) : "未知错误") << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // 启用 WAL 模式提高并发与性能
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // 创建数据表结构
    const char* ddl = R"(
        CREATE TABLE IF NOT EXISTS scanned_tracks (
            id INTEGER PRIMARY KEY,
            title TEXT NOT NULL,
            artist TEXT NOT NULL,
            album TEXT NOT NULL,
            file_path TEXT UNIQUE NOT NULL,
            format INTEGER NOT NULL,
            sample_rate INTEGER NOT NULL,
            bit_depth INTEGER NOT NULL,
            duration_sec INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            cover_path TEXT
        );

        CREATE TABLE IF NOT EXISTS playlist_tracks (
            playlist_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            artist TEXT NOT NULL,
            album TEXT NOT NULL,
            file_path TEXT NOT NULL,
            format INTEGER NOT NULL,
            sample_rate INTEGER NOT NULL,
            bit_depth INTEGER NOT NULL,
            duration_sec INTEGER NOT NULL,
            sort_order INTEGER NOT NULL,
            PRIMARY KEY(playlist_id, sort_order)
        );
    )";

    char* err_msg = nullptr;
    rc = sqlite3_exec(db_, ddl, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "[MusicDatabase] 初始化数据表失败: " << (err_msg ? err_msg : "未知") << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    is_initialized_ = true;
    std::cout << "[MusicDatabase] SQLite 发烧曲库数据库已成功连接: " << path << std::endl;
    return true;
}

void MusicDatabase::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    is_initialized_ = false;
}

bool MusicDatabase::saveScannedTracks(const std::vector<Track>& tracks) {
    if (!init()) return false;
    std::lock_guard<std::mutex> lock(mutex_);

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 清空现有曲目，全量写入最新扫描结果
    sqlite3_exec(db_, "DELETE FROM scanned_tracks;", nullptr, nullptr, nullptr);

    const char* sql = R"(
        INSERT INTO scanned_tracks (id, title, artist, album, file_path, format, sample_rate, bit_depth, duration_sec)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& t : tracks) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(t.id));
        sqlite3_bind_text(stmt, 2, t.title.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, t.artist.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, t.album.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, t.file_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, static_cast<int>(t.format));
        sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(t.sample_rate));
        sqlite3_bind_int(stmt, 8, static_cast<int>(t.bit_depth));
        sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(t.duration_sec));

        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);

    std::cout << "[MusicDatabase] 已成功持久化 " << tracks.size() << " 首扫描曲目至数据库。" << std::endl;
    return true;
}

std::vector<Track> MusicDatabase::loadScannedTracks() {
    std::vector<Track> tracks;
    if (!init()) return tracks;
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT id, title, artist, album, file_path, format, sample_rate, bit_depth, duration_sec FROM scanned_tracks ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tracks;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Track t;
        t.id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        const unsigned char* title_str = sqlite3_column_text(stmt, 1);
        t.title = title_str ? reinterpret_cast<const char*>(title_str) : "";
        const unsigned char* artist_str = sqlite3_column_text(stmt, 2);
        t.artist = artist_str ? reinterpret_cast<const char*>(artist_str) : "";
        const unsigned char* album_str = sqlite3_column_text(stmt, 3);
        t.album = album_str ? reinterpret_cast<const char*>(album_str) : "";
        const unsigned char* path_str = sqlite3_column_text(stmt, 4);
        t.file_path = path_str ? reinterpret_cast<const char*>(path_str) : "";
        t.format = static_cast<AudioFormat>(sqlite3_column_int(stmt, 5));
        t.sample_rate = static_cast<uint32_t>(sqlite3_column_int64(stmt, 6));
        t.bit_depth = static_cast<uint8_t>(sqlite3_column_int(stmt, 7));
        t.duration_sec = static_cast<uint32_t>(sqlite3_column_int64(stmt, 8));

        tracks.push_back(std::move(t));
    }

    sqlite3_finalize(stmt);
    return tracks;
}

void MusicDatabase::clearScannedTracks() {
    if (!init()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_exec(db_, "DELETE FROM scanned_tracks;", nullptr, nullptr, nullptr);
}

bool MusicDatabase::savePlaylists(const std::vector<Playlist>& playlists) {
    if (!init()) return false;
    std::lock_guard<std::mutex> lock(mutex_);

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "DELETE FROM playlist_tracks;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "DELETE FROM playlists;", nullptr, nullptr, nullptr);

    const char* pl_sql = "INSERT INTO playlists (id, name, cover_path) VALUES (?, ?, ?);";
    sqlite3_stmt* pl_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, pl_sql, -1, &pl_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* trk_sql = R"(
        INSERT INTO playlist_tracks (playlist_id, track_id, title, artist, album, file_path, format, sample_rate, bit_depth, duration_sec, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_stmt* trk_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, trk_sql, -1, &trk_stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(pl_stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& pl : playlists) {
        sqlite3_reset(pl_stmt);
        sqlite3_bind_int64(pl_stmt, 1, static_cast<sqlite3_int64>(pl.getId()));
        sqlite3_bind_text(pl_stmt, 2, pl.getName().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(pl_stmt, 3, pl.getCoverPath().c_str(), -1, SQLITE_STATIC);
        sqlite3_step(pl_stmt);

        const auto& pl_tracks = pl.getTracks();
        for (size_t i = 0; i < pl_tracks.size(); ++i) {
            const auto& t = pl_tracks[i];
            sqlite3_reset(trk_stmt);
            sqlite3_bind_int64(trk_stmt, 1, static_cast<sqlite3_int64>(pl.getId()));
            sqlite3_bind_int64(trk_stmt, 2, static_cast<sqlite3_int64>(t.id));
            sqlite3_bind_text(trk_stmt, 3, t.title.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(trk_stmt, 4, t.artist.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(trk_stmt, 5, t.album.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(trk_stmt, 6, t.file_path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(trk_stmt, 7, static_cast<int>(t.format));
            sqlite3_bind_int64(trk_stmt, 8, static_cast<sqlite3_int64>(t.sample_rate));
            sqlite3_bind_int(trk_stmt, 9, static_cast<int>(t.bit_depth));
            sqlite3_bind_int64(trk_stmt, 10, static_cast<sqlite3_int64>(t.duration_sec));
            sqlite3_bind_int(trk_stmt, 11, static_cast<int>(i));

            sqlite3_step(trk_stmt);
        }
    }

    sqlite3_finalize(trk_stmt);
    sqlite3_finalize(pl_stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);

    std::cout << "[MusicDatabase] 已成功持久化 " << playlists.size() << " 张播放列表至数据库。" << std::endl;
    return true;
}

std::vector<Playlist> MusicDatabase::loadPlaylists() {
    std::vector<Playlist> playlists;
    if (!init()) return playlists;
    std::lock_guard<std::mutex> lock(mutex_);

    const char* pl_sql = "SELECT id, name, cover_path FROM playlists ORDER BY id ASC;";
    sqlite3_stmt* pl_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, pl_sql, -1, &pl_stmt, nullptr) != SQLITE_OK) {
        return playlists;
    }

    const char* trk_sql = R"(
        SELECT track_id, title, artist, album, file_path, format, sample_rate, bit_depth, duration_sec
        FROM playlist_tracks WHERE playlist_id = ? ORDER BY sort_order ASC;
    )";
    sqlite3_stmt* trk_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, trk_sql, -1, &trk_stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(pl_stmt);
        return playlists;
    }

    while (sqlite3_step(pl_stmt) == SQLITE_ROW) {
        uint64_t pid = static_cast<uint64_t>(sqlite3_column_int64(pl_stmt, 0));
        const unsigned char* name_str = sqlite3_column_text(pl_stmt, 1);
        std::string name = name_str ? reinterpret_cast<const char*>(name_str) : "";
        const unsigned char* cover_str = sqlite3_column_text(pl_stmt, 2);
        std::string cover = cover_str ? reinterpret_cast<const char*>(cover_str) : "";

        Playlist pl(pid, name);
        pl.setCoverPath(cover);

        // 加载该歌单的所有曲目
        sqlite3_reset(trk_stmt);
        sqlite3_bind_int64(trk_stmt, 1, static_cast<sqlite3_int64>(pid));
        while (sqlite3_step(trk_stmt) == SQLITE_ROW) {
            Track t;
            t.id = static_cast<uint64_t>(sqlite3_column_int64(trk_stmt, 0));
            const unsigned char* title_str = sqlite3_column_text(trk_stmt, 1);
            t.title = title_str ? reinterpret_cast<const char*>(title_str) : "";
            const unsigned char* artist_str = sqlite3_column_text(trk_stmt, 2);
            t.artist = artist_str ? reinterpret_cast<const char*>(artist_str) : "";
            const unsigned char* album_str = sqlite3_column_text(trk_stmt, 3);
            t.album = album_str ? reinterpret_cast<const char*>(album_str) : "";
            const unsigned char* path_str = sqlite3_column_text(trk_stmt, 4);
            t.file_path = path_str ? reinterpret_cast<const char*>(path_str) : "";
            t.format = static_cast<AudioFormat>(sqlite3_column_int(trk_stmt, 5));
            t.sample_rate = static_cast<uint32_t>(sqlite3_column_int64(trk_stmt, 6));
            t.bit_depth = static_cast<uint8_t>(sqlite3_column_int(trk_stmt, 7));
            t.duration_sec = static_cast<uint32_t>(sqlite3_column_int64(trk_stmt, 8));

            pl.addTrack(std::move(t));
        }

        playlists.push_back(std::move(pl));
    }

    sqlite3_finalize(trk_stmt);
    sqlite3_finalize(pl_stmt);

    return playlists;
}
