#include "audio_engine/GraphicEqualizer.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace audio_engine;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr size_t frames = 48000;
        std::vector<float> source(frames*2);
        for (size_t f = 0; f < frames; ++f) {
            const auto value = float(0.95 * std::sin(2*3.141592653589793*8000*f/48000));
            source[f*2] = source[f*2+1] = value;
        }
        auto dry = source;
        GraphicEqualizer bypass;
        bypass.process(dry.data(), frames, 2, 48000, {});
        check(dry == source, "Bypass changed PCM samples");

        const std::array<float,10> gains{0,0,.5,1,2,3.5,5,6.5,7.5,8};
        auto expected = source, migrated = source;
        GraphicEqualizer reference, changing_thread;
        for (size_t start = 0; start < frames; start += 4096) {
            const auto count = std::min(size_t(4096), frames-start);
            reference.process(expected.data()+start*2,count,2,48000,gains);
            std::thread callback([&] {
                changing_thread.process(migrated.data()+start*2,count,2,48000,gains);
            });
            callback.join();
        }
        check(expected == migrated, "Filter history changed with the callback thread");
        for (auto value : migrated)
            check(std::isfinite(value) && std::abs(value) <= 1, "Boosted EQ clipped or produced invalid PCM");

        GraphicEqualizer transition;
        float previous = 0;
        for (size_t block = 0; block < 64; ++block) {
            std::array<float,10> boost{};
            boost.fill(block % 2 ? 12 : -12);
            std::array<float,512> pcm{};
            for (size_t f = 0; f < 256; ++f) {
                const auto value = float(0.25*std::sin(2*3.141592653589793*200*(block*256+f)/48000));
                pcm[f*2] = pcm[f*2+1] = value;
            }
            transition.process(pcm.data(),256,2,48000,boost);
            for (size_t f = 0; f < 256; ++f) {
                check(std::isfinite(pcm[f*2]), "EQ transition produced non-finite PCM");
                check(std::abs(pcm[f*2]-previous) < 0.1, "EQ setting change produced an impulse");
                previous = pcm[f*2];
            }
        }
        std::cout << "EQ bypass, callback thread migration, boost headroom, and smooth parameter changes: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
