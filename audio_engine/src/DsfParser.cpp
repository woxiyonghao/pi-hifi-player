#include "audio_engine/DsfParser.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace audio_engine {

static inline uint32_t readBE32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
}

static inline uint64_t readBE64(std::ifstream& f) {
    uint8_t b[8];
    f.read(reinterpret_cast<char*>(b), 8);
    return (uint64_t(b[0]) << 56) | (uint64_t(b[1]) << 48) | (uint64_t(b[2]) << 40) | (uint64_t(b[3]) << 32) |
           (uint64_t(b[4]) << 24) | (uint64_t(b[5]) << 16) | (uint64_t(b[6]) << 8) | uint64_t(b[7]);
}

DsfParser::DsfParser() = default;

DsfParser::~DsfParser() {
    close();
}

bool DsfParser::open(const std::string& filepath) {
    close();

    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "[DsfParser] 无法打开 DSD 文件: " << filepath << std::endl;
        return false;
    }

    char magic[4];
    file_.read(magic, 4);
    if (file_.gcount() < 4) {
        close();
        return false;
    }

    // 检查是否为 Philips DSDIFF (DFF) 格式 (Magic: 'FRM8')
    if (std::memcmp(magic, "FRM8", 4) == 0) {
        uint64_t form_size = readBE64(file_);
        (void)form_size;

        char form_type[4];
        file_.read(form_type, 4);
        if (std::memcmp(form_type, "DSD ", 4) != 0) {
            std::cerr << "[DsfParser] 不是有效的 DSDIFF 格式 (FormType 不匹配): " << filepath << std::endl;
            close();
            return false;
        }

        uint32_t dff_sample_rate = 0;
        uint32_t dff_channels = 2;
        uint64_t dff_data_offset = 0;
        uint64_t dff_data_size = 0;

        while (file_.good() && !file_.eof()) {
            char chunk_id[4];
            file_.read(chunk_id, 4);
            if (file_.gcount() < 4) break;

            uint64_t chunk_len = readBE64(file_);
            std::streampos chunk_data_pos = file_.tellg();

            if (std::memcmp(chunk_id, "PROP", 4) == 0) {
                char prop_type[4];
                file_.read(prop_type, 4);
                std::streampos prop_end = chunk_data_pos + static_cast<std::streamoff>(chunk_len);
                while (file_.tellg() < prop_end && file_.good()) {
                    char sub_id[4];
                    file_.read(sub_id, 4);
                    if (file_.gcount() < 4) break;
                    uint64_t sub_len = readBE64(file_);
                    std::streampos sub_data_pos = file_.tellg();
                    if (std::memcmp(sub_id, "FS  ", 4) == 0) {
                        dff_sample_rate = readBE32(file_);
                    } else if (std::memcmp(sub_id, "CHNL", 4) == 0) {
                        uint8_t ch_b[2];
                        file_.read(reinterpret_cast<char*>(ch_b), 2);
                        dff_channels = (uint32_t(ch_b[0]) << 8) | uint32_t(ch_b[1]);
                    }
                    file_.seekg(sub_data_pos + static_cast<std::streamoff>(sub_len), std::ios::beg);
                }
                file_.seekg(prop_end, std::ios::beg);
            } else if (std::memcmp(chunk_id, "DSD ", 4) == 0) {
                dff_data_offset = chunk_data_pos;
                dff_data_size = chunk_len;
                break;
            } else {
                file_.seekg(chunk_data_pos + static_cast<std::streamoff>(chunk_len), std::ios::beg);
            }
        }

        if (dff_data_offset == 0 || dff_sample_rate == 0) {
            std::cerr << "[DsfParser] DSDIFF (DFF) 文件解析失败，未找到有效音频数据块: " << filepath << std::endl;
            close();
            return false;
        }

        is_dff_ = true;
        dsd_sample_rate_ = dff_sample_rate;
        channels_ = dff_channels;
        data_offset_ = dff_data_offset;
        data_size_ = dff_data_size;
        block_size_ = 4096;
        sample_count_per_ch_ = (channels_ > 0) ? ((data_size_ * 8) / channels_) : 0;

        spec_.sample_rate = dsd_sample_rate_ / DECIMATION_RATIO;
        spec_.channels = std::min(channels_, 2u);
        spec_.bit_depth = 24;
        spec_.format = SampleFormat::Float32;

        duration_sec_ = (dsd_sample_rate_ > 0) ? (static_cast<double>(sample_count_per_ch_) / dsd_sample_rate_) : 0.0;

        block_buffer_left_.resize(block_size_);
        block_buffer_right_.resize(block_size_);
        current_block_pos_ = block_size_;
        has_active_block_ = false;
        current_sample_idx_ = 0;
        return true;
    }

    // 检查是否为 Sony DSF 格式 (Magic: 'DSD ')
    if (std::memcmp(magic, "DSD ", 4) != 0) {
        std::cerr << "[DsfParser] 不是有效的 DSF/DFF 格式 (Magic 不匹配): " << filepath << std::endl;
        close();
        return false;
    }

    is_dff_ = false;
    uint64_t dsd_chunk_size = 0;
    file_.read(reinterpret_cast<char*>(&dsd_chunk_size), 8);
    file_.seekg(28, std::ios::beg); // 跳过首部 DSD Chunk

    // 2. 读取 'fmt ' Chunk
    char fmt_magic[4];
    file_.read(fmt_magic, 4);
    if (std::memcmp(fmt_magic, "fmt ", 4) != 0) {
        std::cerr << "[DsfParser] 缺少 fmt Chunk: " << filepath << std::endl;
        close();
        return false;
    }

    uint64_t fmt_chunk_size = 0;
    file_.read(reinterpret_cast<char*>(&fmt_chunk_size), 8);
    std::streampos fmt_end_pos = file_.tellg() + static_cast<std::streamoff>(fmt_chunk_size - 12);

    uint32_t format_version = 0, format_id = 0, channel_type = 0;
    file_.read(reinterpret_cast<char*>(&format_version), 4);
    file_.read(reinterpret_cast<char*>(&format_id), 4);
    file_.read(reinterpret_cast<char*>(&channel_type), 4);
    file_.read(reinterpret_cast<char*>(&channels_), 4);
    file_.read(reinterpret_cast<char*>(&dsd_sample_rate_), 4);

    uint32_t bits_per_sample = 0;
    file_.read(reinterpret_cast<char*>(&bits_per_sample), 4);
    file_.read(reinterpret_cast<char*>(&sample_count_per_ch_), 8);
    file_.read(reinterpret_cast<char*>(&block_size_), 4);

    file_.seekg(fmt_end_pos, std::ios::beg);

    // 3. 循环寻找 'data' Chunk
    while (file_.good()) {
        char chunk_id[4];
        file_.read(chunk_id, 4);
        if (!file_.good()) break;

        uint64_t chunk_size = 0;
        file_.read(reinterpret_cast<char*>(&chunk_size), 8);

        if (std::memcmp(chunk_id, "data", 4) == 0) {
            data_offset_ = file_.tellg();
            data_size_ = chunk_size - 12; // 减去 4 字节 tag + 8 字节 size
            break;
        } else {
            // 跳过未知 Chunk
            if (chunk_size > 12) {
                file_.seekg(chunk_size - 12, std::ios::cur);
            }
        }
    }

    if (data_offset_ == 0) {
        std::cerr << "[DsfParser] 未找到 data Chunk" << std::endl;
        close();
        return false;
    }

    // 4. 计算规格与时间
    spec_.sample_rate = dsd_sample_rate_ / DECIMATION_RATIO; // 默认下抽样到 44.1kHz / 88.2kHz 供通用声卡输出
    spec_.channels = std::min(channels_, 2u);
    spec_.bit_depth = 24;
    spec_.format = SampleFormat::Float32;

    duration_sec_ = (dsd_sample_rate_ > 0) ? (static_cast<double>(sample_count_per_ch_) / dsd_sample_rate_) : 0.0;

    block_buffer_left_.resize(block_size_);
    block_buffer_right_.resize(block_size_);
    current_block_pos_ = block_size_; // 触发首帧加载
    has_active_block_ = false;
    current_sample_idx_ = 0;

    return true;
}

