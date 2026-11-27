// SPDX-License-Identifier: AGPL-3.0-or-later
// VCF: ladder filter (Spec S5.7, S5.9). Zero-delay-feedback (TPT) solution of four one-pole stages
// with global feedback k = 4.4 * resonance; the feedback signal is tanh-limited so high resonance
// self-oscillates without blowing up. Modes (slope 6/12/24 dB, type LP/HP/BP/Notch) mix the input
// after feedback (y0) and the four stage outputs (y1..y4), as in the Oberheim Xpander, so cutoff and
// resonance behave alike in every mode. 36 dB (Spec S5.23) adds two one-pole stages at the same cutoff after
// the ladder, outside the feedback: lowpass y4 through two more lowpass stages (z1), highpass the 24 dB
// highpass through two highpass stages (z2). A mode change crossfades the mix over 20 ms. RT-safe.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace lmw::dsp {

enum class FilterSlope : int { Db36, Db24, Db12, Db6 };      // order = parameter choice order (S5.23: 36 first)
enum class FilterType : int { Lowpass, Highpass, Bandpass, Notch };

// Mix of y0..y4, z1, z2 for each mode, with H = one stage (1 / (1 + s/wc)):
//   LP: H^n; HP: (1-H)^n; BP: 2H(1-H) or 4H^2(1-H)^2 (gain 1 at the cutoff);
//   Notch: 1 - BP (exactly 0 at the cutoff). Bandpass and notch have no 6 dB form: 6 dB acts as 12; nor a
//   36 dB form: 36 dB acts as 24 (S5.23). LP 36 = z1 (H^6), HP 36 = z2 ((1-H)^6).
using FilterMix = std::array<double, 7>;
inline FilterMix filterMix(FilterSlope slope, FilterType type) noexcept {
    const bool bandOrNotch = type == FilterType::Bandpass || type == FilterType::Notch;
    if (bandOrNotch && slope == FilterSlope::Db6) slope = FilterSlope::Db12;
    if (bandOrNotch && slope == FilterSlope::Db36) slope = FilterSlope::Db24;
    const bool four = slope == FilterSlope::Db24, one = slope == FilterSlope::Db6, six = slope == FilterSlope::Db36;
    switch (type) {
        case FilterType::Lowpass:  return six ? FilterMix{0, 0, 0, 0, 0, 1, 0} : one ? FilterMix{0, 1, 0, 0, 0, 0, 0}
                                       : four ? FilterMix{0, 0, 0, 0, 1, 0, 0} : FilterMix{0, 0, 1, 0, 0, 0, 0};
        case FilterType::Highpass: return six ? FilterMix{0, 0, 0, 0, 0, 0, 1} : one ? FilterMix{1, -1, 0, 0, 0, 0, 0}
                                       : four ? FilterMix{1, -4, 6, -4, 1, 0, 0} : FilterMix{1, -2, 1, 0, 0, 0, 0};
        case FilterType::Bandpass: return four ? FilterMix{0, 0, 4, -8, 4, 0, 0} : FilterMix{0, 2, -2, 0, 0, 0, 0};
        case FilterType::Notch:    return four ? FilterMix{1, 0, -4, 8, -4, 0, 0} : FilterMix{1, -2, 2, 0, 0, 0, 0};
    }
    return {0, 0, 0, 0, 1, 0, 0};
}

class Ladder {
public:
    static constexpr double minCutoff = 20.0, maxCutoff = 18000.0;

    void setSampleRate(double sr) noexcept {
        sampleRate = sr; lastCutoff = -1.0;
        fadeStep = 1.0 / (0.02 * sr);                                   // 20 ms mode crossfade
    }
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0; t[0] = t[1] = t[2] = t[3] = 0.0; fade = 1.0; }

    // Spec S5.11: 24 dB lowpass (fully faded in) with no resonance — at maximum cutoff this is "open".
    bool isPlainLowpassNoResonance() const noexcept { return isPlainLowpass24 && fade >= 1.0 && !(k > 0.0); }

    // Spec S5.9. A change crossfades from the mix in use now (also mid-fade) to the new one.
    void setMode(FilterSlope slope, FilterType type) noexcept {
        const auto next = filterMix(slope, type);
        if (next == target) return;
        from = current();
        target = next;
        fade = 0.0;
        isPlainLowpass24 = next == filterMix(FilterSlope::Db24, FilterType::Lowpass);
        const bool was = extra;
        extra = next[5] != 0.0 || next[6] != 0.0 || from[5] != 0.0 || from[6] != 0.0;
        if (extra && !was) t[0] = t[1] = t[2] = t[3] = 0.0;   // the extra stages start from rest (under the fade)
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

    // Spec S6.27 choice C (user 2026-11-27): above the self-oscillation threshold (k > 4, Res above ~91 %) a filter at
    // rest gets a tiny start value, as an analog ladder starts from its own noise — so it rings without any input.
    // Below the threshold nothing changes (the default sound stays exactly as it was).
    static constexpr double selfStart = 1e-3;          // -60 dB: at Res 100 % and 1 kHz it reaches 0.05 after ~40 ms
    float process(float in) noexcept {
        if (k > 4.0 && s[0] == 0.0 && s[1] == 0.0 && s[2] == 0.0 && s[3] == 0.0) s[0] = selfStart;
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
        if (fade < 1.0) {
            fade = std::min(1.0, fade + fadeStep);
            if (fade >= 1.0) extra = target[5] != 0.0 || target[6] != 0.0;   // the old mix is gone
        }
        double z1 = 0.0, z2 = 0.0;
        if (extra) {                               // S5.23: two more stages, outside the feedback
            const auto onePole = [this](double& st, double x) { const double v = (x - st) * G; const double lp = v + st; st = lp + v; return lp; };
            z1 = onePole(t[1], onePole(t[0], y[4]));
            const double hp24 = y[0] - 4.0 * y[1] + 6.0 * y[2] - 4.0 * y[3] + y[4];
            const double h1 = hp24 - onePole(t[2], hp24);
            z2 = h1 - onePole(t[3], h1);
        }
        const auto m = current();
        return static_cast<float>(m[0] * y[0] + m[1] * y[1] + m[2] * y[2] + m[3] * y[3] + m[4] * y[4] + m[5] * z1 + m[6] * z2);
    }

private:
    FilterMix current() const noexcept {
        if (fade >= 1.0) return target;
        FilterMix m{};
        for (std::size_t i = 0; i < m.size(); ++i) m[i] = from[i] + (target[i] - from[i]) * fade;
        return m;
    }

    double sampleRate = 48000.0, G = 0.5, k = 0.0, lastCutoff = -1.0;
    double s[4]{};
    double t[4]{};                            // S5.23 extra stages: two lowpass (z1), two highpass (z2)
    FilterMix target{0, 0, 0, 0, 1, 0, 0}, from{0, 0, 0, 0, 1, 0, 0};
    bool extra = false;                       // target or from uses z1 / z2: run the extra stages
    double fade = 1.0, fadeStep = 1.0 / 960.0;
    bool isPlainLowpass24 = true;             // target is 24 dB lowpass (once faded in: original path)
};

} // namespace lmw::dsp
