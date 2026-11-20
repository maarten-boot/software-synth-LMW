// SPDX-License-Identifier: AGPL-3.0-or-later
// FORMANT module (Spec S5.16, Phase 16a): a vowel filter. Five band-pass filters in parallel, one per
// formant (resonance of the vocal tract), with the centre frequency, level and bandwidth of a sung
// vowel A, E, I, O, U for a voice type. The vowel position is continuous (0 = A .. 4 = U): between
// two vowels the formants glide (frequencies on a log scale, levels and bandwidths linearly).
// Formant data: the widely used table of sung-vowel formants (Csound manual, "Formant values") for
// bass, tenor, alto and soprano; baritone lies between bass and tenor; child is the soprano table
// raised by 15 % (an approximation). Each band is a state-variable filter (Simper's trapezoidal SVF)
// with its peak gain normalised to the formant's level. Coefficients are recomputed only when the
// target changes, at most every 16 samples. Pure C++, RT-safe.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace lmw::dsp {

enum class FormantVoice : int { Bass, Baritone, Tenor, Alto, Soprano, Child };   // order = choice order
inline constexpr int numFormantVoices = 6, numVowels = 5, numFormants = 5;
inline constexpr float maxVowelPosition = static_cast<float>(numVowels - 1);    // U

struct FormantSet {
    std::array<float, numFormants> freq{};     // Hz
    std::array<float, numFormants> gainDb{};   // level of each formant, dB (first = 0)
    std::array<float, numFormants> bw{};       // bandwidth, Hz
};

namespace detail {
// [voice][vowel A E I O U]: frequencies, levels (dB), bandwidths of formants 1-5.
inline const std::array<std::array<FormantSet, numVowels>, numFormantVoices>& formantTable() {
    static const auto table = [] {
        std::array<std::array<FormantSet, numVowels>, numFormantVoices> t{};
        const auto V = [](int v) { return static_cast<std::size_t>(v); };
        const std::array<FormantSet, numVowels> bass{{
            {{600, 1040, 2250, 2450, 2750}, {0, -7, -9, -9, -20},   {60, 70, 110, 120, 130}},
            {{400, 1620, 2400, 2800, 3100}, {0, -12, -9, -12, -18}, {40, 80, 100, 120, 120}},
            {{250, 1750, 2600, 3050, 3340}, {0, -30, -16, -22, -28},{60, 90, 100, 120, 120}},
            {{400, 750, 2400, 2600, 2900},  {0, -11, -21, -20, -40},{40, 80, 100, 120, 120}},
            {{350, 600, 2400, 2675, 2950},  {0, -20, -32, -28, -36},{40, 80, 100, 120, 120}}}};
        const std::array<FormantSet, numVowels> tenor{{
            {{650, 1080, 2650, 2900, 3250}, {0, -6, -7, -8, -22},   {80, 90, 120, 130, 140}},
            {{400, 1700, 2600, 3200, 3580}, {0, -14, -12, -14, -20},{70, 80, 100, 120, 120}},
            {{290, 1870, 2800, 3250, 3540}, {0, -15, -18, -20, -30},{40, 90, 100, 120, 120}},
            {{400, 800, 2600, 2800, 3000},  {0, -10, -12, -12, -26},{40, 80, 100, 120, 120}},
            {{350, 600, 2700, 2900, 3300},  {0, -20, -17, -14, -26},{40, 60, 100, 120, 120}}}};
        const std::array<FormantSet, numVowels> alto{{
            {{800, 1150, 2800, 3500, 4950}, {0, -4, -20, -36, -60}, {80, 90, 120, 130, 140}},
            {{400, 1600, 2700, 3300, 4950}, {0, -24, -30, -35, -60},{60, 80, 120, 150, 200}},
            {{350, 1700, 2700, 3700, 4950}, {0, -20, -30, -36, -60},{50, 100, 120, 150, 200}},
            {{450, 800, 2830, 3500, 4950},  {0, -9, -16, -28, -55}, {70, 80, 100, 130, 135}},
            {{325, 700, 2530, 3500, 4950},  {0, -12, -30, -40, -64},{50, 60, 170, 180, 200}}}};
        const std::array<FormantSet, numVowels> soprano{{
            {{800, 1150, 2900, 3900, 4950}, {0, -6, -32, -20, -50}, {80, 90, 120, 130, 140}},
            {{350, 2000, 2800, 3600, 4950}, {0, -20, -15, -40, -56},{60, 100, 120, 150, 200}},
            {{270, 2140, 2950, 3900, 4950}, {0, -12, -26, -26, -44},{60, 90, 100, 120, 120}},
            {{450, 800, 2830, 3800, 4950},  {0, -11, -22, -22, -50},{70, 80, 100, 130, 135}},
            {{325, 700, 2700, 3800, 4950},  {0, -16, -35, -40, -60},{50, 60, 170, 180, 200}}}};
        t[V(static_cast<int>(FormantVoice::Bass))] = bass;
        t[V(static_cast<int>(FormantVoice::Tenor))] = tenor;
        t[V(static_cast<int>(FormantVoice::Alto))] = alto;
        t[V(static_cast<int>(FormantVoice::Soprano))] = soprano;
        for (std::size_t v = 0; v < numVowels; ++v) {
            auto& bar = t[V(static_cast<int>(FormantVoice::Baritone))][v];
            auto& child = t[V(static_cast<int>(FormantVoice::Child))][v];
            for (std::size_t f = 0; f < numFormants; ++f) {
                bar.freq[f] = std::sqrt(bass[v].freq[f] * tenor[v].freq[f]);
                bar.gainDb[f] = 0.5f * (bass[v].gainDb[f] + tenor[v].gainDb[f]);
                bar.bw[f] = 0.5f * (bass[v].bw[f] + tenor[v].bw[f]);
                child.freq[f] = soprano[v].freq[f] * 1.15f;
                child.gainDb[f] = soprano[v].gainDb[f];
                child.bw[f] = soprano[v].bw[f] * 1.15f;
            }
        }
        return t;
    }();
    return table;
}
} // namespace detail

