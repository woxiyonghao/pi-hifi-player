#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sqlite3.h>

#include "audio_engine/AudioEngine.hpp"
#include "audio_engine/decoders/FlacDecoder.hpp"
#include "audio_engine/decoders/WavDecoder.hpp"
#include "audio_engine/decoders/Mp3Decoder.hpp"
#include "audio_engine/decoders/AppleAudioDecoder.hpp"
#include "audio_engine/DsfParser.hpp"

using namespace audio_engine;

struct TrackRow {
    int id = 0;
    std::string title;
    std::string path;
    int format = 0;
    int is_cue = 0;
    std::string cue_path;
    double start_time = 0.0;
    double end_time = 0.0;
};

int main(int argc, char** argv) {
    const char* db_path = (argc > 1) ? argv[1] : nullptr;
    std::string default_path;
    if (!db_path) {
        const char* home = std::getenv("HOME");
        if (!home) home = "";
        default_path = std::string(home) + "/.config/hifi_player/library.db";
        db_path = default_path.c_str();
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "🎵 HiFi Audio Engine Full Library Playback & Decoder Audit" << std::endl;
    std::cout << "Target DB: " << db_path << std::endl;
    std::cout << "==================================================" << std::endl;

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        std::cerr << "❌ Failed to open database: " << db_path << std::endl;
        return 1;
    }

    std::vector<TrackRow> tracks;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, title, file_path, format, is_cue_track, cue_path, start_time_sec, end_time_sec FROM scanned_tracks ORDER BY id ASC";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TrackRow t;
            t.id = sqlite3_column_int(stmt, 0);
            const char* tit = (const char*)sqlite3_column_text(stmt, 1);
            t.title = tit ? tit : "";
            const char* fp = (const char*)sqlite3_column_text(stmt, 2);
            t.path = fp ? fp : "";
            t.format = sqlite3_column_int(stmt, 3);
            t.is_cue = sqlite3_column_int(stmt, 4);
            const char* cp = (const char*)sqlite3_column_text(stmt, 5);
            t.cue_path = cp ? cp : "";
            t.start_time = sqlite3_column_double(stmt, 6);
            t.end_time = sqlite3_column_double(stmt, 7);
            tracks.push_back(t);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Loaded " << tracks.size() << " tracks from library database." << std::endl;
    if (tracks.empty()) {
        std::cerr << "⚠️ No tracks found in scanned_tracks table." << std::endl;
        return 0;
    }

    int passed = 0;
    int failed = 0;

    for (const auto& t : tracks) {
        if (!std::filesystem::exists(t.path)) {
            std::cout << "❌ [MISSING] ID " << t.id << " File not found on disk: " << t.path << std::endl;
            failed++;
            continue;
        }

        std::string ext = std::filesystem::path(t.path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

        std::unique_ptr<IAudioDecoder> decoder;
        if (ext == ".flac") {
            decoder = std::make_unique<FlacDecoder>();
        } else if (ext == ".wav") {
            decoder = std::make_unique<WavDecoder>();
        } else if (ext == ".mp3") {
            decoder = std::make_unique<Mp3Decoder>();
        } else if (ext == ".dsf" || ext == ".dff") {
            decoder = std::make_unique<DsfParser>();
        } else {
            decoder = std::make_unique<AppleAudioDecoder>();
        }

        bool opened = false;
        std::string decoder_name = "Primary";
        if (decoder && decoder->open(t.path)) {
            opened = true;
        } else {
            // Apple 系统 ExtAudioFile 兜底
            decoder = std::make_unique<AppleAudioDecoder>();
            if (decoder->open(t.path)) {
                opened = true;
                decoder_name = "AppleFallback";
            }
        }

        if (!opened) {
            std::cout << "❌ [FAIL] ID " << t.id << " (" << ext << ") " << t.title << " -> Decoder open() failed!" << std::endl;
            failed++;
            continue;
        }

        auto spec = decoder->getSpec();
        double dur = decoder->getDuration();

        // 1. 测试读取并抽取 2048 帧音频样本
        std::vector<float> pcm_buf(2048 * std::max(1u, spec.channels), 0.0f);
        uint64_t read_frames = decoder->readFrames(pcm_buf.data(), 2048);

        bool finite_samples = true;
        for (size_t i = 0; i < read_frames * spec.channels; ++i) {
            if (!std::isfinite(pcm_buf[i])) {
                finite_samples = false;
                break;
            }
        }

        // 2. 测试 Seek 功能
        double seek_point = t.is_cue ? (t.start_time + 2.0) : std::min(dur * 0.5, 15.0);
        bool seek_ok = decoder->seek(seek_point);
        uint64_t post_seek_frames = 0;
        if (seek_ok) {
            post_seek_frames = decoder->readFrames(pcm_buf.data(), 1024);
        }

        if (read_frames > 0 && finite_samples && (!t.is_cue || seek_ok)) {
            std::cout << "✅ [PASS] ID " << t.id << " [" << ext << " | " << decoder_name << "] "
                      << spec.sample_rate << "Hz " << spec.channels << "ch "
                      << spec.bit_depth << "bit, dur=" << dur << "s : " << t.title;
            if (t.is_cue) {
                std::cout << " (CUE: " << t.start_time << "s -> " << t.end_time << "s)";
            }
            std::cout << std::endl;
            passed++;
        } else {
            std::cout << "⚠️ [WARN] ID " << t.id << " read=" << read_frames
                      << " finite=" << finite_samples << " seek=" << seek_ok
                      << " -> " << t.title << std::endl;
            failed++;
        }
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "AUDIT SUMMARY: " << passed << " PASSED, " << failed << " FAILED out of " << tracks.size() << " tracks." << std::endl;
    std::cout << "==================================================" << std::endl;

    return (failed == 0) ? 0 : 1;
}
