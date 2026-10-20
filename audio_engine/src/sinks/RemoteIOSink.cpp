#include "audio_engine/sinks/RemoteIOSink.hpp"

#include <cstring>
#include <iostream>

namespace audio_engine {

RemoteIOSink::~RemoteIOSink() {
    close();
}

OSStatus RemoteIOSink::renderCallback(void* ref_con,
                                      AudioUnitRenderActionFlags* /*flags*/,
                                      const AudioTimeStamp* /*timestamp*/,
                                      UInt32 /*bus_number*/,
                                      UInt32 frame_count,
                                      AudioBufferList* io_data) {
    auto* self = static_cast<RemoteIOSink*>(ref_con);
    if (!self || !io_data || io_data->mNumberBuffers == 0) {
        return noErr;
    }

    AudioBuffer& output = io_data->mBuffers[0];
    const size_t byte_count = static_cast<size_t>(frame_count)
        * self->actual_spec_.channels * sizeof(float);

    if (!output.mData || output.mDataByteSize < byte_count) {
        for (UInt32 i = 0; i < io_data->mNumberBuffers; ++i) {
            if (io_data->mBuffers[i].mData) {
                std::memset(io_data->mBuffers[i].mData, 0, io_data->mBuffers[i].mDataByteSize);
            }
        }
        return noErr;
    }

    auto* samples = static_cast<float*>(output.mData);
    if (self->is_running_.load(std::memory_order_acquire) && self->callback_) {
        self->callback_(samples, frame_count);
    } else {
        std::memset(samples, 0, byte_count);
    }

    // 配置为交错 PCM，正常情况下只有一个 buffer；其余 buffer 始终静音。
    for (UInt32 i = 1; i < io_data->mNumberBuffers; ++i) {
        if (io_data->mBuffers[i].mData) {
            std::memset(io_data->mBuffers[i].mData, 0, io_data->mBuffers[i].mDataByteSize);
        }
    }
    return noErr;
}

bool RemoteIOSink::open(const AudioFormatSpec& requested_spec, AudioCallback callback) {
    close();
    callback_ = std::move(callback);

    AudioComponentDescription description{};
    description.componentType = kAudioUnitType_Output;
    description.componentSubType = kAudioUnitSubType_RemoteIO;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &description);
    if (!component) {
        std::cerr << "[RemoteIOSink] RemoteIO component not found" << std::endl;
        return false;
    }

    OSStatus status = AudioComponentInstanceNew(component, &audio_unit_);
    if (status != noErr || !audio_unit_) {
        std::cerr << "[RemoteIOSink] AudioComponentInstanceNew failed: " << status << std::endl;
        audio_unit_ = nullptr;
        return false;
    }

    actual_spec_ = requested_spec;
    actual_spec_.sample_rate = requested_spec.sample_rate > 0 ? requested_spec.sample_rate : 48000;
    actual_spec_.channels = requested_spec.channels > 0 ? requested_spec.channels : 2;
    actual_spec_.bit_depth = 32;
    actual_spec_.format = SampleFormat::Float32;

    AudioStreamBasicDescription format{};
    format.mSampleRate = actual_spec_.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 32;
    format.mChannelsPerFrame = actual_spec_.channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = actual_spec_.channels * sizeof(float);
    format.mBytesPerPacket = format.mBytesPerFrame;

    status = AudioUnitSetProperty(audio_unit_,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &format,
                                  sizeof(format));
    if (status != noErr) {
        std::cerr << "[RemoteIOSink] set stream format failed: " << status << std::endl;
        close();
        return false;
    }

    AURenderCallbackStruct render{};
    render.inputProc = renderCallback;
    render.inputProcRefCon = this;
    status = AudioUnitSetProperty(audio_unit_,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input,
                                  0,
                                  &render,
                                  sizeof(render));
    if (status != noErr) {
        std::cerr << "[RemoteIOSink] set render callback failed: " << status << std::endl;
        close();
        return false;
    }

    UInt32 maximum_frames = 4096;
    AudioUnitSetProperty(audio_unit_,
                         kAudioUnitProperty_MaximumFramesPerSlice,
                         kAudioUnitScope_Global,
                         0,
                         &maximum_frames,
                         sizeof(maximum_frames));

    status = AudioUnitInitialize(audio_unit_);
    if (status != noErr) {
        std::cerr << "[RemoteIOSink] AudioUnitInitialize failed: " << status << std::endl;
        close();
        return false;
    }

    std::cout << "[RemoteIOSink] opened: " << actual_spec_.sample_rate
              << " Hz, " << actual_spec_.channels << " channels" << std::endl;
    return true;
}

void RemoteIOSink::start() {
    if (!audio_unit_) return;
    is_running_.store(true, std::memory_order_release);
    OSStatus status = AudioOutputUnitStart(audio_unit_);
    if (status != noErr) {
        is_running_.store(false, std::memory_order_release);
        std::cerr << "[RemoteIOSink] AudioOutputUnitStart failed: " << status << std::endl;
    }
}

void RemoteIOSink::pause() {
    if (!audio_unit_) return;
    is_running_.store(false, std::memory_order_release);
    AudioOutputUnitStop(audio_unit_);
}

void RemoteIOSink::stop() {
    pause();
}

void RemoteIOSink::close() {
    is_running_.store(false, std::memory_order_release);
    if (audio_unit_) {
        AudioOutputUnitStop(audio_unit_);
        AudioUnitUninitialize(audio_unit_);
        AudioComponentInstanceDispose(audio_unit_);
        audio_unit_ = nullptr;
    }
    callback_ = nullptr;
}

double RemoteIOSink::getHardwareLatencySec() const {
    return actual_spec_.sample_rate > 0
        ? static_cast<double>(preferred_buffer_frames_) / actual_spec_.sample_rate
        : 0.0;
}

bool RemoteIOSink::isOpen() const {
    return audio_unit_ != nullptr;
}

AudioFormatSpec RemoteIOSink::getActualSpec() const {
    return actual_spec_;
}

} // namespace audio_engine
