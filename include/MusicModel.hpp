#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <numeric>
#include <utility>

// 音频无损/母带编码格式
enum class AudioFormat{
    UNKNOWN, // 未知或暂不支持的文件格式 (安全兜底)
    FLAC,
    WAV,
    DSD_DSF, // DSD 原生发烧 1-bit 流 (DSF)
    DSD_DFF,
    ALAC, // Apple 无损
    MP3
};

// ==============================================================================
// 1. 单曲母带元数据模型 (Track)
// ==============================================================================
struct Track{
    uint64_t id = 0;
    std::string title;
    std::string artist;
    std::string album;
    std::string file_path;

    // 默认赋值为 Unknown，避免未解析时误报
    AudioFormat format = AudioFormat::UNKNOWN;
    uint32_t sample_rate = 0;
    uint8_t bit_depth = 0;
    uint32_t duration_sec = 0;
    std::string getFormatBadge() const
    {
        if (format == AudioFormat::UNKNOWN)
        {
            return "UNKNOWN";
        }
        if (format == AudioFormat::DSD_DSF || format == AudioFormat::DSD_DFF)
        {
            return "DSD " + std::to_string(sample_rate / 44100) + "x";
        }
        return std::to_string(sample_rate / 1000) + "kHz / " + std::to_string(bit_depth) + "bit";
    }
};

// ==============================================================================
// 2. 播放列表实体模型 (Playlist)
// ==============================================================================
class Playlist {
public:
    Playlist() = default;
    Playlist(uint64_t id, std::string name): id_(id), name_(std::move(name)) {}
    uint64_t getId() const { return id_; }
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    const std::string& getCoverPath() const { return cover_path_; }
    void setCoverPath(const std::string& path) { cover_path_ = path; }
    const std::vector<Track>& getTracks() const { return tracks_; }
    size_t getTrackCount() const { return tracks_.size(); }
    void addTrack(const Track& track) {
        tracks_.push_back(track);
    }
    void removeTrack(size_t index) {
        if (index < tracks_.size()) {
            tracks_.erase(tracks_.begin() + index);
        }
    }
    // 统计整张歌单的总时长
    uint32_t getTotalDurationSec() const {
        return std::accumulate(tracks_.begin(), tracks_.end(), 0u,
            [](uint32_t sum, const Track& t) { return sum + t.duration_sec; });
    }
private:
    uint64_t id_ = 0;                       // 歌单独立唯一 ID
    std::string name_;                      // 歌单标题
    std::string cover_path_;                // 封面图绝对路径
    std::vector<Track> tracks_;             // 歌单持有的曲目实体列表
};