bool DsfParser::loadNextBlock() {
    if (!file_.is_open() || file_.eof()) return false;

    if (is_dff_) {
        // DSDIFF (DFF) 格式：字节级声道交错 (L0, R0, L1, R1, ...)
        std::vector<uint8_t> interleaved(block_size_ * channels_);
        file_.read(reinterpret_cast<char*>(interleaved.data()), interleaved.size());
        std::streamsize bytes = file_.gcount();
        if (bytes <= 0) return false;

        size_t frames_in_block = bytes / channels_;
        for (size_t i = 0; i < frames_in_block; ++i) {
            block_buffer_left_[i] = interleaved[i * channels_ + 0];
            if (channels_ >= 2) {
                block_buffer_right_[i] = interleaved[i * channels_ + 1];
            } else {
                block_buffer_right_[i] = block_buffer_left_[i];
            }
        }
        current_block_pos_ = 0;
        has_active_block_ = true;
        return true;
    }

    // DSF 双声道分块格式：Block L (block_size_ 字节), 紧接着 Block R (block_size_ 字节)
    file_.read(reinterpret_cast<char*>(block_buffer_left_.data()), block_size_);
    std::streamsize bytes_l = file_.gcount();
    if (bytes_l <= 0) return false;

    if (channels_ >= 2) {
        file_.read(reinterpret_cast<char*>(block_buffer_right_.data()), block_size_);
    } else {
        std::memcpy(block_buffer_right_.data(), block_buffer_left_.data(), block_size_);
    }

    current_block_pos_ = 0;
    has_active_block_ = true;
    return true;
}