// The formants of `voice` at vowel position 0..4 (A, E, I, O, U; clamped), interpolated between vowels.
inline FormantSet formants(FormantVoice voice, float position) noexcept {
    const auto& row = detail::formantTable()[static_cast<std::size_t>(std::clamp(static_cast<int>(voice), 0, numFormantVoices - 1))];
    const float p = std::clamp(position, 0.0f, maxVowelPosition);
    const int lo = std::min(static_cast<int>(p), numVowels - 2);
    const float t = p - static_cast<float>(lo);
    const auto& a = row[static_cast<std::size_t>(lo)];
    const auto& b = row[static_cast<std::size_t>(lo + 1)];
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    FormantSet s;
    for (std::size_t f = 0; f < numFormants; ++f) {
        s.freq[f] = a.freq[f] * std::pow(b.freq[f] / a.freq[f], t);
        s.gainDb[f] = a.gainDb[f] + (b.gainDb[f] - a.gainDb[f]) * t;
        s.bw[f] = a.bw[f] + (b.bw[f] - a.bw[f]) * t;
    }
    return s;
}

class FormantFilter {
public:
    // Output level so a bright source (a saw) keeps about its loudness through the filter (T-S5.16).
    static constexpr float makeup = 4.0f;
    static constexpr int updateInterval = 16;   // samples between coefficient updates while modulated

    void setSampleRate(double sr) noexcept { sampleRate = sr; dirty = true; }
    void reset() noexcept { ic1.fill(0.0); ic2.fill(0.0); sinceUpdate = updateInterval; }

    // Voice type, vowel position 0..4 and resonance 0..1 (0.5 = the table's bandwidths, 1 = half of
    // them, 0 = double). Cheap when unchanged.
    void setTarget(FormantVoice v, float position, float resonance) noexcept {
        const auto differs = [](float a, float b) { return a < b || a > b; };   // exact check
        if (v != voice || differs(position, pos) || differs(resonance, res)) { voice = v; pos = position; res = resonance; dirty = true; }
    }

    float process(float x) noexcept {
        if (dirty && sinceUpdate >= updateInterval) update();
        ++sinceUpdate;
        double y = 0.0;
        for (std::size_t f = 0; f < numFormants; ++f) {
            const double v3 = static_cast<double>(x) - ic2[f];
            const double v1 = a1[f] * ic1[f] + a2[f] * v3;
            const double v2 = ic2[f] + a2[f] * ic1[f] + a3[f] * v3;
            ic1[f] = 2.0 * v1 - ic1[f];
            ic2[f] = 2.0 * v2 - ic2[f];
            y += gain[f] * v1;                  // gain includes k: band-pass with peak = formant level
        }
        return static_cast<float>(y) * makeup;
    }

private:
    void update() noexcept {
        const auto s = formants(voice, pos);
        const double bwScale = std::pow(2.0, 1.0 - 2.0 * static_cast<double>(std::clamp(res, 0.0f, 1.0f)));
        const double nyquistLimit = 0.45 * sampleRate;
        for (std::size_t f = 0; f < numFormants; ++f) {
            const double fc = std::clamp(static_cast<double>(s.freq[f]), 20.0, nyquistLimit);
            const double g = std::tan(std::numbers::pi * fc / sampleRate);
            const double k = std::max(0.01, static_cast<double>(s.bw[f]) * bwScale / fc);   // 1 / Q
            a1[f] = 1.0 / (1.0 + g * (g + k));
            a2[f] = g * a1[f];
            a3[f] = g * a2[f];
            gain[f] = k * std::pow(10.0, static_cast<double>(s.gainDb[f]) / 20.0);
        }
        dirty = false;
        sinceUpdate = 0;
    }

    double sampleRate = 48000.0;
    FormantVoice voice = FormantVoice::Tenor;
    float pos = 0.0f, res = 0.5f;
    bool dirty = true;
    int sinceUpdate = updateInterval;
    std::array<double, numFormants> a1{}, a2{}, a3{}, gain{}, ic1{}, ic2{};
};

} // namespace lmw::dsp
