#include "tools/CueSheetParser.hpp"
#include "types/SupportedFormats.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/stat.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(__ANDROID__)
#include <jni.h>
extern "C" {
    extern JavaVM* g_android_jvm;
}
#elif defined(__linux__)
#include <iconv.h>
#endif

bool CueSheetParser::isValidUtf8(const std::string& str) {
    size_t i = 0;
    const size_t len = str.size();
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c <= 0x7F) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) return false;
            if (c == 0xE0 && (static_cast<unsigned char>(str[i + 1]) < 0xA0)) return false;
            if (c == 0xED && (static_cast<unsigned char>(str[i + 1]) >= 0xA0)) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) return false;
            if (c == 0xF0 && (static_cast<unsigned char>(str[i + 1]) < 0x90)) return false;
            if (c == 0xF4 && (static_cast<unsigned char>(str[i + 1]) >= 0x90)) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

std::string CueSheetParser::sanitizeUtf8(const std::string& str) {
    if (isValidUtf8(str)) return str;

    std::string out;
    out.reserve(str.size());
    size_t i = 0;
    const size_t len = str.size();
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c <= 0x7F) {
            out.push_back(str[i]);
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < len &&
                   (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80 && c >= 0xC2) {
            out.push_back(str[i]);
            out.push_back(str[i + 1]);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len &&
                   (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(str[i + 2]) & 0xC0) == 0x80 &&
                   !(c == 0xE0 && (static_cast<unsigned char>(str[i + 1]) < 0xA0)) &&
                   !(c == 0xED && (static_cast<unsigned char>(str[i + 1]) >= 0xA0))) {
            out.push_back(str[i]);
            out.push_back(str[i + 1]);
            out.push_back(str[i + 2]);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len &&
                   (static_cast<unsigned char>(str[i + 1]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(str[i + 2]) & 0xC0) == 0x80 &&
                   (static_cast<unsigned char>(str[i + 3]) & 0xC0) == 0x80 &&
                   !(c == 0xF0 && (static_cast<unsigned char>(str[i + 1]) < 0x90)) &&
                   !(c == 0xF4 && (static_cast<unsigned char>(str[i + 1]) >= 0x90))) {
            out.push_back(str[i]);
            out.push_back(str[i + 1]);
            out.push_back(str[i + 2]);
            out.push_back(str[i + 3]);
            i += 4;
        } else {
            out.push_back('?');
            i++;
        }
    }
    return out;
}

std::string CueSheetParser::convertGbkToUtf8(const std::string& raw_bytes) {
    if (raw_bytes.empty()) return "";

    // 1. 检查是否存在 UTF-8 BOM (\xEF\xBB\xBF)
    if (raw_bytes.size() >= 3 &&
        static_cast<unsigned char>(raw_bytes[0]) == 0xEF &&
        static_cast<unsigned char>(raw_bytes[1]) == 0xBB &&
        static_cast<unsigned char>(raw_bytes[2]) == 0xBF) {
        return raw_bytes.substr(3);
    }

    // 2. 如果本身就是合法的 UTF-8，直接返回
    if (isValidUtf8(raw_bytes)) {
        return raw_bytes;
    }

    // 3. 否则尝试作为 GB18030 / GBK 解码
#if defined(__APPLE__)
    CFStringRef cfstr = CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(raw_bytes.data()),
        raw_bytes.size(),
        kCFStringEncodingGB_18030_2000,
        false
    );
    if (!cfstr) {
        cfstr = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(raw_bytes.data()),
            raw_bytes.size(),
            kCFStringEncodingGBK_95,
            false
        );
    }
    if (cfstr) {
        CFIndex length = CFStringGetLength(cfstr);
        CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::vector<char> utf8_buf(max_size, '\0');
        if (CFStringGetCString(cfstr, utf8_buf.data(), max_size, kCFStringEncodingUTF8)) {
            CFRelease(cfstr);
            return std::string(utf8_buf.data());
        }
        CFRelease(cfstr);
    }