uint64_t DsfParser::readFrames(float* buffer, uint64_t max_frames) {
    if (!file_.is_open() || !buffer || max_frames == 0) return 0;

    uint64_t frames_decoded = 0;
    // 每 1 个 PCM 采样帧消耗 8 个字节 (64 bit DSD 样点)
    constexpr size_t BYTES_PER_PCM_FRAME = DECIMATION_RATIO / 8; // 64 / 8 = 8 字节

    while (frames_decoded < max_frames) {
        if (!has_active_block_ || current_block_pos_ + BYTES_PER_PCM_FRAME > block_size_) {
            if (!loadNextBlock()) {
                break; // EOF
            }
        }

        // 抽取左声道 8 字节
        int ones_l = 0;
        for (size_t b = 0; b < BYTES_PER_PCM_FRAME; ++b) {
            ones_l += __builtin_popcount(block_buffer_left_[current_block_pos_ + b]);
        }
        float raw_l = static_cast<float>(ones_l - 32) / 32.0f;

        // 抽取右声道 8 字节
        int ones_r = 0;
        for (size_t b = 0; b < BYTES_PER_PCM_FRAME; ++b) {
            ones_r += __builtin_popcount(block_buffer_right_[current_block_pos_ + b]);
        }
        float raw_r = static_cast<float>(ones_r - 32) / 32.0f;

        // 低通平滑滤波器，消除 Delta-Sigma 降采样后的高频噪底与沙沙声
        lp_l_ += 0.35f * (raw_l - lp_l_);
        lp_r_ += 0.35f * (raw_r - lp_r_);

        buffer[frames_decoded * 2 + 0] = lp_l_;
        buffer[frames_decoded * 2 + 1] = lp_r_;

        current_block_pos_ += BYTES_PER_PCM_FRAME;
        frames_decoded++;
        current_sample_idx_ += DECIMATION_RATIO;

        if (current_sample_idx_ >= sample_count_per_ch_) {
            break; // 到达标称总样点
        }
    }

    return frames_decoded;
}

bool DsfParser::seek(double target_seconds) {
    if (!file_.is_open() || dsd_sample_rate_ == 0 || block_size_ == 0) return false;

    double clamped_sec = std::clamp(target_seconds, 0.0, duration_sec_);

    if (is_dff_) {
        uint64_t target_sample = static_cast<uint64_t>(clamped_sec * dsd_sample_rate_);
        uint64_t target_byte = target_sample / 8;
        // 对齐到 8 字节 PCM 采样帧边界
        target_byte = (target_byte / (DECIMATION_RATIO / 8)) * (DECIMATION_RATIO / 8);
        uint64_t target_file_pos = data_offset_ + target_byte * channels_;

        file_.clear();
        file_.seekg(static_cast<std::streamoff>(target_file_pos), std::ios::beg);

        if (loadNextBlock()) {
            current_block_pos_ = 0;
            current_sample_idx_ = target_sample;
            lp_l_ = 0.0f;
            lp_r_ = 0.0f;
            return true;
        }
        return false;
    }

    uint64_t target_sample = static_cast<uint64_t>(clamped_sec * dsd_sample_rate_);
    uint64_t target_byte = target_sample / 8;

    uint64_t block_index = target_byte / block_size_;
    uint64_t byte_in_block = target_byte % block_size_;
    // 对齐到 PCM 帧字节边界
    byte_in_block = (byte_in_block / (DECIMATION_RATIO / 8)) * (DECIMATION_RATIO / 8);

    uint64_t block_stride = block_size_ * channels_;
    uint64_t target_file_pos = data_offset_ + block_index * block_stride;

    file_.clear();
    file_.seekg(static_cast<std::streamoff>(target_file_pos), std::ios::beg);

    if (loadNextBlock()) {
        current_block_pos_ = byte_in_block;
        current_sample_idx_ = (block_index * block_size_ + byte_in_block) * 8;
        lp_l_ = 0.0f;
        lp_r_ = 0.0f;
        return true;
    }
    return false;
}

double DsfParser::getDuration() const {
    return duration_sec_;
}

AudioFormatSpec DsfParser::getSpec() const {
    return spec_;
}

void DsfParser::close() {
    if (file_.is_open()) {
        file_.close();
    }
    duration_sec_ = 0.0;
    current_sample_idx_ = 0;
    has_active_block_ = false;
    is_dff_ = false;
    lp_l_ = 0.0f;
    lp_r_ = 0.0f;
}

} // namespace audio_engine
