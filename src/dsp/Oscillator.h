// SPDX-License-Identifier: AGPL-3.0-or-later
// VCO core (Spec S5.1). Band-limited saw/square via polyBLEP. RT-safe.
#pragma once
#include <cmath>
#include <numbers>

namespace lmw::dsp {

enum class Waveform { Sine = 0, Saw, Square, Triangle };

class Oscillator {
public:
    void setSampleRate(double sr) noexcept { sampleRate = sr; updateIncrement(); }
    void setFrequency(double hz) noexcept { frequency = hz; updateIncrement(); }
    void setWaveform(Waveform w) noexcept { wave = w; }
    void reset() noexcept { phase = 0.0; }

    float next() noexcept {
        const double t = phase, dt = increment;
        double y = 0.0;
        switch (wave) {
            case Waveform::Sine:     y = std::sin(2.0 * std::numbers::pi * t); break;
            case Waveform::Saw:      y = 2.0 * t - 1.0 - polyBlep(t, dt); break;
            case Waveform::Square:   y = (t < 0.5 ? 1.0 : -1.0) + polyBlep(t, dt)
                                         - polyBlep(std::fmod(t + 0.5, 1.0), dt); break;
            case Waveform::Triangle: y = 4.0 * std::fabs(t - 0.5) - 1.0; break;
        }
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return static_cast<float>(y);
    }

private:
    static double polyBlep(double t, double dt) noexcept {
        if (t < dt)       { t /= dt;               return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
        return 0.0;
    }
    void updateIncrement() noexcept { increment = sampleRate > 0 ? frequency / sampleRate : 0.0; }

    double sampleRate = 48000.0, frequency = 440.0, increment = 0.0, phase = 0.0;
    Waveform wave = Waveform::Saw;
};

} // namespace lmw::dsp