#elif defined(__ANDROID__)
    if (g_android_jvm) {
        JNIEnv* env = nullptr;
        bool need_detach = false;
        if (g_android_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (g_android_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK && env) {
                need_detach = true;
            }
        }
        if (env) {
            auto convertWithCharset = [&](const char* charset_name) -> std::string {
                jbyteArray bytes = env->NewByteArray(raw_bytes.size());
                if (!bytes) return "";
                env->SetByteArrayRegion(bytes, 0, raw_bytes.size(), reinterpret_cast<const jbyte*>(raw_bytes.data()));
                jstring charset = env->NewStringUTF(charset_name);
                jclass str_cls = env->FindClass("java/lang/String");
                jmethodID ctor = env->GetMethodID(str_cls, "<init>", "([BLjava/lang/String;)V");
                jstring result_str = static_cast<jstring>(env->NewObject(str_cls, ctor, bytes, charset));
                std::string result;
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                } else if (result_str) {
                    const char* chars = env->GetStringUTFChars(result_str, nullptr);
                    if (chars) {
                        result = chars;
                        env->ReleaseStringUTFChars(result_str, chars);
                    }
                    env->DeleteLocalRef(result_str);
                }
                env->DeleteLocalRef(bytes);
                env->DeleteLocalRef(charset);
                env->DeleteLocalRef(str_cls);
                return result;
            };

            std::string res = convertWithCharset("GB18030");
            if (res.empty()) res = convertWithCharset("GBK");
            if (need_detach) {
                g_android_jvm->DetachCurrentThread();
            }
            if (!res.empty()) return res;
        }
    }
#elif defined(__linux__)
    iconv_t cd = iconv_open("UTF-8", "GB18030");
    if (cd == (iconv_t)-1) {
        cd = iconv_open("UTF-8", "GBK");
    }
    if (cd == (iconv_t)-1) {
        cd = iconv_open("UTF-8", "GB2312");
    }
    if (cd != (iconv_t)-1) {
        size_t in_bytes = raw_bytes.size();
        size_t out_bytes = in_bytes * 4 + 1;
        std::vector<char> out_buf(out_bytes, '\0');
        char* in_ptr = const_cast<char*>(raw_bytes.data());
        char* out_ptr = out_buf.data();
        size_t orig_out_bytes = out_bytes;
        if (iconv(cd, &in_ptr, &in_bytes, &out_ptr, &out_bytes) != (size_t)-1) {
            iconv_close(cd);
            return std::string(out_buf.data(), orig_out_bytes - out_bytes);
        }
        iconv_close(cd);
    }
#endif

    // 若系统级转码失败，使用严格 UTF-8 清洗兜底，坚决防止 SQLite 异常
    return sanitizeUtf8(raw_bytes);
}

double CueSheetParser::parseTimestamp(const std::string& time_str) {
    int mm = 0, ss = 0, ff = 0;
    char colon1 = ':', colon2 = ':';
    std::istringstream iss(time_str);
    if (iss >> mm >> colon1 >> ss >> colon2 >> ff) {
        return static_cast<double>(mm) * 60.0 + static_cast<double>(ss) + static_cast<double>(ff) / 75.0;
    }
    std::istringstream iss2(time_str);
    if (iss2 >> mm >> colon1 >> ss) {
        return static_cast<double>(mm) * 60.0 + static_cast<double>(ss);
    }
    return 0.0;
}

static std::string trimWhitespace(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string toUpperAscii(const std::string& s) {
    std::string res = s;
    for (char& c : res) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return res;
}

static std::string cleanQuotedValue(const std::string& text) {
    std::string s = trimWhitespace(text);
    if (s.empty()) return "";

    if (s.front() == '"') {
        size_t end_q = s.find('"', 1);
        if (end_q != std::string::npos) {
            return s.substr(1, end_q - 1);
        }
        return s.substr(1);
    }
    if (s.front() == '\'') {
        size_t end_q = s.find('\'', 1);
        if (end_q != std::string::npos) {
            return s.substr(1, end_q - 1);
        }
        return s.substr(1);
    }
    return s;
}

static std::string cleanAudioFilename(const std::string& raw) {
    std::string s = cleanQuotedValue(raw);
    // 移除末尾的文件类型标识 (如 WAVE, BINARY, FLAC, MP3)
    std::string s_up = toUpperAscii(s);
    for (const char* kw : {" WAVE", " BINARY", " FLAC", " MP3", " AIFF", " MOTOROLA"}) {
        size_t pos = s_up.rfind(kw);
        if (pos != std::string::npos && pos + std::strlen(kw) == s_up.size()) {
            s = trimWhitespace(s.substr(0, pos));
            s = cleanQuotedValue(s);
            break;
        }
    }

    // 剥离 Windows 盘符与路径 (如 "C:\Music\CDImage.wav" 或 "..\CDImage.wav")
    size_t last_slash = s.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        s = s.substr(last_slash + 1);
    }
    return trimWhitespace(s);
}

