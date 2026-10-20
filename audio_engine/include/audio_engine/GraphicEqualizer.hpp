#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>

namespace audio_engine {

// Owned by the stream, not by a callback thread: AudioQueue primes buffers on
// the caller's thread and may render subsequent buffers on different threads.
class GraphicEqualizer {
    static constexpr double pi = 3.14159265358979323846;
    static constexpr std::array<double, 10> frequencies{31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    struct Coefficients { double b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct State { double z1=0, z2=0; };
    struct Bank {
        std::array<Coefficients, 10> coefficients{};
        std::array<std::array<State, 10>, 2> states{};
        double preamp = 1;

        double response(double frequency, double rate) const {
            const auto z = std::polar(1.0, -2 * pi * frequency / rate);
            std::complex<double> gain = 1;
            for (const auto& c : coefficients)
                gain *= (c.b0 + c.b1*z + c.b2*z*z) / (1.0 + c.a1*z + c.a2*z*z);
            return std::abs(gain);
        }

        void configure(const std::array<float, 10>& gains, double rate) {
            states = {};
            for (size_t b = 0; b < gains.size(); ++b) {
                const double gain = std::clamp(double(gains[b]), -12.0, 12.0);
                if (std::abs(gain) < 0.05) { coefficients[b] = {}; continue; }
                const double w = std::min(2*pi*frequencies[b]/rate, pi*0.95);
                const double a = std::sin(w)/(2*1.414);
                const double A = std::pow(10.0, gain/40);
                const double divisor = 1 + a/A;
                coefficients[b] = {(1+a*A)/divisor, -2*std::cos(w)/divisor,
                    (1-a*A)/divisor, -2*std::cos(w)/divisor, (1-a/A)/divisor};
            }
            // Measure the combined filter response, including overlapping
            // boosted bands. Reserve headroom before filtering, not hard clip
            // an already overloaded waveform at the output.
            double maximum = 1;
            for (int i = 0; i <= 512; ++i) {
                const double f = 10 * std::pow(rate*0.049, double(i)/512);
                maximum = std::max(maximum, response(f, rate));
            }
            for (auto f : frequencies) maximum = std::max(maximum, response(std::min(f, rate*0.475), rate));
            preamp = maximum > 1.001 ? 1/(maximum*1.15) : 1;
        }

        float render(float sample, size_t channel) {
            double value = std::isfinite(sample) ? sample * preamp : 0;
            for (size_t b = 0; b < coefficients.size(); ++b) {
                const auto& c = coefficients[b];
                auto& s = states[channel][b];
                const double output = c.b0*value + s.z1;
                s.z1 = c.b1*value - c.a1*output + s.z2;
                s.z2 = c.b2*value - c.a2*output;
                value = output;
            }
            return std::isfinite(value) ? static_cast<float>(value) : 0;
        }
    };

    Bank current_, target_;
    std::array<float, 10> gains_{};
    uint32_t sample_rate_ = 0;
    size_t transition_frames_ = 0;
    static constexpr size_t transition_length = 512;

public:
    void reset() { *this = GraphicEqualizer{}; }

    void process(float* samples, size_t frames, uint32_t channels, uint32_t rate,
                 const std::array<float, 10>& gains) {
        if (!samples || channels == 0 || rate == 0) return;
        if (sample_rate_ != rate) {
            sample_rate_ = rate;
            gains_ = gains;
            current_.configure(gains, rate);
            transition_frames_ = 0;
        } else if (gains_ != gains && transition_frames_ == 0) {
            gains_ = gains;
            target_.configure(gains, rate);
            transition_frames_ = transition_length;
        }
        for (size_t f = 0; f < frames; ++f) {
            const float mix = transition_frames_ ?
                float(transition_length-transition_frames_+1)/transition_length : 0;
            for (uint32_t ch = 0; ch < std::min(channels, 2u); ++ch) {
                const auto index = f*channels+ch;
                const auto input = samples[index];
                const auto old = current_.render(input, ch);
                samples[index] = transition_frames_ ? old*(1-mix)+target_.render(input,ch)*mix : old;
            }
            if (transition_frames_ && --transition_frames_ == 0) current_ = target_;
        }
    }
};
} // namespace audio_engine
