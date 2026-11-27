// SPDX-License-Identifier: AGPL-3.0-or-later
// VCO core (Spec S5.1). Band-limited saw/square/pulse via polyBLEP. RT-safe.
// Spec S5.28 (Phase 49): Shape 0..1 morphs every wave — square: pulse width 50 -> 5 %; triangle: the rise 50 -> 2 %
// of the cycle (towards a saw); saw: a second saw slides in up to half a cycle behind (at 1 an octave up); sine:
// phase distortion (towards a bright saw-like wave); pulse: 12.5 -> 2 %. Jumps band-limited with polyBLEP, corners
// with polyBLAMP. Shape 0 takes the original code path: the waves stay bit-identical.
#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lmw::dsp {

// Order = parameter choice order; Pulse (Spec S5.18) last, so saved states keep their waves.
enum class Waveform { Sine = 0, Saw, Square, Triangle, Pulse };

// Spec S5.18: narrow pulse, high for 12.5 % of the cycle; DC removed and scaled to a peak of +1
// (-1/7 .. +1), like an AC-coupled analogue pulse: quieter than the square, rich in overtones.
inline constexpr double pulseWidth = 0.125;

class Oscillator {
public:
    void setSampleRate(double sr) noexcept { sampleRate = sr; updateIncrement(); }
    void setFrequency(double hz) noexcept { frequency = hz; updateIncrement(); }
    void setWaveform(Waveform w) noexcept { wave = w; }
    void setShape(double s) noexcept { shape = s < 0.0 ? 0.0 : (s > 1.0 ? 1.0 : s); }   // Spec S5.28
    void reset() noexcept { phase = 0.0; }

    // Spec S5.20: `phaseOffset` in cycles (phase modulation); 0 = the plain oscillator, exactly.
    float next(double phaseOffset = 0.0) noexcept {
        double t = phase;
        if (phaseOffset < 0.0 || phaseOffset > 0.0) { t += phaseOffset; t -= std::floor(t); }
        const double dt = increment;
        double y = 0.0;
        if (shape > 0.0) y = shaped(t, dt);
        else switch (wave) {
            case Waveform::Sine:     y = std::sin(2.0 * std::numbers::pi * t); break;
            case Waveform::Saw:      y = 2.0 * t - 1.0 - polyBlep(t, dt); break;
            case Waveform::Square:   y = (t < 0.5 ? 1.0 : -1.0) + polyBlep(t, dt)
                                         - polyBlep(std::fmod(t + 0.5, 1.0), dt); break;
            case Waveform::Triangle: y = 4.0 * std::fabs(t - 0.5) - 1.0; break;
            case Waveform::Pulse: {
                const double raw = (t < pulseWidth ? 1.0 : -1.0) + polyBlep(t, dt)
                                 - polyBlep(std::fmod(t + 1.0 - pulseWidth, 1.0), dt);
                y = (raw + (1.0 - 2.0 * pulseWidth)) / (2.0 - 2.0 * pulseWidth);   // mean 0, peak +1
                break;
            }
        }
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return static_cast<float>(y);
    }

private:
    // Spec S5.28: the waves with Shape above 0.
    double shaped(double t, double dt) const noexcept {
        const double s = shape;
        auto wrap = [](double x) { return x - std::floor(x); };
        switch (wave) {
            case Waveform::Square:
            case Waveform::Pulse: {                        // width 50 -> 5 % (square), 12.5 -> 2 % (pulse); mean 0, peak +1
                // at least two samples wide: a narrower pulse cannot be band-limited (high notes keep a wider one)
                const double w = std::min(0.5, std::max(wave == Waveform::Square ? 0.5 - 0.45 * s : pulseWidth - 0.105 * s, 2.0 * dt));
                const double raw = (t < w ? 1.0 : -1.0) + polyBlep(t, dt) - polyBlep(wrap(t + 1.0 - w), dt);
                return (raw + (1.0 - 2.0 * w)) / (2.0 - 2.0 * w);
            }
            case Waveform::Triangle: {                     // falls from +1 over f of the cycle, rises over 1 - f
                const double f = std::max(0.5, std::min(0.5 + 0.48 * s, 1.0 - 2.0 * dt));   // the rise at least two samples
                double y = t < f ? 1.0 - 2.0 * t / f : -1.0 + 2.0 * (t - f) / (1.0 - f);
                const double jump = 2.0 / f + 2.0 / (1.0 - f);   // the change of slope at each corner (per cycle)
                y += jump * dt * (polyBlamp(wrap(t - f), dt) - polyBlamp(t, dt));
                return y;
            }
            case Waveform::Saw: {                          // a second saw, up to half a cycle behind; level kept
                const double o = 0.5 * s, t2 = wrap(t - o);
                const double sum = (2.0 * t - 1.0 - polyBlep(t, dt)) + (2.0 * t2 - 1.0 - polyBlep(t2, dt));
                return sum / (2.0 - 2.0 * o);
            }
            case Waveform::Sine: {                         // phase distortion: the first half of the wave in d of the cycle
                const double d = std::max(0.5 - 0.45 * s, std::min(0.5, 6.0 * dt));   // limited: stays clean up high
                const double u = t < d ? 0.5 * t / d : 0.5 + 0.5 * (t - d) / (1.0 - d);
                const double dc = (2.0 / std::numbers::pi) * (2.0 * d - 1.0);   // the short positive half: mean below 0
                return (std::sin(2.0 * std::numbers::pi * u) - dc) / (1.0 + std::fabs(dc));   // mean 0, peak at most 1
            }
        }
        return 0.0;
    }
    // Integrated polyBLEP: rounds a corner (a jump in slope) over two samples. `jump x dt x polyBlamp` is added.
    static double polyBlamp(double t, double dt) noexcept {
        if (t < dt)       { const double x = t / dt - 1.0;       return -x * x * x / 3.0; }
        if (t > 1.0 - dt) { const double x = (t - 1.0) / dt + 1.0; return x * x * x / 3.0; }
        return 0.0;
    }
    static double polyBlep(double t, double dt) noexcept {
        if (t < dt)       { t /= dt;               return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
        return 0.0;
    }
    void updateIncrement() noexcept { increment = sampleRate > 0 ? frequency / sampleRate : 0.0; }

    double sampleRate = 48000.0, frequency = 440.0, increment = 0.0, phase = 0.0;
    Waveform wave = Waveform::Saw;
    double shape = 0.0;                                     // Spec S5.28
};

} // namespace lmw::dsp
