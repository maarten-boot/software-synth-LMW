// SPDX-License-Identifier: AGPL-3.0-or-later
// VCF: ladder filter (Spec S5.7, S5.9). Zero-delay-feedback (TPT) solution of four one-pole stages
// with global feedback k = 4.4 * resonance; the feedback signal is tanh-limited so high resonance
// self-oscillates without blowing up. Modes (slope 6/12/24 dB, type LP/HP/BP/Notch) mix the input
// after feedback (y0) and the four stage outputs (y1..y4), as in the Oberheim Xpander, so cutoff and
// resonance behave alike in every mode. A mode change crossfades the mix over 20 ms. RT-safe.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace lmw::dsp {

enum class FilterSlope : int { Db24, Db12, Db6 };            // order = parameter choice order
enum class FilterType : int { Lowpass, Highpass, Bandpass, Notch };

// Mix of y0..y4 for each mode, with H = one stage (1 / (1 + s/wc)):
//   LP: H^n; HP: (1-H)^n; BP: 2H(1-H) or 4H^2(1-H)^2 (gain 1 at the cutoff);
//   Notch: 1 - BP (exactly 0 at the cutoff). Bandpass and notch have no 6 dB form: 6 dB acts as 12.
inline std::array<double, 5> filterMix(FilterSlope slope, FilterType type) noexcept {
    if ((type == FilterType::Bandpass || type == FilterType::Notch) && slope == FilterSlope::Db6) slope = FilterSlope::Db12;
    const bool four = slope == FilterSlope::Db24, one = slope == FilterSlope::Db6;
    switch (type) {
        case FilterType::Lowpass:  return one ? std::array<double, 5>{0, 1, 0, 0, 0}
                                       : four ? std::array<double, 5>{0, 0, 0, 0, 1} : std::array<double, 5>{0, 0, 1, 0, 0};
        case FilterType::Highpass: return one ? std::array<double, 5>{1, -1, 0, 0, 0}
                                       : four ? std::array<double, 5>{1, -4, 6, -4, 1} : std::array<double, 5>{1, -2, 1, 0, 0};
        case FilterType::Bandpass: return four ? std::array<double, 5>{0, 0, 4, -8, 4} : std::array<double, 5>{0, 2, -2, 0, 0};
        case FilterType::Notch:    return four ? std::array<double, 5>{1, 0, -4, 8, -4} : std::array<double, 5>{1, -2, 2, 0, 0};
    }
    return {0, 0, 0, 0, 1};
}

class Ladder {
public:
    static constexpr double minCutoff = 20.0, maxCutoff = 18000.0;

    void setSampleRate(double sr) noexcept {
        sampleRate = sr; lastCutoff = -1.0;
        fadeStep = 1.0 / (0.02 * sr);                                   // 20 ms mode crossfade
    }
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0; fade = 1.0; }

    // Spec S5.9. A change crossfades from the mix in use now (also mid-fade) to the new one.
    void setMode(FilterSlope slope, FilterType type) noexcept {
        const auto next = filterMix(slope, type);
        if (next == target) return;
        from = current();
        target = next;
        fade = 0.0;
        isPlainLowpass24 = next == filterMix(FilterSlope::Db24, FilterType::Lowpass);
    }
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
        double y[5];
        y[0] = u;
        for (int i = 0; i < 4; ++i) {              // TPT one-pole lowpass stages
            double& st = s[i];
            const double v = (u - st) * G;
            const double out = v + st;
            st = out + v;
            u = out;
            y[i + 1] = out;
        }
        if (fade >= 1.0 && isPlainLowpass24) return static_cast<float>(u);   // 24 dB LP: the original path
        if (fade < 1.0) fade = std::min(1.0, fade + fadeStep);
        const auto m = current();
        return static_cast<float>(m[0] * y[0] + m[1] * y[1] + m[2] * y[2] + m[3] * y[3] + m[4] * y[4]);
    }

private:
    std::array<double, 5> current() const noexcept {
        if (fade >= 1.0) return target;
        std::array<double, 5> m{};
        for (std::size_t i = 0; i < 5; ++i) m[i] = from[i] + (target[i] - from[i]) * fade;
        return m;
    }

    double sampleRate = 48000.0, G = 0.5, k = 0.0, lastCutoff = -1.0;
    double s[4]{};
    std::array<double, 5> target{0, 0, 0, 0, 1}, from{0, 0, 0, 0, 1};
    double fade = 1.0, fadeStep = 1.0 / 960.0;
    bool isPlainLowpass24 = true;             // target is 24 dB lowpass (once faded in: original path)
};

} // namespace lmw::dsp
