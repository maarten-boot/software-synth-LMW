// SPDX-License-Identifier: AGPL-3.0-or-later
// Output soft limiter (Spec S4.6): transparent below the threshold (-6 dBFS), smooth tanh knee
// above it, and the output never exceeds the ceiling just below full scale. Stateless, RT-safe.
#pragma once
#include <atomic>
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

// Spec S7.4 (2026-11-26): the limiter after the effects (MAIN): transparent up to -0.9 dBFS, so the synth
// (already limited in OUT) passes unchanged and only the effects' build-up (a long reverb, delay feedback) is
// caught before it clips at the output.
struct MainLimiter {
    static constexpr float threshold = 0.9f;
    static constexpr float ceiling   = 0.995f;
    static float process(float x) noexcept {
        const float a = std::fabs(x);
        if (a <= threshold) return x;
        const float range = ceiling - threshold;
        return std::copysign(threshold + range * std::tanh((a - threshold) / range), x);
    }
};

// Spec S5.31: how much a limiter takes off (dB, 0 = nothing) of a signal whose highest |x| was `peak` (both
// limiters are monotonic, so the peak's reduction is the most taken off in that stretch).
template <class Limiter> inline float reductionDb(float peak) noexcept {
    if (!(peak > Limiter::threshold) || !std::isfinite(peak)) return 0.0f;
    return static_cast<float>(20.0 * std::log10(static_cast<double>(peak) / static_cast<double>(Limiter::process(peak))));
}

// Spec S5.31: a limiter lamp. The audio thread reports each block's reduction with its time (seconds); readers
// (GUI, 9P) ask whether the lamp is lit (working now or within the last 0.3 s) and the most taken off within the
// last second. Lock-free; a torn read between the fields only shifts the lamp by one block.
class LimitMeter {
public:
    void block(float db, double now) noexcept {
        if (db > 0.0f) activeAt.store(now, std::memory_order_relaxed);
        if (db >= held.load(std::memory_order_relaxed) || now - heldAt.load(std::memory_order_relaxed) > 1.0) {
            held.store(db, std::memory_order_relaxed);
            heldAt.store(now, std::memory_order_relaxed);
        }
    }
    bool lit(double now) const noexcept { return now - activeAt.load(std::memory_order_relaxed) < 0.3; }
    float worst(double now) const noexcept {
        return now - heldAt.load(std::memory_order_relaxed) <= 1.0 ? held.load(std::memory_order_relaxed) : 0.0f;
    }
private:
    std::atomic<float> held{0.0f};
    std::atomic<double> heldAt{-1e9}, activeAt{-1e9};
};

} // namespace lmw::dsp
