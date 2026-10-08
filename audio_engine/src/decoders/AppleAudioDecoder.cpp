#if defined(__APPLE__)

#include "audio_engine/decoders/AppleAudioDecoder.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <algorithm>

namespace audio_engine {

AppleAudioDecoder::AppleAudioDecoder() = default;

AppleAudioDecoder::~AppleAudioDecoder() {
    close();
}

bool AppleAudioDecoder::open(const std::string& filepath) {
    close();

    CFStringRef path_cf = CFStringCreateWithCString(kCFAllocatorDefault, filepath.c_str(), kCFStringEncodingUTF8);
    if (!path_cf) return false;
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cf, kCFURLPOSIXPathStyle, false);
    CFRelease(path_cf);
    if (!url) return false;

    OSStatus status = ExtAudioFileOpenURL(url, &ext_file_);
    CFRelease(url);
    if (status != noErr || !ext_file_) {
        std::cerr << "[AppleAudioDecoder] ExtAudioFileOpenURL 失败: " << filepath << " (OSStatus: " << status << ")" << std::endl;
        ext_file_ = nullptr;
        return false;
    }

    AudioStreamBasicDescription file_format{};
    UInt32 prop_size = sizeof(file_format);
    status = ExtAudioFileGetProperty(ext_file_, kExtAudioFileProperty_FileDataFormat, &prop_size, &file_format);
    if (status != noErr) {
        close();
        return false;
    }

    spec_.sample_rate = static_cast<uint32_t>(file_format.mSampleRate > 0 ? file_format.mSampleRate : 44100);
    spec_.channels = file_format.mChannelsPerFrame > 0 ? file_format.mChannelsPerFrame : 2;
    spec_.bit_depth = file_format.mBitsPerChannel > 0 ? static_cast<uint8_t>(file_format.mBitsPerChannel) : 16;
    spec_.format = SampleFormat::Float32;

    // 配置客户端统一输出格式为标准交错 Float32 PCM，由 CoreAudio 内部硬件解码器与重采样器负责解压
    AudioStreamBasicDescription client_format{};
    client_format.mSampleRate = spec_.sample_rate;
    client_format.mFormatID = kAudioFormatLinearPCM;
    client_format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    client_format.mBitsPerChannel = 32;
    client_format.mChannelsPerFrame = spec_.channels;
    client_format.mBytesPerFrame = client_format.mChannelsPerFrame * sizeof(float);
    client_format.mFramesPerPacket = 1;
    client_format.mBytesPerPacket = client_format.mBytesPerFrame;

    status = ExtAudioFileSetProperty(ext_file_, kExtAudioFileProperty_ClientDataFormat, sizeof(client_format), &client_format);
    if (status != noErr) {
        std::cerr << "[AppleAudioDecoder] 设置 ClientDataFormat 失败: " << status << std::endl;
        close();
        return false;
    }

    SInt64 total_frames = 0;
    prop_size = sizeof(total_frames);
    status = ExtAudioFileGetProperty(ext_file_, kExtAudioFileProperty_FileLengthFrames, &prop_size, &total_frames);
    total_frames_ = (status == noErr && total_frames > 0) ? static_cast<uint64_t>(total_frames) : 0;
    duration_sec_ = (spec_.sample_rate > 0) ? (static_cast<double>(total_frames_) / spec_.sample_rate) : 0.0;

    std::cout << "[AppleAudioDecoder] 成功打开音频: " << filepath
              << " | 采样率: " << spec_.sample_rate << "Hz | 声道: " << spec_.channels
              << " | 时长: " << duration_sec_ << "s" << std::endl;

    return true;
}

uint64_t AppleAudioDecoder::readFrames(float* buffer, uint64_t max_frames) {
    if (!ext_file_ || !buffer || max_frames == 0) return 0;

    AudioBufferList buf_list{};
    buf_list.mNumberBuffers = 1;
    buf_list.mBuffers[0].mNumberChannels = spec_.channels;
    buf_list.mBuffers[0].mDataByteSize = static_cast<UInt32>(max_frames * spec_.channels * sizeof(float));
    buf_list.mBuffers[0].mData = buffer;

    UInt32 io_frames = static_cast<UInt32>(max_frames);
    OSStatus status = ExtAudioFileRead(ext_file_, &io_frames, &buf_list);
    if (status != noErr) {
        return 0;
    }
    return io_frames;
}

bool AppleAudioDecoder::seek(double target_seconds) {
    if (!ext_file_ || spec_.sample_rate == 0) return false;
    double clamped = std::clamp(target_seconds, 0.0, duration_sec_);
    SInt64 target_frame = static_cast<SInt64>(clamped * spec_.sample_rate);
    return ExtAudioFileSeek(ext_file_, target_frame) == noErr;
}

double AppleAudioDecoder::getDuration() const {
    return duration_sec_;
}

AudioFormatSpec AppleAudioDecoder::getSpec() const {
    return spec_;
}

void AppleAudioDecoder::close() {
    if (ext_file_) {
        ExtAudioFileDispose(ext_file_);
        ext_file_ = nullptr;
    }
    total_frames_ = 0;
    duration_sec_ = 0.0;
}

} // namespace audio_engine

#endif // __APPLE__