static bool isRegularOrReadableFile(const std::filesystem::path& p) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(p, ec)) return true;
    if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) return true;
    if (access(p.string().c_str(), R_OK) == 0) {
        struct stat st;
        if (stat(p.string().c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) return true;
    }
    return false;
}

std::string CueSheetParser::resolveAudioFile(const std::filesystem::path& cue_dir, const std::string& filename, const std::string& cue_stem) {
    std::error_code ec;
    std::string clean_name = cleanAudioFilename(filename);

    if (!clean_name.empty()) {
        // 1. 同级目录下完全匹配
        std::filesystem::path direct = cue_dir / clean_name;
        if (isRegularOrReadableFile(direct)) {
            return direct.string();
        }

        // 2. 大小写不敏感匹配 (兼容 Linux / Android / macOS 大小写规范)
        std::string clean_lower = clean_name;
        std::transform(clean_lower.begin(), clean_lower.end(), clean_lower.begin(), ::tolower);

        ec.clear();
        for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
            if (!isRegularOrReadableFile(entry.path())) continue;
            std::string entry_name = entry.path().filename().string();
            std::string entry_lower = entry_name;
            std::transform(entry_lower.begin(), entry_lower.end(), entry_lower.begin(), ::tolower);
            if (entry_lower == clean_lower) {
                return entry.path().string();
            }
        }

        // 3. 同名格式转写匹配 (如 CUE 记录的是 CDImage.wav，但实际为 CDImage.flac 或 CDImage.ape / CDImage.dsf)
        std::filesystem::path target_stem_path(clean_name);
        std::string target_stem = target_stem_path.stem().string();
        std::string target_stem_lower = target_stem;
        std::transform(target_stem_lower.begin(), target_stem_lower.end(), target_stem_lower.begin(), ::tolower);

        ec.clear();
        for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
            if (!isRegularOrReadableFile(entry.path())) continue;
            std::string ext = entry.path().extension().string();
            if (AudioFormatUtils::isSupported(ext)) {
                std::string stem_lower = entry.path().stem().string();
                std::transform(stem_lower.begin(), stem_lower.end(), stem_lower.begin(), ::tolower);
                if (stem_lower == target_stem_lower) {
                    return entry.path().string();
                }
            }
        }
    }

    // 4. 目录内同名 CUE 关联匹配 (如 CDImage.cue 关联同目录 CDImage.wav)
    std::string cue_stem_target = cue_stem.empty() ? cue_dir.filename().string() : cue_stem;
    std::string cue_stem_lower = cue_stem_target;
    std::transform(cue_stem_lower.begin(), cue_stem_lower.end(), cue_stem_lower.begin(), ::tolower);

    ec.clear();
    for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
        if (!isRegularOrReadableFile(entry.path())) continue;
        std::string ext = entry.path().extension().string();
        if (AudioFormatUtils::isSupported(ext)) {
            std::string stem_lower = entry.path().stem().string();
            std::transform(stem_lower.begin(), stem_lower.end(), stem_lower.begin(), ::tolower);
            if (stem_lower == cue_stem_lower) {
                return entry.path().string();
            }
        }
    }

    // 5. 目录内单一大母带音频自适应匹配 (发烧整轨 CD 经典模式)
    std::vector<std::filesystem::path> supported_audio_files;
    ec.clear();
    for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
        if (!isRegularOrReadableFile(entry.path())) continue;
        std::string ext = entry.path().extension().string();
        if (AudioFormatUtils::isSupported(ext)) {
            supported_audio_files.push_back(entry.path());
        }
    }
    if (supported_audio_files.size() == 1) {
        return supported_audio_files.front().string();
    }

    return "";
}

