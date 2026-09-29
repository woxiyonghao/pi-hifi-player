#pragma once

#include "audio_engine/IAudioDecoder.hpp"
#include <string>
#include <fstream>
#include <vector>

namespace audio_engine {

/**
 * @brief 发烧级 DSF (Direct Stream Digital) 原生流解析器
 * 直接解析 Sony DSF 1-bit 发烧母带结构，杜绝系统粗暴转换。
 * 支持 DSD64 (2.8224MHz)、DSD128 (5.6448MHz) 及更高规格。
 */
class DsfParser : public IAudioDecoder {
public:
    DsfParser();
    ~DsfParser() override;

    bool open(const std::string& filepath) override;
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double target_seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override;
    void close() override;

private:
    std::ifstream file_;
    AudioFormatSpec spec_;
    double duration_sec_ = 0.0;

    uint32_t dsd_sample_rate_ = 2822400; // 原始 1-bit 采样率 (如 2.8224MHz)
    uint32_t channels_ = 2;
    uint64_t sample_count_per_ch_ = 0;   // 每声道总 1-bit 采样点数
    uint32_t block_size_ = 4096;         // 每个 Block 的声道字节块大小 (默认 4096)

    uint64_t data_offset_ = 0;           // data payload 起始文件偏移
    uint64_t data_size_ = 0;             // data payload 字节总大小
    uint64_t current_sample_idx_ = 0;    // 当前已读取的 DSD 样点序号

    // 声道交错读取缓冲区 (双声道各分配 block_size_ 大小)
    std::vector<uint8_t> block_buffer_left_;
    std::vector<uint8_t> block_buffer_right_;
    size_t current_block_pos_ = 0;       // 当前 block 内消费的字节数
    bool has_active_block_ = false;

    // 抽样抽取比率 (例如 DSD64 2822400Hz / 64 = 44100Hz PCM 输出)
    static constexpr uint32_t DECIMATION_RATIO = 64;

    // 二阶超声量化噪声平滑低通滤波器状态
    float lp_l_ = 0.0f;
    float lp_r_ = 0.0f;

    bool loadNextBlock();
};

} // namespace audio_engine
