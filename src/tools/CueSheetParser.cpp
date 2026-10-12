#include "tools/CueSheetParser.hpp"
#include "types/SupportedFormats.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>

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
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
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
        // 降级尝试 GBK
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
            jbyteArray bytes = env->NewByteArray(raw_bytes.size());
            env->SetByteArrayRegion(bytes, 0, raw_bytes.size(), reinterpret_cast<const jbyte*>(raw_bytes.data()));
            jstring charset = env->NewStringUTF("GB18030");
            jclass str_cls = env->FindClass("java/lang/String");
            jmethodID ctor = env->GetMethodID(str_cls, "<init>", "([BLjava/lang/String;)V");
            jstring result_str = static_cast<jstring>(env->NewObject(str_cls, ctor, bytes, charset));
            std::string result;
            if (result_str) {
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
            if (need_detach) {
                g_android_jvm->DetachCurrentThread();
            }
            if (!result.empty()) return result;
        }
    }
#elif defined(__linux__)
    iconv_t cd = iconv_open("UTF-8", "GB18030");
    if (cd == (iconv_t)-1) {
        cd = iconv_open("UTF-8", "GBK");
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

    // 若转码未成功，作为安全兜底返回原串
    return raw_bytes;
}

double CueSheetParser::parseTimestamp(const std::string& time_str) {
    // 格式 mm:ss:ff (ff 为帧数，1 秒 = 75 帧)
    int mm = 0, ss = 0, ff = 0;
    char colon1 = ':', colon2 = ':';
    std::istringstream iss(time_str);
    if (iss >> mm >> colon1 >> ss >> colon2 >> ff) {
        return static_cast<double>(mm) * 60.0 + static_cast<double>(ss) + static_cast<double>(ff) / 75.0;
    }
    return 0.0;
}

std::string CueSheetParser::resolveAudioFile(const std::filesystem::path& cue_dir, const std::string& filename) {
    std::error_code ec;

    // 1. 同级目录下直接完全匹配
    std::filesystem::path direct = cue_dir / filename;
    if (std::filesystem::exists(direct, ec) && std::filesystem::is_regular_file(direct, ec)) {
        return direct.string();
    }

    // 2. 大小写不敏感匹配 (兼容 Linux/Android 大小写敏感文件系统)
    for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string entry_name = entry.path().filename().string();
        if (entry_name.size() == filename.size()) {
            bool match = true;
            for (size_t i = 0; i < filename.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(entry_name[i])) !=
                    std::tolower(static_cast<unsigned char>(filename[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return entry.path().string();
            }
        }
    }

    // 3. 同名其他格式兜底匹配 (如 CUE 记录的是 CDImage.wav，但实际为 CDImage.flac 或同名母带)
    std::filesystem::path target_stem_path(filename);
    std::string target_stem = target_stem_path.stem().string();

    for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        if (AudioFormatUtils::isSupported(ext)) {
            if (entry.path().stem().string() == target_stem) {
                return entry.path().string();
            }
        }
    }

    // 4. 目录内唯一主流音频文件自适应 (单曲整轨 CD 常见模式)
    std::vector<std::filesystem::path> supported_audio_files;
    for (const auto& entry : std::filesystem::directory_iterator(cue_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
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

static std::string extractQuotedString(const std::string& line, size_t prefix_len) {
    if (line.size() <= prefix_len) return "";
    std::string rest = line.substr(prefix_len);
    // 去除前导空白
    size_t first_char = rest.find_first_not_of(" \t");
    if (first_char == std::string::npos) return "";
    rest = rest.substr(first_char);

    if (!rest.empty() && rest.front() == '"') {
        size_t end_quote = rest.find('"', 1);
        if (end_quote != std::string::npos) {
            return rest.substr(1, end_quote - 1);
        }
        return rest.substr(1);
    }
    // 未带引号，截取到下一个空格或行末
    size_t end_sp = rest.find_first_of(" \t\r\n");
    if (end_sp != std::string::npos) {
        return rest.substr(0, end_sp);
    }
    return rest;
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
        // 移除行首行尾空白及回车
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t start_idx = line.find_first_not_of(" \t");
        if (start_idx == std::string::npos) continue;
        line = line.substr(start_idx);

        if (line.rfind("PERFORMER", 0) == 0) {
            std::string val = extractQuotedString(line, 9);
            if (current_track.has_value()) {
                current_track->performer = val;
            } else {
                sheet.album_performer = val;
            }
        } else if (line.rfind("TITLE", 0) == 0) {
            std::string val = extractQuotedString(line, 5);
            if (current_track.has_value()) {
                current_track->title = val;
            } else {
                sheet.album_title = val;
            }
        } else if (line.rfind("FILE", 0) == 0) {
            current_file = extractQuotedString(line, 4);
            sheet.referenced_audio_files.push_back(current_file);
        } else if (line.rfind("TRACK", 0) == 0) {
            if (current_track.has_value()) {
                sheet.tracks.push_back(*current_track);
                current_track.reset();
            }
            CueTrackInfo t;
            t.file_reference = current_file;
            std::istringstream tr_ss(line.substr(5));
            tr_ss >> t.track_number;
            t.performer = sheet.album_performer; // 默认继承专辑艺术家
            current_track = t;
        } else if (line.rfind("INDEX 01", 0) == 0) {
            std::string time_str = line.substr(8);
            size_t p = time_str.find_first_not_of(" \t");
            if (p != std::string::npos) time_str = time_str.substr(p);
            if (current_track.has_value()) {
                current_track->start_time_sec = parseTimestamp(time_str);
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
        t.resolved_audio_path = resolveAudioFile(cue_dir, ref);
    }

    // 按照音频文件分组计算分轨时间
    for (size_t i = 0; i < sheet.tracks.size(); ++i) {
        if (i + 1 < sheet.tracks.size() && sheet.tracks[i].resolved_audio_path == sheet.tracks[i + 1].resolved_audio_path) {
            sheet.tracks[i].end_time_sec = sheet.tracks[i + 1].start_time_sec;
            double dur = sheet.tracks[i].end_time_sec - sheet.tracks[i].start_time_sec;
            sheet.tracks[i].duration_sec = static_cast<uint32_t>(std::max(0.0, std::round(dur)));
        } else {
            // 最后一首或当前文件最后一个分轨，end_time_sec = 0.0 表示播至物理文件末尾 (EOF)
            sheet.tracks[i].end_time_sec = 0.0;
            sheet.tracks[i].duration_sec = 0; // 后续扫描时由实际音频总时长相减填充
        }
    }

    return sheet;
}
