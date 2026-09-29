#pragma once

#include "types/MusicModel.hpp"
#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

class MusicDatabase {
public:
    static MusicDatabase& getInstance() {
        static MusicDatabase instance;
        return instance;
    }

    MusicDatabase(const MusicDatabase&) = delete;
    MusicDatabase& operator=(const MusicDatabase&) = delete;

    ~MusicDatabase();

    // 初始化并连接本地 SQLite 数据库
    bool init(const std::string& db_path = "");
    void close();

    // 扫描曲库持久化接口
    bool saveScannedTracks(const std::vector<Track>& tracks);
    std::vector<Track> loadScannedTracks();
    void clearScannedTracks();

    // 播放列表持久化接口
    bool savePlaylists(const std::vector<Playlist>& playlists);
    std::vector<Playlist> loadPlaylists();

private:
    MusicDatabase() = default;

    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
    bool is_initialized_ = false;
};
