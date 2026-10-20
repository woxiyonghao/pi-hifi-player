#include "audio_engine/decoders/CoreAudioPcmDecoder.hpp"
#include <algorithm>
#include <iostream>
#include <limits>

namespace audio_engine {
namespace {
AudioStreamBasicDescription pcmFormat(const AudioFormatSpec& spec) {
    AudioStreamBasicDescription format{};
    format.mSampleRate = spec.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 32;
    format.mChannelsPerFrame = spec.channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = format.mBytesPerFrame = spec.channels * sizeof(float);
    return format;
}
}

CoreAudioPcmDecoder::CoreAudioPcmDecoder(std::unique_ptr<IAudioDecoder> source, AudioFormatSpec output)
    : source_(std::move(source)), output_(output) {
    output_.format = SampleFormat::Float32;
    output_.bit_depth = 32;
}

CoreAudioPcmDecoder::~CoreAudioPcmDecoder() { close(); }

bool CoreAudioPcmDecoder::open(const std::string& filepath) {
    close();
    return source_->open(filepath) && configure();
}

bool CoreAudioPcmDecoder::configure() {
    if (converter_) AudioConverterDispose(converter_);
    converter_ = nullptr;
    const auto inputFormat = pcmFormat(source_->getSpec());
    const auto outputFormat = pcmFormat(output_);
    const auto status = AudioConverterNew(&inputFormat, &outputFormat, &converter_);
    if (status != noErr) {
        std::cerr << "[CoreAudioPcmDecoder] Cannot convert PCM format: " << status << std::endl;
        return false;
    }
    UInt32 quality = kAudioConverterQuality_High;
    AudioConverterSetProperty(converter_, kAudioConverterSampleRateConverterQuality, sizeof(quality), &quality);
    input_.resize(8192 * source_->getSpec().channels);
    return true;
}

OSStatus CoreAudioPcmDecoder::supplyInput(AudioConverterRef, UInt32* packets, AudioBufferList* data,
                                         AudioStreamPacketDescription**, void* context) {
    auto& self = *static_cast<CoreAudioPcmDecoder*>(context);
    const auto channels = self.source_->getSpec().channels;
    const auto frames = self.source_->readFrames(self.input_.data(), std::min<UInt32>(*packets, 8192));
    *packets = static_cast<UInt32>(frames);
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = channels;
    data->mBuffers[0].mDataByteSize = static_cast<UInt32>(frames * channels * sizeof(float));
    data->mBuffers[0].mData = frames ? self.input_.data() : nullptr;
    return noErr;
}

uint64_t CoreAudioPcmDecoder::readFrames(float* buffer, uint64_t max_frames) {
    if (!converter_ || !buffer || max_frames == 0) return 0;
    UInt32 frames = static_cast<UInt32>(std::min<uint64_t>(max_frames,
        std::numeric_limits<UInt32>::max() / (output_.channels * sizeof(float))));
    AudioBufferList data{};
    data.mNumberBuffers = 1;
    data.mBuffers[0] = {output_.channels, static_cast<UInt32>(frames * output_.channels * sizeof(float)), buffer};
    const auto status = AudioConverterFillComplexBuffer(converter_, supplyInput, this, &frames, &data, nullptr);
    if (status != noErr) {
        std::cerr << "[CoreAudioPcmDecoder] PCM conversion failed: " << status << std::endl;
        return 0;
    }
    return frames;
}

bool CoreAudioPcmDecoder::seek(double seconds) {
    if (!source_->seek(seconds)) return false;
    return !converter_ || AudioConverterReset(converter_) == noErr;
}

double CoreAudioPcmDecoder::getDuration() const { return source_->getDuration(); }

void CoreAudioPcmDecoder::close() {
    if (converter_) AudioConverterDispose(converter_);
    converter_ = nullptr;
    source_->close();
}
} // namespace audio_engine
