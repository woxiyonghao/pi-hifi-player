#include "audio_engine/decoders/CoreAudioPcmDecoder.hpp"
#include "audio_engine/sinks/AudioQueueSink.hpp"
#include "audio_engine/DacDeviceCatalog.hpp"
#include "audio_engine/AudioEngine.hpp"
#include "audio_engine/MacAudioOutput.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace audio_engine;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class SineDecoder final : public IAudioDecoder {
public:
    SineDecoder(uint32_t rate, uint32_t channels) { spec_.sample_rate = rate; spec_.channels = channels; }
    bool open(const std::string&) override { position_ = 0; return true; }
    uint64_t readFrames(float* data, uint64_t frames) override {
        const auto count = std::min(frames, uint64_t(spec_.sample_rate) - position_);
        for (uint64_t f = 0; f < count; ++f) {
            const float value = 0.25f * std::sin(2 * 3.141592653589793 * 997 * (position_ + f) / spec_.sample_rate);
            for (uint32_t c = 0; c < spec_.channels; ++c) data[f * spec_.channels + c] = value;
        }
        position_ += count;
        return count;
    }
    bool seek(double seconds) override { position_ = uint64_t(seconds * spec_.sample_rate); return true; }
    double getDuration() const override { return 1; }
    AudioFormatSpec getSpec() const override { return spec_; }
    void close() override { position_ = 0; }
private:
    AudioFormatSpec spec_;
    uint64_t position_{0};
};

