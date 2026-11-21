// SPDX-License-Identifier: AGPL-3.0-or-later
// DELAY effect (Spec S7.8, Phase 17c): a stereo echo. Each channel: y = line delayed by Time; the line is
// written with the input plus Feedback x lowpass(echo) (Damping: each repeat darker). Spread crosses the
// feedback between the channels and moves the input to the left: 0 = each channel echoes itself, 1 =
// ping-pong (left, right, left, ...). Time glides over about 50 ms like a tape delay, so moving it while
// echoes ring bends their pitch. Mix is applied by the caller (0: the input passes exactly). Buffers are
// sized in prepare() (not RT); process() is RT-safe.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace lmw::dsp {

class Delay {
public:
    static constexpr double minTime = 0.001, maxTime = 2.0;   // seconds
    static constexpr float maxFeedback = 0.95f;

    void prepare(double sr) {
        sampleRate = sr;
        const auto n = static_cast<std::size_t>(sr * maxTime) + 4;
        left.assign(n, 0.0f);
        right.assign(n, 0.0f);
        glide = 1.0 - std::exp(-1.0 / (0.05 * sr));   // ~50 ms
        reset();
    }
    void reset() noexcept {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        pos = 0; lpL = lpR = 0.0f; delay = target; first = true;
    }
    // Time in seconds (clamped), feedback 0..0.95, damping 0..1, spread 0..1.
    void setParameters(double seconds, float feedback, float damping, float spreadAmount) noexcept {
        target = std::clamp(seconds, minTime, maxTime) * sampleRate;
        g = std::clamp(feedback, 0.0f, maxFeedback);
        d = std::clamp(damping, 0.0f, 0.99f);
        s = std::clamp(spreadAmount, 0.0f, 1.0f);
        if (first) { delay = target; first = false; }
    }
    // One stereo sample in, the echoes (wet only) out.
    void process(float inL, float inR, float& outL, float& outR) noexcept {
        if (left.empty()) { outL = inL; outR = inR; return; }
        delay += (target - delay) * glide;
        const float yL = read(left), yR = read(right);
        lpL = yL * (1.0f - d) + lpL * d;
        lpR = yR * (1.0f - d) + lpR * d;
        const float fbL = (1.0f - s) * lpL + s * lpR, fbR = (1.0f - s) * lpR + s * lpL;   // cross by Spread
        const float mid = 0.5f * (inL + inR);
        const float wL = (1.0f - s) * inL + s * mid + g * fbL;   // Spread moves the input to the left
        const float wR = (1.0f - s) * inR + g * fbR;
        left[pos] = std::isfinite(wL) ? wL : 0.0f;
        right[pos] = std::isfinite(wR) ? wR : 0.0f;
        pos = pos + 1 < left.size() ? pos + 1 : 0;
        outL = yL;
        outR = yR;
    }
    double delaySamples() const noexcept { return delay; }

private:
    float read(const std::vector<float>& b) const noexcept {
        const auto size = static_cast<double>(b.size());
        double rp = static_cast<double>(pos) - delay;
        if (rp < 0.0) rp += size;
        const auto i0 = static_cast<std::size_t>(rp);
        const std::size_t i1 = i0 + 1 < b.size() ? i0 + 1 : 0;
        const auto frac = static_cast<float>(rp - static_cast<double>(i0));
        return b[i0] + (b[i1] - b[i0]) * frac;
    }

    std::vector<float> left, right;
    std::size_t pos = 0;
    double sampleRate = 48000.0;
    double target = 16800.0, delay = 16800.0, glide = 0.0004;   // double: the glide's small steps do not round away
    float g = 0.0f, d = 0.0f, s = 0.0f, lpL = 0.0f, lpR = 0.0f;
    bool first = true;
};

} // namespace lmw::dsp
