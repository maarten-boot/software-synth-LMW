// SPDX-License-Identifier: AGPL-3.0-or-later
// COMB effect (Spec S7.6, Phase 16f): a feedback comb filter per channel. y[n] = x[n] + g * lp(y[n - D]),
// D = sampleRate / frequency (fractional, linear interpolation), lp = one-pole lowpass in the loop
// (damping). Positive feedback: peaks at every multiple of the frequency (hollow, tube-like);
// negative: peaks at the odd multiples of half of it (clarinet-like). The output is scaled by
// sqrt(1 - g^2), so broadband material keeps about its loudness at any feedback. The delay glides
// to a new frequency over about 10 ms, so key tracking does not click. Mix 0 is applied by the caller
// (the input passes exactly). Buffers are sized in prepare() (not RT); process() is RT-safe.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace lmw::dsp {

class Comb {
public:
    static constexpr double minFrequency = 20.0, maxFrequency = 5000.0;
    static constexpr float maxFeedback = 0.95f;

    void prepare(double sr) {
        sampleRate = sr;
        buffer.assign(static_cast<std::size_t>(sr / minFrequency) + 4, 0.0f);
        glide = static_cast<float>(1.0 - std::exp(-1.0 / (0.01 * sr)));   // ~10 ms
        reset();
    }
    void reset() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        pos = 0; lp = 0.0f; delay = target; first = true;
    }
    // Frequency in Hz (clamped), feedback -0.95..0.95, damping 0..1 (0 = none).
    void setParameters(double hz, float feedback, float damping) noexcept {
        const double f = std::clamp(hz, minFrequency, maxFrequency);
        target = static_cast<float>(sampleRate / f);
        g = std::clamp(feedback, -maxFeedback, maxFeedback);
        d = std::clamp(damping, 0.0f, 0.99f);
        scale = std::sqrt(1.0f - g * g);
        if (first) { delay = target; first = false; }
    }
    float process(float x) noexcept {
        if (buffer.empty()) return x;
        delay += (target - delay) * glide;
        const auto size = static_cast<float>(buffer.size());
        float readPos = static_cast<float>(pos) - delay;
        if (readPos < 0.0f) readPos += size;
        const auto i0 = static_cast<std::size_t>(readPos);
        const std::size_t i1 = i0 + 1 < buffer.size() ? i0 + 1 : 0;
        const float frac = readPos - static_cast<float>(i0);
        const float delayed = buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
        lp = delayed * (1.0f - d) + lp * d;                       // damping in the loop
        const float y = x + g * lp;
        buffer[pos] = std::isfinite(y) ? y : 0.0f;
        pos = pos + 1 < buffer.size() ? pos + 1 : 0;
        return y * scale;
    }
    float delaySamples() const noexcept { return delay; }

private:
    std::vector<float> buffer;
    std::size_t pos = 0;
    double sampleRate = 48000.0;
    float target = 109.0f, delay = 109.0f, glide = 0.002f;
    float g = 0.0f, d = 0.0f, lp = 0.0f, scale = 1.0f;
    bool first = true;
};

} // namespace lmw::dsp
