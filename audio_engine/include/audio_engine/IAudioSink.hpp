#pragma once

#include "audio_engine/AudioFormatSpec.hpp"
#include <functional>
#include <cstdint>
#include <cstddef>

namespace audio_engine {

/**
 * @brief 音频硬件输出驱动抽象基类
 * 隔离平台差异，实现跨平台 (SDL2) 与树莓派 5 生产环境 (ALSA Direct) 的无缝切换
 */
class IAudioSink {
public:
    /**
     * @brief 硬件数据请求回调 (由声卡推流线程/中断直接触发)
     * @param output 输出浮点缓冲区 (交错立体声)
     * @param frame_count 期望填充的帧数
     */
    using AudioCallback = std::function<void(float* output, size_t frame_count)>;

    virtual ~IAudioSink() = default;

    /**
     * @brief 打开并初始化音频输出设备
     * @param requested_spec 期望匹配的音频物理规格
     * @param callback 帧数据填充回调
     * @return 成功返回 true，失败返回 false
     */
    virtual bool open(const AudioFormatSpec& requested_spec, AudioCallback callback) = 0;

    /**
     * @brief 启动音频输出推流
     */
    virtual void start() = 0;

    /**
     * @brief 暂停音频输出推流 (静音并暂停中断)
     */
    virtual void pause() = 0;

    /**
     * @brief 停止音频推流
     */
    virtual void stop() = 0;

    /**
     * @brief 关闭音频输出设备并释放底层句柄
     */
    virtual void close() = 0;

    /**
     * @brief 获取声卡硬件缓冲区当前的物理延迟 (秒)
     * 用于驱动高精度、晶振级的毫秒播放时钟
     */
    virtual double getHardwareLatencySec() const = 0;

    /**
     * @brief 设备是否处于已打开状态
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief 获取声卡硬件实际协商生效的物理规格
     */
    virtual AudioFormatSpec getActualSpec() const = 0;
};

} // namespace audio_engine
