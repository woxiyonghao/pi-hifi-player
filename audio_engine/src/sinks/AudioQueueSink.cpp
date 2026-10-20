#include "audio_engine/sinks/AudioQueueSink.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
#include "audio_engine/MacAudioOutput.hpp"
#endif

namespace audio_engine {

AudioQueueSink::AudioQueueSink() = default;

AudioQueueSink::~AudioQueueSink() {
    close();
}

void AudioQueueSink::outputCallbackThunk(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    auto* self = static_cast<AudioQueueSink*>(inUserData);
    if (!self) {
        return;
    }

    uint32_t frame_count = self->buffer_size_samples_;
    size_t byte_count = frame_count * self->actual_spec_.channels * sizeof(float);
    if (byte_count > inBuffer->mAudioDataBytesCapacity) {
        frame_count = inBuffer->mAudioDataBytesCapacity / (self->actual_spec_.channels * sizeof(float));
        byte_count = frame_count * self->actual_spec_.channels * sizeof(float);
    }

    auto* float_stream = reinterpret_cast<float*>(inBuffer->mAudioData);
    if (self->is_running_.load(std::memory_order_acquire) && self->callback_) {
        self->callback_(float_stream, frame_count);
    } else {
        // Pause 与 AudioQueue 回调可能并发。即使暂停标志已置位，也必须把
        // 当前取出的 buffer 以静音重新入队，否则恢复播放时队列会逐个丢空。
        std::memset(inBuffer->mAudioData, 0, byte_count);
    }

    inBuffer->mAudioDataByteSize = static_cast<UInt32>(byte_count);
    const auto status = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, nullptr);
    // AudioQueueStop/Reset may return EnqueueDuringReset for an in-flight
    // callback. That is expected during teardown, not a playback failure.
    if (status != noErr && self->is_running_.load()) self->last_error_.store(status);
}

bool AudioQueueSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();
    callback_ = callback;
    last_error_.store(noErr);

    AudioStreamBasicDescription asbd{};
    asbd.mSampleRate = static_cast<Float64>(requested_spec.sample_rate > 0 ? requested_spec.sample_rate : 44100);
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    asbd.mBitsPerChannel = 32;
    asbd.mChannelsPerFrame = requested_spec.channels > 0 ? requested_spec.channels : 2;
    asbd.mBytesPerFrame = asbd.mChannelsPerFrame * sizeof(float);
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;

    OSStatus err = AudioQueueNewOutput(&asbd, outputCallbackThunk, this, nullptr, nullptr, 0, &audio_queue_);
    if (err != noErr || !audio_queue_) {
        last_error_.store(err != noErr ? err : -1);
        std::cerr << "[AudioQueueSink] AudioQueueNewOutput failed with error: " << err << std::endl;
        return false;
    }

    actual_spec_ = requested_spec;
    actual_spec_.sample_rate = static_cast<uint32_t>(asbd.mSampleRate);
    actual_spec_.channels = asbd.mChannelsPerFrame;
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;

#if TARGET_OS_OSX
    // Share the engine's route policy, including built-in fallback for monitors.
    const AudioDeviceID device = getPlaybackOutputDeviceID();
    if (device != kAudioObjectUnknown) {
        CFStringRef uid = nullptr;
        UInt32 size = sizeof(uid);
        AudioObjectPropertyAddress address{kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &uid) == noErr && uid) {
            err = AudioQueueSetProperty(audio_queue_, kAudioQueueProperty_CurrentDevice, &uid, sizeof(uid));
            CFRelease(uid);
            if (err != noErr) {
                if (device == getSystemDefaultOutputDeviceID()) {
                    // Some Bluetooth drivers reject explicit binding. The
                    // default route is safe here because it is already audio.
                    std::cerr << "[AudioQueueSink] Using compatible system audio route: " << err << std::endl;
                } else {
                    last_error_.store(err);
                    std::cerr << "[AudioQueueSink] Output binding failed: " << err << std::endl;
                    close();
                    return false;
                }
            }
        } else {
            last_error_.store(-1);
            close();
            return false;
        }
    } else {
        last_error_.store(-1);
        close();
        return false;
    }
