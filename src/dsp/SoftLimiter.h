// SPDX-License-Identifier: AGPL-3.0-or-later
// Output soft limiter (Spec S4.6): transparent below the threshold (-6 dBFS), smooth tanh knee
// above it, and the output never exceeds the ceiling just below full scale. Stateless, RT-safe.
#pragma once
#include <cmath>

namespace lmw::dsp {

struct SoftLimiter {
    static constexpr float threshold = 0.5f;     // -6 dBFS
    static constexpr float ceiling   = 0.995f;   // -0.04 dBFS; float tanh rounds to 1.0, so keep a margin

    static float process(float x) noexcept {
        const float a = std::fabs(x);
        if (a <= threshold) return x;
        const float range = ceiling - threshold;
        const float y = threshold + range * std::tanh((a - threshold) / range);   // slope 1 at the knee
        return std::copysign(y, x);
    }
};

} // namespace lmw::dsp