int main(int argc, char**) {
    try {
        const std::vector<MacOutputDevice> devices{
            {1, kAudioDeviceTransportTypeHDMI}, {2, kAudioDeviceTransportTypeDisplayPort},
            {3, kAudioDeviceTransportTypeBluetooth}, {4, kAudioDeviceTransportTypeUSB},
            {5, kAudioDeviceTransportTypeBuiltIn}};
        for (const auto display : {1u, 2u})
            require(choosePlaybackOutputDevice(display, devices) == 5, "Monitor route did not fall back to built-in");
        for (const auto audio : {3u, 4u, 5u})
            require(choosePlaybackOutputDevice(audio, devices) == audio, "Valid default audio route was changed");
        require(choosePlaybackOutputDevice(1, {{1, kAudioDeviceTransportTypeHDMI},
            {4, kAudioDeviceTransportTypeUSB}}) == 4, "USB fallback failed without a built-in speaker");
        require(choosePlaybackOutputDevice(1, {{1, kAudioDeviceTransportTypeHDMI},
            {2, kAudioDeviceTransportTypeDisplayPort}}) == kAudioObjectUnknown,
            "Monitor-only machine invented an audio output");
        require(choosePlaybackOutputDevice(1, {}) == kAudioObjectUnknown, "Empty list invented an output");
        std::cout << "Monitor exclusion and built-in / Bluetooth / USB routing: passed\n";
        {
            AudioDeviceID system_default = 1;
            std::vector<AudioDeviceID> writes;
            const MacOutputDevice monitor{1, kAudioDeviceTransportTypeHDMI};
            const MacOutputDevice speaker{5, kAudioDeviceTransportTypeBuiltIn};
            const MacOutputDevice headphones{3, kAudioDeviceTransportTypeBluetooth};
            auto read = [&] { return system_default; };
            auto write = [&](AudioDeviceID id) -> OSStatus {
                writes.push_back(id);
                system_default = id;
                return noErr;
            };
            require(synchronizeSystemOutputWithPlaybackDevice(monitor, speaker, read, write) == noErr &&
                system_default == speaker.id && writes == std::vector<AudioDeviceID>{speaker.id},
                "System media-volume output was not synchronized with the built-in speaker");
            writes.clear();
            system_default = headphones.id; // User selected AirPods after the worker's first read.
            synchronizeSystemOutputWithPlaybackDevice(monitor, speaker, read, write);
            synchronizeSystemOutputWithPlaybackDevice(headphones, speaker, read, write);
            require(system_default == headphones.id && writes.empty(),
                "Monitor fallback overwrote the user's headphone selection");
            system_default = monitor.id;
            synchronizeSystemOutputWithPlaybackDevice(monitor, monitor, read, write);
            synchronizeSystemOutputWithPlaybackDevice(monitor, {kAudioObjectUnknown, 0}, read, write);
            require(system_default == monitor.id && writes.empty(), "Invalid fallback changed system output");
            require(synchronizeSystemOutputWithPlaybackDevice(monitor, speaker, read,
                [](AudioDeviceID) -> OSStatus { return kAudioHardwareIllegalOperationError; }) ==
                kAudioHardwareIllegalOperationError, "Failed system-output changes were ignored");
            std::cout << "System volume route sync and concurrent headphone selection: passed\n";
        }
        for (const auto bus : {"Bluetooth", "HDMI / DP"}) {
            const auto profile = DacDeviceCatalog::getInstance().matchProfile("System output", false, bus, 48000);
            require(!profile.supports_dsd && !profile.supports_hog_mode && !profile.supports_uac_mode &&
                !profile.supports_dpll && profile.digital_filters.empty(), "System output was identified as a USB DAC");
        }
        for (const auto rate : {44100u, 96000u, 192000u, 384000u}) {
            for (const auto channels : {1u, 2u}) {
                AudioFormatSpec output;
                output.sample_rate = 48000;
                output.channels = channels;
                CoreAudioPcmDecoder pcm(std::make_unique<SineDecoder>(rate, channels), output);
                require(pcm.open("synthetic"), "Converter failed to open");
                std::vector<float> data(2048 * channels);
                uint64_t total = 0;
                double squared_error = 0;
                size_t compared = 0;
                while (const auto frames = pcm.readFrames(data.data(), 2048)) {
                    require(total + frames <= 48100, "Converter did not reach EOF");
                    for (size_t f = 0; f < frames; ++f) {
                        if (total + f < 200 || total + f >= 47800) continue;
                        const double expected = 0.25 * std::sin(2 * 3.141592653589793 * 997 * (total + f) / 48000);
                        const auto value = data[f * channels];
                        require(std::isfinite(value), "Non-finite PCM sample");
                        squared_error += (value - expected) * (value - expected);
                        ++compared;
                    }
                    total += frames;
                }
                require(std::abs(double(total) - 48000) < 100, "Resampling changed duration");
                require(compared && std::sqrt(squared_error / compared) < 0.001, "Discontinuity / distortion in resampled PCM");
                require(pcm.seek(0.5), "Seek failed");
                require(pcm.readFrames(data.data(), 2048) == 2048, "Converter failed after seek");
                std::cout << "PCM " << rate << " -> 48000, " << channels << " channels: passed\n";
            }
        }
        // AirPods may switch to a mono voice profile while a microphone is in
        // use. Its lower rate and channel count must still receive valid PCM.
        {
            AudioFormatSpec mono;
            mono.sample_rate = 16000;
            mono.channels = 1;
            CoreAudioPcmDecoder pcm(std::make_unique<SineDecoder>(192000, 2), mono);
            require(pcm.open("voice-profile"), "Mono voice-profile conversion failed");
            std::vector<float> data(2048);
            uint64_t total = 0;
            double energy = 0;
            while (const auto frames = pcm.readFrames(data.data(), data.size())) {
                require(total + frames <= 16100, "Voice converter did not reach EOF");
                for (size_t i = 0; i < frames; ++i) {
                    require(std::isfinite(data[i]), "Non-finite voice-profile PCM");
                    energy += data[i] * data[i];
                }
                total += frames;
            }
            require(std::abs(double(total) - 16000) < 100 && energy > 1,
                "Voice-profile conversion produced silence / wrong duration");
            std::cout << "PCM stereo 192000 -> mono 16000 voice profile: passed\n";
        }
        if (argc > 1) {
            AudioQueueSink sink;
            AudioFormatSpec spec;
            spec.sample_rate = 48000;
            std::atomic<size_t> calls{0};
            std::atomic<bool> wrong_frames{false};
            require(sink.open(spec, [&](float* samples, size_t frames) {
                if (frames != 4096) wrong_frames.store(true);
                std::fill(samples, samples + frames * 2, 0.0f);
                ++calls;
            }), "AudioQueue open failed");
            sink.start();
            for (auto frames : {64u, 128u, 256u, 512u, 1024u}) {
                sink.setBufferSize(frames);
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
            }
            sink.pause();
            sink.start();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            sink.stop();
            sink.start();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            require(!sink.hasError(), "AudioQueue enqueue / start failed");
            require(calls.load() > 8, "AudioQueue stopped consuming PCM");
            require(!wrong_frames.load(), "Live buffer size changed after allocation");
            std::cout << "AudioQueue buffer changes, pause/resume, stop/start: passed\n";

            // Exercise the actual engine completion flag without audible test
            // tones, using the engine's normal display-filtering route policy.
            const auto path = std::filesystem::temp_directory_path() /
                ("hifi-completion-" + std::to_string(getpid()) + ".wav");
            {
                std::ofstream wav(path, std::ios::binary);
                auto integer = [&](uint32_t value, int bytes) {
                    for (int i = 0; i < bytes; ++i) wav.put(char(value >> (i * 8)));
                };
                constexpr uint32_t data_bytes = 48000 * 2 * 2 * 3;
                wav.write("RIFF", 4); integer(36 + data_bytes, 4);
                wav.write("WAVEfmt ", 8); integer(16, 4); integer(1, 2); integer(2, 2);
                integer(48000, 4); integer(48000 * 4, 4); integer(4, 2); integer(16, 2);
                wav.write("data", 4); integer(data_bytes, 4);
                const std::vector<char> silence(data_bytes, 0);
                wav.write(silence.data(), silence.size());
            }
            auto& engine = AudioEngine::getInstance();
            require(!engine.hasPlaybackCompleted(), "Idle engine falsely completed");
            require(engine.openAndPlay(path.string()), "Completion fixture failed to play");
            require(!engine.hasPlaybackCompleted(), "New playback falsely completed");
            engine.pause();
            require(!engine.hasPlaybackCompleted(), "Pause falsely completed");
            engine.stop();
            require(!engine.hasPlaybackCompleted(), "Output-rebuild stop falsely completed");
            require(engine.openAndPlay(path.string()), "Completion fixture failed to reopen");
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
            while (!engine.hasPlaybackCompleted() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            require(engine.hasPlaybackCompleted(), "Natural EOF did not complete");
            engine.seek(0);
            require(!engine.hasPlaybackCompleted(), "Seek retained stale completion");
            engine.stop();
            require(!engine.hasPlaybackCompleted(), "Stop retained stale completion");
            std::filesystem::remove(path);
            std::cout << "Engine completion: natural EOF only; pause, rebuild, seek and stop passed\n";
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
