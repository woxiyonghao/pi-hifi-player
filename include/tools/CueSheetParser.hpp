#pragma once

#include "types/MusicModel.hpp"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

struct CueTrackInfo {
    uint32_t track_number = 0;
    std::string title;
    std::string performer;
    std::string file_reference;
    std::string resolved_audio_path;
    double start_time_sec = 0.0;
    double end_time_sec = 0.0;
    double pregap_sec = 0.0;
    uint32_t duration_sec = 0;
};

struct CueSheet {
    std::string cue_path;
    std::string album_title;
    std::string album_performer;
    std::vector<std::string> referenced_audio_files;
    std::vector<CueTrackInfo> tracks;
};

class CueSheetParser {
public:
    // 解析指定路径的 .cue 文件并解析关联的整轨音频 (自动处理 GB18030/GBK 与 UTF-8 字符编码)
    static std::optional<CueSheet> parse(const std::filesystem::path& cue_path);

    // 将 GB18030 / GBK 编码数据安全转换为 UTF-8 (支持 macOS, iOS, Android, Linux)
    static std::string convertGbkToUtf8(const std::string& raw_bytes);

    // 检测字符串是否为合法 UTF-8
    static bool isValidUtf8(const std::string& str);

    // 净化字符串为严格有效 UTF-8，防止 SQLite 事务中断回滚
    static std::string sanitizeUtf8(const std::string& str);

    // 解析标准 CUE 时间戳 mm:ss:ff 为秒数 (每秒 75 帧)
    static double parseTimestamp(const std::string& time_str);

    // 在 CUE 文件同级目录下解析音频物理文件绝对路径 (支持大小写自愈与同名格式匹配)
    static std::string resolveAudioFile(const std::filesystem::path& cue_dir, const std::string& filename, const std::string& cue_stem = "");
};
