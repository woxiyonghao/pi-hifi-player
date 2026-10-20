#pragma once

#include "audio_engine/IAudioDecoder.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <memory>
#include <vector>

namespace audio_engine {

// Converts any of our decoders (including DSD-to-PCM) on the decode thread,
// before samples reach the real-time AudioQueue callback.
class CoreAudioPcmDecoder final : public IAudioDecoder {
public:
    CoreAudioPcmDecoder(std::unique_ptr<IAudioDecoder> source, AudioFormatSpec output);
    ~CoreAudioPcmDecoder() override;
    bool open(const std::string& filepath) override;
    bool configure(); // source is already open
    uint64_t readFrames(float* buffer, uint64_t max_frames) override;
    bool seek(double seconds) override;
    double getDuration() const override;
    AudioFormatSpec getSpec() const override { return output_; }
    void close() override;

private:
    static OSStatus supplyInput(AudioConverterRef, UInt32*, AudioBufferList*,
                                AudioStreamPacketDescription**, void*);
    std::unique_ptr<IAudioDecoder> source_;
    AudioFormatSpec output_;
    AudioConverterRef converter_{nullptr};
    std::vector<float> input_;
};

} // namespace audio_engine
