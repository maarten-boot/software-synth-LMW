// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.28 (Phase 47): the load meter. The audio thread reports each block (the time it took to compute and the
// samples it made); readers on other threads see the share of the block's own duration spent computing, smoothed
// (about 0.3 s), its peak over the last 5 s and the overruns (blocks that took longer than their own duration: a
// dropout is likely). Lock-free: relaxed atomics, one writer.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace lmw::dsp {

class LoadMeter {
public:
    static constexpr double smoothing = 0.3;            // s
    static constexpr double peakWindow = 5.0;           // s: the peak is the highest block of the last 2.5-5 s
    static constexpr double warnLoad = 0.70;            // the header turns red above this, or after an overrun
    static constexpr double warnAfterOverrun = 5.0;     // s

    void prepare(double rate) noexcept { sampleRate = rate > 0.0 ? rate : 48000.0; }

    // Audio thread: one block of `samples` took `seconds` to compute; `now` is a clock in seconds. `realtime` false
    // (an offline render): the load is shown, but a slow block is not counted as an overrun.
    void block(double seconds, int samples, double now, bool realtime = true) noexcept {
        if (samples <= 0) return;
        const double duration = samples / sampleRate;
        const double share = seconds / duration;
        const double a = 1.0 - std::exp(-duration / smoothing);
        smoothed += (share - smoothed) * a;
        if (now - bucketStart >= peakWindow / 2) { previousMax = currentMax; currentMax = 0.0; bucketStart = now; }
        currentMax = std::max(currentMax, share);
        if (share > 1.0 && realtime) { overrunCount.fetch_add(1, std::memory_order_relaxed); lastOverrun.store(now, std::memory_order_relaxed); }
        load.store(smoothed, std::memory_order_relaxed);
        peak.store(std::max(currentMax, previousMax), std::memory_order_relaxed);
    }

    double current() const noexcept { return load.load(std::memory_order_relaxed); }       // 0.07 = 7 %
    double highest() const noexcept { return peak.load(std::memory_order_relaxed); }
    std::uint32_t overruns() const noexcept { return overrunCount.load(std::memory_order_relaxed); }
    double secondsSinceOverrun(double now) const noexcept {
        const double t = lastOverrun.load(std::memory_order_relaxed);
        return t < 0.0 ? 1e9 : now - t;
    }
    bool warning(double now) const noexcept { return current() > warnLoad || secondsSinceOverrun(now) < warnAfterOverrun; }

private:
    double sampleRate = 48000.0;
    double smoothed = 0.0, currentMax = 0.0, previousMax = 0.0, bucketStart = -1e9;   // audio thread only
    std::atomic<double> load{0.0}, peak{0.0}, lastOverrun{-1.0};
    static_assert(std::atomic<double>::is_always_lock_free, "the load meter must not lock on the audio thread");
    std::atomic<std::uint32_t> overrunCount{0};
};

struct LoadInfo {
    double cpu = 0.0, peak = 0.0;                         // shares of one block's duration (0.07 = 7 %)
    int voices = 0;
    std::uint32_t overruns = 0, xruns = 0;
    double memoryMB = 0.0;                                // 0: not known
    double limitOut = 0.0, limitMain = 0.0;               // Spec S5.31: dB the OUT / MAIN limiter took off (last second)
};

// The 9P /load text (S6.28): "cpu 7.2", "peak 12.0", "voices 3", "overruns 0", "xruns 0", "memory 84.2";
// S5.31: "limit_out 2.4", "limit_main 0.0" (dB taken off in the last second).
inline std::string loadText(const LoadInfo& l) {
    char buf[260];
    std::snprintf(buf, sizeof buf, "cpu %.1f\npeak %.1f\nvoices %d\noverruns %u\nxruns %u\nmemory %.1f\nlimit_out %.1f\nlimit_main %.1f\n",
                  l.cpu * 100.0, l.peak * 100.0, l.voices, static_cast<unsigned>(l.overruns), static_cast<unsigned>(l.xruns), l.memoryMB,
                  l.limitOut, l.limitMain);
    return buf;
}

} // namespace lmw::dsp
