// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.11-ensemble: the ENSEMBLE chorus (src/dsp/Ensemble.h, Spec S7.11).
#include "dsp/Ensemble.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using lmw::dsp::Ensemble;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

struct Out { std::vector<float> l, r; };
static Out run(Ensemble& e, int n, double hz = 440.0, float amp = 0.5f) {
    Out o{std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n))};
    for (int i = 0; i < n; ++i) {
        const float x = amp * static_cast<float>(std::sin(6.283185307179586 * hz * i / SR));
        e.process(x, x, o.l[static_cast<size_t>(i)], o.r[static_cast<size_t>(i)]);
    }
    return o;
}
static double rms(const std::vector<float>& v, size_t from) {
    double s = 0; for (size_t i = from; i < v.size(); ++i) s += double(v[i]) * v[i]; return std::sqrt(s / double(v.size() - from));
}

int main() {
    {   // Depth 0: three equal taps at 10 ms: left = right = the input 10 ms later, x 1.5 x 0.8 (+1.6 dB).
        Ensemble e; e.prepare(SR); e.setParameters(0.0f, 0.6, 0.4f);
        const auto o = run(e, 9600);
        double maxLR = 0, maxErr = 0;
        for (size_t i = 0; i < o.l.size(); ++i) maxLR = std::max(maxLR, std::fabs(double(o.l[i]) - o.r[i]));
        for (size_t i = 960; i < o.l.size(); ++i) {
            const double want = 1.5 * Ensemble::outputGain * 0.5 * std::sin(6.283185307179586 * 440.0 * (double(i) - 480.0) / SR);
            maxErr = std::max(maxErr, std::fabs(double(o.l[i]) - want));
        }
        std::printf("depth 0: |L-R| %.2g, error to the input 10 ms later %.2g\n", maxLR, maxErr);
        CHECK(maxLR < 1e-6 && maxErr < 1e-3);
    }
    {   // Full depth: stereo (left and right differ), level within +-1.5 dB of the input.
        Ensemble e; e.prepare(SR); e.setParameters(1.0f, 0.6, 0.5f);
        const auto o = run(e, 96000);
        double dot = 0, ll = 0, rr = 0;
        for (size_t i = 4800; i < o.l.size(); ++i) { dot += double(o.l[i]) * o.r[i]; ll += double(o.l[i]) * o.l[i]; rr += double(o.r[i]) * o.r[i]; }
        const double corr = dot / std::sqrt(ll * rr);
        const double inRms = 0.5 / std::sqrt(2.0);
        const double dbL = 20 * std::log10(rms(o.l, 4800) / inRms), dbR = 20 * std::log10(rms(o.r, 4800) / inRms);
        std::printf("full depth: L/R correlation %.3f, level L %+.2f dB R %+.2f dB\n", corr, dbL, dbR);
        CHECK(corr < 0.98 && std::fabs(dbL) < 1.5 && std::fabs(dbR) < 1.5);
    }
    {   // Rate: with Shimmer 0 the sweep repeats once per slow period (80000 samples at 0.6 Hz, 40000 at 1.2 Hz).
        auto repeatError = [](double rate, int period) {
            Ensemble e; e.prepare(SR); e.setParameters(1.0f, rate, 0.0f);
            const auto o = run(e, 2 * period + 4800, 480.0);       // 480 Hz: whole cycles in 20000, 40000, 80000
            double m = 0;
            for (int i = 4800; i < 4800 + period; ++i) m = std::max(m, std::fabs(double(o.l[size_t(i)]) - o.l[size_t(i + period)]));
            return m;
        };
        const double e06 = repeatError(0.6, 80000), e12 = repeatError(1.2, 40000), e12half = repeatError(1.2, 20000);
        std::printf("repeat error: 0.6 Hz over 80000 %.2g, 1.2 Hz over 40000 %.2g, over 20000 %.2g\n", e06, e12, e12half);
        CHECK(e06 < 1e-3 && e12 < 1e-3 && e12half > 1e-2);
    }
    {   // Silence after sound: exactly 0 once the line has emptied; a NaN input does not stick.
        Ensemble e; e.prepare(SR); e.setParameters(1.0f, 0.6, 1.0f);
        run(e, 4800);
        float l = 1, r = 1;
        for (int i = 0; i < 2000; ++i) e.process(0.0f, 0.0f, l, r);
        CHECK(l == 0.0f && r == 0.0f);
        e.process(std::numeric_limits<float>::quiet_NaN(), 0.0f, l, r);
        for (int i = 0; i < 2000; ++i) e.process(0.1f, 0.1f, l, r);
        CHECK(std::isfinite(l) && std::isfinite(r));
    }
    {   // CPU: one second of audio.
        Ensemble e; e.prepare(SR); e.setParameters(0.7f, 0.6, 0.5f);
        const auto t0 = std::chrono::steady_clock::now();
        run(e, 480000);
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("10 s of audio in %.3f s (%.2f %% of one core)\n", s, 100.0 * s / 10.0);
        CHECK(s < 2.0);
    }
    if (failures == 0) std::printf("ok\n");
    else std::printf("%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
