#include "audio_engine/sinks/AAudioSink.hpp"
#include <cstring>
#include <iostream>

namespace audio_engine {

AAudioSink::AAudioSink() = default;

AAudioSink::~AAudioSink() {
    close();
}

aaudio_data_callback_result_t AAudioSink::dataCallbackThunk(
    AAudioStream* /*stream*/, void* userData, void* audioData, int32_t numFrames) {
    auto* self = static_cast<AAudioSink*>(userData);
    if (!self || numFrames <= 0) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    auto* float_stream = static_cast<float*>(audioData);
    if (self->is_running_.load(std::memory_order_acquire) && self->callback_) {
        self->callback_(float_stream, static_cast<size_t>(numFrames));
    } else {
        std::memset(audioData, 0, numFrames * self->actual_spec_.channels * sizeof(float));
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioSink::errorCallbackThunk(
    AAudioStream* /*stream*/, void* userData, aaudio_result_t error) {
    auto* self = static_cast<AAudioSink*>(userData);
    std::cerr << "[AAudioSink] 硬件错误或路由变更: " << AAudio_convertResultToText(error) << std::endl;
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        if (self) {
            self->is_running_.store(false, std::memory_order_release);
        }
    }
}

bool AAudioSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();
    callback_ = std::move(callback);

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        std::cerr << "[AAudioSink] 无法创建 AAudioStreamBuilder: " << AAudio_convertResultToText(result) << std::endl;
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, requested_spec.sample_rate > 0 ? requested_spec.sample_rate : 44100);
    AAudioStreamBuilder_setChannelCount(builder, requested_spec.channels > 0 ? requested_spec.channels : 2);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    // 优先尝试 EXCLUSIVE 独占流 (Bit-Perfect 直通)
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setDataCallback(builder, dataCallbackThunk, this);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallbackThunk, this);

    result = AAudioStreamBuilder_openStream(builder, &stream_);
    if (result != AAUDIO_OK || !stream_) {
        std::cout << "[AAudioSink] EXCLUSIVE 独占流不可用，尝试降级为 SHARED 共享流: "
                  << AAudio_convertResultToText(result) << std::endl;
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        result = AAudioStreamBuilder_openStream(builder, &stream_);
    }

    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream_) {
        std::cerr << "[AAudioSink] AAudio 打开失败: " << AAudio_convertResultToText(result) << std::endl;
        stream_ = nullptr;
        return false;
    }

    actual_spec_ = requested_spec;
    actual_spec_.sample_rate = AAudioStream_getSampleRate(stream_);
    actual_spec_.channels = AAudioStream_getChannelCount(stream_);
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;

    int32_t burst = AAudioStream_getFramesPerBurst(stream_);
    if (burst > 0) {
        AAudioStream_setBufferSizeInFrames(stream_, burst * 2);
        buffer_size_samples_ = burst * 2;
    }

    aaudio_sharing_mode_t sharing = AAudioStream_getSharingMode(stream_);
    active_device_name_ = (sharing == AAUDIO_SHARING_MODE_EXCLUSIVE)
        ? "Android AAudio (独占硬件直通)"
        : "Android AAudio (共享低延迟)";

    std::cout << "[AAudioSink] 成功开启输出: " << active_device_name_
              << ", 采样率: " << actual_spec_.sample_rate << " Hz"
              << ", 声道: " << actual_spec_.channels
              << ", Burst 帧: " << burst << std::endl;

    return true;
}

void AAudioSink::start() {
    if (!stream_) return;
    is_running_.store(true, std::memory_order_release);
    aaudio_result_t result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        std::cerr << "[AAudioSink] requestStart 失败: " << AAudio_convertResultToText(result) << std::endl;
    }
}

void AAudioSink::pause() {
    if (!stream_) return;
    is_running_.store(false, std::memory_order_release);
    AAudioStream_requestPause(stream_);
}

void AAudioSink::stop() {
    if (!stream_) return;
    is_running_.store(false, std::memory_order_release);
    AAudioStream_requestStop(stream_);
}

void AAudioSink::close() {
    if (stream_) {
        is_running_.store(false, std::memory_order_release);
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

double AAudioSink::getHardwareLatencySec() const {
    if (!stream_ || actual_spec_.sample_rate == 0) return 0.0;
    int32_t buf_frames = AAudioStream_getBufferSizeInFrames(stream_);
    return static_cast<double>(buf_frames) / static_cast<double>(actual_spec_.sample_rate);
}

bool AAudioSink::isOpen() const {
    return stream_ != nullptr;
}

AudioFormatSpec AAudioSink::getActualSpec() const {
    return actual_spec_;
}

void AAudioSink::setBufferSize(uint32_t samples) {
    buffer_size_samples_ = samples;
    if (stream_ && samples > 0) {
        AAudioStream_setBufferSizeInFrames(stream_, static_cast<int32_t>(samples));
    }
}

} // namespace audio_engine
