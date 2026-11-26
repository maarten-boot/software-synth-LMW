// SPDX-License-Identifier: AGPL-3.0-or-later
// ENSEMBLE effect (Spec S7.11, Phase 28a): the string-machine (Solina) ensemble chorus. Since S5.25 (stereo pan)
// the left and right inputs each have a delay line (before: summed to mono, as the string machines), so a
// panned voice keeps its side; with left = right the output is exactly as before. Each line is read at three taps; each
// tap's delay is swept by a slow LFO (Rate, about 0.6 Hz, +-3 ms at full Depth) and a fast one (10 x Rate,
// +-0.3 ms at full Depth x Shimmer), the three taps 120 degrees apart on both. Left = tap 1 + 1/2 tap 2,
// right = tap 3 + 1/2 tap 2, times 0.8 (between the level of three taps in step, +1.6 dB at Depth 0, and of
// taps out of step, about -0.5 dB at full Depth), so the result is wide and moving without an audible pitch wobble. Mix is applied by the caller (0: the input passes exactly). Fractional delays: 4-point
// Hermite interpolation; delays in double precision. prepare() allocates (not RT); process() is RT-safe.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace lmw::dsp {

class Ensemble {
public:
    static constexpr double centreSeconds = 0.010, slowSeconds = 0.003, fastSeconds = 0.0003;
    static constexpr double minRate = 0.15, maxRate = 2.4, fastFactor = 10.0;   // Hz (the slow LFO)
    static constexpr float outputGain = 0.8f;

    void prepare(double sr) {
        sampleRate = sr;
        const auto n = static_cast<std::size_t>(std::ceil((centreSeconds + slowSeconds + fastSeconds) * sr)) + 8;
        line.assign(n, 0.0f);
        lineR.assign(n, 0.0f);
        reset();
    }
    void reset() noexcept {
        std::fill(line.begin(), line.end(), 0.0f);
        std::fill(lineR.begin(), lineR.end(), 0.0f);
        pos = 0; slowPhase = fastPhase = 0.0;
    }
    // depth 0..1, rate (slow LFO) in Hz, shimmer 0..1.
    void setParameters(float depthAmount, double rateHz, float shimmerAmount) noexcept {
        depth = std::clamp(depthAmount, 0.0f, 1.0f);
        rate = std::clamp(rateHz, minRate, maxRate);
        shimmer = std::clamp(shimmerAmount, 0.0f, 1.0f);
    }
    // One stereo sample in, the ensemble (wet only) out.
    void process(float inL, float inR, float& outL, float& outR) noexcept {
        if (line.empty()) { outL = inL; outR = inR; return; }
        line[pos] = std::isfinite(inL) ? inL : 0.0f;
        lineR[pos] = std::isfinite(inR) ? inR : 0.0f;
        constexpr double twoPi = 6.283185307179586, third = twoPi / 3.0;
        std::array<float, 3> tap{}, tapR{};
        for (int k = 0; k < 3; ++k) {
            const double off = third * k;
            const double seconds = centreSeconds
                                 + static_cast<double>(depth) * (slowSeconds * std::sin(slowPhase + off)
                                                                 + static_cast<double>(shimmer) * fastSeconds * std::sin(fastPhase + off));
            tap[static_cast<std::size_t>(k)] = read(line, seconds * sampleRate);
            tapR[static_cast<std::size_t>(k)] = read(lineR, seconds * sampleRate);
        }
        pos = pos + 1 < line.size() ? pos + 1 : 0;
        slowPhase += twoPi * rate / sampleRate;
        fastPhase += twoPi * rate * fastFactor / sampleRate;
        if (slowPhase >= twoPi) slowPhase -= twoPi;
        if (fastPhase >= twoPi) fastPhase -= twoPi;
        outL = (tap[0] + 0.5f * tap[1]) * outputGain;       // left input: taps 1 and 2
        outR = (tapR[2] + 0.5f * tapR[1]) * outputGain;     // right input: taps 3 and 2
    }

private:
    // The line `delay` samples back from the sample just written (4-point Hermite).
    float read(const std::vector<float>& buf, double delay) const noexcept {
        const auto size = static_cast<double>(buf.size());
        double rp = static_cast<double>(pos) - delay;
        while (rp < 0.0) rp += size;
        const auto i1 = static_cast<std::size_t>(rp);
        const auto f = static_cast<float>(rp - static_cast<double>(i1));
        const auto n = buf.size();
        const auto at = [&buf, n](std::size_t i, int d) {       // d = -1 .. 2; i + n - 1 never goes below 0
            return buf[(i + n + static_cast<std::size_t>(d + 1) - 1) % n];
        };
        const float y0 = at(i1, -1), y1 = at(i1, 0), y2 = at(i1, 1), y3 = at(i1, 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    std::vector<float> line, lineR;            // left and right inputs (S5.25)
    std::size_t pos = 0;
    double sampleRate = 48000.0, rate = 0.6, slowPhase = 0.0, fastPhase = 0.0;
    float depth = 0.6f, shimmer = 0.4f;
};

} // namespace lmw::dsp