#endif
    // Maintain generous PCM buffers; tiny user preferences must
    // never starve a Bluetooth output queue. Do not change this while open.
    const uint32_t minimum = actual_spec_.sample_rate > 96000 ? 8192 : 4096;
    buffer_size_samples_ = std::clamp(requested_buffer_frames_.load(), minimum, uint32_t(16384));

    uint32_t buffer_byte_size = buffer_size_samples_ * actual_spec_.channels * sizeof(float);
    for (int i = 0; i < kNumBuffers; ++i) {
        err = AudioQueueAllocateBuffer(audio_queue_, buffer_byte_size, &buffers_[i]);
        if (err != noErr) {
            last_error_.store(err);
            std::cerr << "[AudioQueueSink] AudioQueueAllocateBuffer failed with error: " << err << std::endl;
            close();
            return false;
        }
    }

    is_stopped_ = true;
    buffer_latency_sec_ = (actual_spec_.sample_rate > 0) ?
        (static_cast<double>(buffer_size_samples_) / actual_spec_.sample_rate) : 0.0;

    return true;
}

void AudioQueueSink::start() {
    if (audio_queue_) {
        last_error_.store(noErr);
        is_running_.store(true, std::memory_order_release);
        // 若此前处于停止状态 (AudioQueueStop 已清空队列中所有 buffer)，向 AudioQueue 投递预填充 buffer
        if (is_stopped_) {
            for (int i = 0; i < kNumBuffers; ++i) {
                if (buffers_[i]) {
                    const uint32_t frame_count = std::min<uint32_t>(buffer_size_samples_,
                        buffers_[i]->mAudioDataBytesCapacity / (actual_spec_.channels * sizeof(float)));
                    const uint32_t buffer_byte_size = frame_count * actual_spec_.channels * sizeof(float);
                    auto* float_stream = reinterpret_cast<float*>(buffers_[i]->mAudioData);
                    if (callback_) {
                        callback_(float_stream, frame_count);
                    } else {
                        std::memset(buffers_[i]->mAudioData, 0, buffer_byte_size);
                    }
                    buffers_[i]->mAudioDataByteSize = buffer_byte_size;
                    const auto enqueue_error = AudioQueueEnqueueBuffer(audio_queue_, buffers_[i], 0, nullptr);
                    if (enqueue_error != noErr) last_error_.store(enqueue_error);
                }
            }
            is_stopped_ = false;
        }
        OSStatus err = AudioQueueStart(audio_queue_, nullptr);
        if (err != noErr) {
            is_running_.store(false);
            last_error_.store(err);
            std::cerr << "[AudioQueueSink] AudioQueueStart failed with error: " << err << std::endl;
        }
    }
}

void AudioQueueSink::pause() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueuePause(audio_queue_);
        // 注意：pause 不重置 is_stopped_，因为 AudioQueuePause 保留队列中已投递的 buffer，恢复时直接 Start 即可
    }
}

void AudioQueueSink::stop() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueueStop(audio_queue_, true);
        is_stopped_ = true;
    }
}

void AudioQueueSink::close() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueueStop(audio_queue_, true);
        AudioQueueDispose(audio_queue_, true);
        audio_queue_ = nullptr;
        for (int i = 0; i < kNumBuffers; ++i) {
            buffers_[i] = nullptr;
        }
        is_stopped_ = true;
    }
}

void AudioQueueSink::flush() {
    if (audio_queue_) {
        // 重置 AudioQueue，清空硬件底层所有残留排队缓冲帧
        AudioQueueReset(audio_queue_);
        is_stopped_ = true;
        if (is_running_.load(std::memory_order_acquire)) {
            start();
        }
    }
}

double AudioQueueSink::getHardwareLatencySec() const {
    return buffer_latency_sec_;
}

bool AudioQueueSink::isOpen() const {
    return audio_queue_ != nullptr;
}

AudioFormatSpec AudioQueueSink::getActualSpec() const {
    return actual_spec_;
}

} // namespace audio_engine
