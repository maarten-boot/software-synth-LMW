// SPDX-License-Identifier: AGPL-3.0-or-later
// VCF: 24 dB/oct lowpass ladder (Spec S5.7). Zero-delay-feedback (TPT) solution of four
// one-pole stages with global feedback k = 4.4 * resonance; the feedback signal is tanh-limited
// so high resonance self-oscillates without blowing up. RT-safe.
#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lmw::dsp {

class Ladder {
public:
    static constexpr double minCutoff = 20.0, maxCutoff = 18000.0;

    void setSampleRate(double sr) noexcept { sampleRate = sr; lastCutoff = -1.0; }
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0; }
    // k = 4 is the self-oscillation threshold; 4.4 * r puts it at r ~ 0.91, so the top of the
    // resonance range really self-oscillates (amplitude then set by the tanh-limited feedback).
    void setResonance(float r) noexcept { k = 4.4 * std::clamp(static_cast<double>(r), 0.0, 1.0); }

    void setCutoff(double hz) noexcept {
        hz = std::clamp(hz, minCutoff, std::min(maxCutoff, 0.45 * sampleRate));
        if (!(hz < lastCutoff || hz > lastCutoff)) return;   // unchanged (exact cache check): skip tan()
        lastCutoff = hz;
        const double g = std::tan(std::numbers::pi * hz / sampleRate);
        G = g / (1.0 + g);
    }

    float process(float in) noexcept {
        // Contribution of the stage states to the 4th output: y4 = G^4 u + S.
        const double b = 1.0 - G;                  // = 1 / (1 + g)
        const double S = G * G * G * b * s[0] + G * G * b * s[1] + G * b * s[2] + b * s[3];
        const double G4 = G * G * G * G;
        const double x = static_cast<double>(in);
        // Linear ZDF estimate of the output, then feedback through tanh: linear at k = 0 (default
        // sound unchanged), bounded feedback at high k (stable self-oscillation).
        const double y4 = (G4 * x + S) / (1.0 + k * G4);
        double u = x - k * std::tanh(y4);
        for (double& st : s) {                     // TPT one-pole lowpass stages
            const double v = (u - st) * G;
            const double y = v + st;
            st = y + v;
            u = y;
        }
        return static_cast<float>(u);
    }

private:
    double sampleRate = 48000.0, G = 0.5, k = 0.0, lastCutoff = -1.0;
    double s[4]{};
};

} // namespace lmw::dsp
