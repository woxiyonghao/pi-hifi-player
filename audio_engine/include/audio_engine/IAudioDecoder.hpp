#pragma once

#include "audio_engine/AudioFormatSpec.hpp"
#include <string>
#include <cstdint>

namespace audio_engine {

/**
 * @brief 发烧级音频解码器统一抽象基类
 * 规范化各种原生发烧格式 (FLAC/WAV/MP3/DSF) 的输入解码接口
 */
class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    /**
     * @brief 打开指定路径的音频文件并解析头信息
     * @param filepath 音频文件绝对路径
     * @return 成功返回 true，失败返回 false
     */
    virtual bool open(const std::string& filepath) = 0;

    /**
     * @brief 解码交错音频帧到 Float32 标准化缓冲区
     * @param buffer 目标输出内存 (交错布局: L, R, L, R...)
     * @param max_frames 最大请求读取帧数
     * @return 实际解码的帧数，返回 0 表示已到文件结尾 (EOF) 或发生错误
     */
    virtual uint64_t readFrames(float* buffer, uint64_t max_frames) = 0;

    /**
     * @brief 精确跳转到指定时间位置 (秒)
     */
    virtual bool seek(double target_seconds) = 0;

    /**
     * @brief 获取音频总时长 (秒)
     */
    virtual double getDuration() const = 0;

    /**
     * @brief 获取当前音频的精确硬件物理规格
     */
    virtual AudioFormatSpec getSpec() const = 0;

    /**
     * @brief 关闭文件并彻底回收内部资源
     */
    virtual void close() = 0;
};

} // namespace audio_engine
