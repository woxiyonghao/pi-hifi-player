#include "audio_engine/sinks/AudioQueueSink.hpp"
#include <cstring>
#include <iostream>

namespace audio_engine {

AudioQueueSink::AudioQueueSink() = default;

AudioQueueSink::~AudioQueueSink() {
    close();
}

void AudioQueueSink::outputCallbackThunk(void* inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    auto* self = static_cast<AudioQueueSink*>(inUserData);
    if (!self || !self->is_running_.load(std::memory_order_acquire)) {
        std::memset(inBuffer->mAudioData, 0, inBuffer->mAudioDataBytesCapacity);
        inBuffer->mAudioDataByteSize = inBuffer->mAudioDataBytesCapacity;
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, nullptr);
        return;
    }

    uint32_t frame_count = self->buffer_size_samples_;
    size_t byte_count = frame_count * self->actual_spec_.channels * sizeof(float);
    if (byte_count > inBuffer->mAudioDataBytesCapacity) {
        frame_count = inBuffer->mAudioDataBytesCapacity / (self->actual_spec_.channels * sizeof(float));
        byte_count = frame_count * self->actual_spec_.channels * sizeof(float);
    }

    auto* float_stream = reinterpret_cast<float*>(inBuffer->mAudioData);
    if (self->callback_) {
        self->callback_(float_stream, frame_count);
    } else {
        std::memset(inBuffer->mAudioData, 0, byte_count);
    }

    inBuffer->mAudioDataByteSize = static_cast<UInt32>(byte_count);
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, nullptr);
}

bool AudioQueueSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();
    callback_ = callback;

    AudioStreamBasicDescription asbd{};
    asbd.mSampleRate = static_cast<Float64>(requested_spec.sample_rate);
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    asbd.mBitsPerChannel = 32;
    asbd.mChannelsPerFrame = requested_spec.channels;
    asbd.mBytesPerFrame = asbd.mChannelsPerFrame * sizeof(float);
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;

    OSStatus err = AudioQueueNewOutput(&asbd, outputCallbackThunk, this, nullptr, nullptr, 0, &audio_queue_);
    if (err != noErr || !audio_queue_) {
        std::cerr << "[AudioQueueSink] AudioQueueNewOutput failed with error: " << err << std::endl;
        return false;
    }

    actual_spec_ = requested_spec;
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;

    uint32_t buffer_byte_size = buffer_size_samples_ * actual_spec_.channels * sizeof(float);
    for (int i = 0; i < kNumBuffers; ++i) {
        err = AudioQueueAllocateBuffer(audio_queue_, buffer_byte_size, &buffers_[i]);
        if (err != noErr) {
            std::cerr << "[AudioQueueSink] AudioQueueAllocateBuffer failed with error: " << err << std::endl;
            close();
            return false;
        }
        std::memset(buffers_[i]->mAudioData, 0, buffer_byte_size);
        buffers_[i]->mAudioDataByteSize = buffer_byte_size;
        AudioQueueEnqueueBuffer(audio_queue_, buffers_[i], 0, nullptr);
    }

    buffer_latency_sec_ = (actual_spec_.sample_rate > 0) ?
        (static_cast<double>(buffer_size_samples_) / actual_spec_.sample_rate) : 0.0;

    return true;
}

void AudioQueueSink::start() {
    if (audio_queue_) {
        is_running_.store(true, std::memory_order_release);
        AudioQueueStart(audio_queue_, nullptr);
    }
}

void AudioQueueSink::pause() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueuePause(audio_queue_);
    }
}

void AudioQueueSink::stop() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueueStop(audio_queue_, true);
    }
}

void AudioQueueSink::close() {
    if (audio_queue_) {
        is_running_.store(false, std::memory_order_release);
        AudioQueueStop(audio_queue_, true);
        AudioQueueDispose(audio_queue_, true);
        audio_queue_ = nullptr;
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
