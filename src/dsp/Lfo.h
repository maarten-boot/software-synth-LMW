// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared low-frequency oscillator (Spec S5.1, S5.6): output ±1. RT-safe.
// Spec S5.28 (choice E a): Shape 0..1 as on the VCOs, without band-limiting (not needed at LFO rates): sine phase-
// distorted (mean 0), triangle's rise 50 -> 2 % of the cycle, saw a second saw up to half a cycle behind, square
// high for 50 -> 5 % (still +-1, so it can gate). Shape 0: the original waves exactly.
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
    void setShape(double s) noexcept { shape = s < 0.0 ? 0.0 : (s > 1.0 ? 1.0 : s); }   // Spec S5.28
    void reset() noexcept { phase = 0.0; }

    float next() noexcept {
        const double t = phase;
        double y = 0.0;
        if (shape > 0.0) y = shaped(t);
        else switch (wave) {
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
    double shaped(double t) const noexcept {
        const double s = shape;
        switch (wave) {
            case LfoWave::Sine: {
                const double d = 0.5 - 0.45 * s;
                const double u = t < d ? 0.5 * t / d : 0.5 + 0.5 * (t - d) / (1.0 - d);
                const double dc = (2.0 / std::numbers::pi) * (2.0 * d - 1.0);
                return (std::sin(2.0 * std::numbers::pi * u) - dc) / (1.0 + std::fabs(dc));
            }
            case LfoWave::Triangle: {
                const double r = 0.5 - 0.48 * s;
                return t < r ? -1.0 + 2.0 * t / r : 1.0 - 2.0 * (t - r) / (1.0 - r);
            }
            case LfoWave::Saw: {
                const double o = 0.5 * s, t2 = t - o - std::floor(t - o);
                return ((2.0 * t - 1.0) + (2.0 * t2 - 1.0)) / (2.0 - 2.0 * o);
            }
            case LfoWave::Square: return t < 0.5 - 0.45 * s ? 1.0 : -1.0;
        }
        return 0.0;
    }
    double sampleRate = 48000.0, rate = 5.0, phase = 0.0;
    double shape = 0.0;                                 // Spec S5.28
    LfoWave wave = LfoWave::Sine;
};

} // namespace lmw::dsp
