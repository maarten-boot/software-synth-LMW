// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared low-frequency oscillator (Spec S5.1, S5.6): output ±1. RT-safe.
#pragma once
#include <cmath>
#include <numbers>

namespace lmw::dsp {

enum class LfoWave { Sine = 0, Triangle, Saw, Square };

class Lfo {
public:
    void setSampleRate(double sr) noexcept { sampleRate = sr; }
    void setRate(double hz) noexcept { rate = hz; }
    void setWave(LfoWave w) noexcept { wave = w; }
    void reset() noexcept { phase = 0.0; }

    float next() noexcept {
        const double t = phase;
        double y = 0.0;
        switch (wave) {
            case LfoWave::Sine:     y = std::sin(2.0 * std::numbers::pi * t); break;
            case LfoWave::Triangle: y = t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t; break;
            case LfoWave::Saw:      y = 2.0 * t - 1.0; break;
            case LfoWave::Square:   y = t < 0.5 ? 1.0 : -1.0; break;
        }
        phase += rate / sampleRate;
        if (phase >= 1.0) phase -= std::floor(phase);
        return static_cast<float>(y);
    }

private:
    double sampleRate = 48000.0, rate = 5.0, phase = 0.0;
    LfoWave wave = LfoWave::Sine;
};

} // namespace lmw::dsp
