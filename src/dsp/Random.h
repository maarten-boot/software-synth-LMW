// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared sample & hold random source (Spec S5.15): a new random value (uniform ±1) at Rate per
// second; Smooth 0 = steps, up to 1 = glides towards each new value (time constant Smooth / (3 x
// Rate), so it about arrives within one step). Own generator, independent of the noise. RT-safe.
#pragma once
#include <cmath>
#include <cstdint>

namespace lmw::dsp {

class Random {
public:
    void setSampleRate(double sr) noexcept { sampleRate = sr; update(); }
    void setRate(double hz) noexcept { rate = hz; update(); }
    void setSmooth(double s) noexcept { smooth = s; update(); }
    void reset() noexcept { state = 0x2545F491u; phase = 1.0; value = target = 0.0f; }

    float next() noexcept {
        if (phase >= 1.0) {                       // next step: a new random target
            phase -= std::floor(phase);
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            target = static_cast<float>(state >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
        phase += rate / sampleRate;
        value = coef >= 1.0f ? target : value + (target - value) * coef;
        return value;
    }

private:
    void update() noexcept {
        const double tau = smooth / (3.0 * rate);          // seconds
        coef = tau <= 0.0 ? 1.0f : static_cast<float>(1.0 - std::exp(-1.0 / (tau * sampleRate)));
    }
    double sampleRate = 48000.0, rate = 4.0, smooth = 0.0, phase = 1.0;
    float value = 0.0f, target = 0.0f, coef = 1.0f;
    std::uint32_t state = 0x2545F491u;
};

} // namespace lmw::dsp