std::optional<CueSheet> CueSheetParser::parse(const std::filesystem::path& cue_path) {
    std::error_code ec;
    if (!std::filesystem::exists(cue_path, ec) || !std::filesystem::is_regular_file(cue_path, ec)) {
        return std::nullopt;
    }

    std::ifstream file(cue_path, std::ios::binary);
    if (!file) return std::nullopt;

    std::vector<char> raw_bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string text = convertGbkToUtf8(std::string(raw_bytes.data(), raw_bytes.size()));

    CueSheet sheet;
    sheet.cue_path = cue_path.string();
    std::filesystem::path cue_dir = cue_path.parent_path();

    std::istringstream stream(text);
    std::string line;

    std::string current_file;
    std::optional<CueTrackInfo> current_track;

    while (std::getline(stream, line)) {
        std::string trimmed = trimWhitespace(line);
        if (trimmed.empty()) continue;

        // 提取行首指令关键字 (忽略大小写)
        size_t sp_pos = trimmed.find_first_of(" \t");
        std::string cmd = (sp_pos == std::string::npos) ? trimmed : trimmed.substr(0, sp_pos);
        std::string rest = (sp_pos == std::string::npos) ? "" : trimWhitespace(trimmed.substr(sp_pos));
        std::string cmd_upper = toUpperAscii(cmd);

        if (cmd_upper == "REM") {
            continue;
        } else if (cmd_upper == "PERFORMER" || cmd_upper == "SONGWRITER") {
            std::string val = cleanQuotedValue(rest);
            if (current_track.has_value()) {
                current_track->performer = val;
            } else {
                sheet.album_performer = val;
            }
        } else if (cmd_upper == "TITLE") {
            std::string val = cleanQuotedValue(rest);
            if (current_track.has_value()) {
                current_track->title = val;
            } else {
                sheet.album_title = val;
            }
        } else if (cmd_upper == "FILE") {
            current_file = cleanAudioFilename(rest);
            sheet.referenced_audio_files.push_back(current_file);
        } else if (cmd_upper == "TRACK") {
            if (current_track.has_value()) {
                sheet.tracks.push_back(*current_track);
                current_track.reset();
            }
            CueTrackInfo t;
            t.file_reference = current_file;
            std::istringstream tr_ss(rest);
            tr_ss >> t.track_number;
            t.performer = sheet.album_performer;
            current_track = t;
        } else if (cmd_upper == "INDEX") {
            // INDEX <00|01|0|1> <mm:ss:ff>
            std::istringstream idx_ss(rest);
            int idx_num = 0;
            std::string time_str;
            if (idx_ss >> idx_num >> time_str) {
                if (current_track.has_value()) {
                    if (idx_num == 1) {
                        current_track->start_time_sec = parseTimestamp(time_str);
                    } else if (idx_num == 0) {
                        current_track->pregap_sec = parseTimestamp(time_str);
                    }
                }
            }
        }
    }

    if (current_track.has_value()) {
        sheet.tracks.push_back(*current_track);
    }

    if (sheet.tracks.empty()) {
        return std::nullopt;
    }

    if (sheet.album_title.empty()) {
        sheet.album_title = cue_path.stem().string();
    }
    if (sheet.album_performer.empty()) {
        sheet.album_performer = "未知艺术家";
    }

    // 解析各音轨关联的物理音频路径与起止时间区间
    for (auto& t : sheet.tracks) {
        if (t.performer.empty()) {
            t.performer = sheet.album_performer;
        }
        if (t.title.empty()) {
            t.title = "Track " + std::to_string(t.track_number);
        }
        std::string ref = t.file_reference.empty() ? current_file : t.file_reference;
        t.resolved_audio_path = resolveAudioFile(cue_dir, ref, cue_path.stem().string());
    }

    // 业内核心算法：依据 CUE 的 INDEX 精准切分 WAV 区间
    for (size_t i = 0; i < sheet.tracks.size(); ++i) {
        if (i + 1 < sheet.tracks.size() &&
            !sheet.tracks[i].resolved_audio_path.empty() &&
            sheet.tracks[i].resolved_audio_path == sheet.tracks[i + 1].resolved_audio_path) {
            
            // 下一首音轨有 INDEX 00 前奏时，当前音轨截断在 INDEX 00；否则截断在下一首 INDEX 01
            double next_start = (sheet.tracks[i + 1].pregap_sec > sheet.tracks[i].start_time_sec)
                                ? sheet.tracks[i + 1].pregap_sec
                                : sheet.tracks[i + 1].start_time_sec;
            sheet.tracks[i].end_time_sec = next_start;
            double dur = sheet.tracks[i].end_time_sec - sheet.tracks[i].start_time_sec;
            sheet.tracks[i].duration_sec = static_cast<uint32_t>(std::max(0.0, std::round(dur)));
        } else {
            // 最后一首或当前文件最后一个分轨，end_time_sec = 0.0 表示播至物理文件末尾 (EOF)
            sheet.tracks[i].end_time_sec = 0.0;
            sheet.tracks[i].duration_sec = 0; // 扫描时由实际物理音频总时长相减填充
        }
    }

    return sheet;
}
