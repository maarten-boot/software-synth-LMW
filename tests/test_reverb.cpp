// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.15-reverb (unit): the Hall reverb (src/dsp/Reverb.h). Impulse responses: finite, the decay time at 1 kHz
// follows Size (RT60 from the Schroeder curve, -5 to -25 dB), Damping makes 8 kHz die faster, Width 0 is mono,
// deterministic; the level with steady noise about the same at Size 0 / 50 / 100 %; the ring of the tail (highest
// peak over the median of the 3-6 kHz spectrum, 1-2 s after the impulse) low. Classic (Freeverb) is compared in the
// host test (it needs JUCE).
#include "dsp/Reverb.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

using lmw::dsp::HallReverb;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;
static constexpr double pi = 3.141592653589793;

struct Ir { std::vector<float> l, r; };
static Ir impulse(float size, float damping, float width, int n) {
    HallReverb h;
    h.setParameters(size, damping, width);
    h.prepare(SR);
    Ir o{std::vector<float>(static_cast<std::size_t>(n)), std::vector<float>(static_cast<std::size_t>(n))};
    for (int i = 0; i < n; ++i) {
        const float x = i == 0 ? 1.0f : 0.0f;
        h.process(x, x, o.l[static_cast<std::size_t>(i)], o.r[static_cast<std::size_t>(i)]);
    }
    return o;
}
// A band around f (two cascaded biquad bandpasses, Q 2).
static std::vector<double> band(const std::vector<float>& x, double f) {
    std::vector<double> y(x.begin(), x.end());
    for (int pass = 0; pass < 2; ++pass) {
        const double w = 2 * pi * f / SR, q = 2.0, alpha = std::sin(w) / (2 * q), a0 = 1 + alpha;
        const double b0 = alpha / a0, b2 = -alpha / a0, a1 = -2 * std::cos(w) / a0, a2 = (1 - alpha) / a0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (auto& v : y) { const double in = v, out = b0 * in + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = in; y2 = y1; y1 = out; v = out; }
    }
    return y;
}
// RT60 from the Schroeder backward integral, fitted between -5 and -25 dB (T20 x 3).
static double rt60(const std::vector<double>& y) {
    std::vector<double> e(y.size());
    double s = 0;
    for (std::size_t i = y.size(); i-- > 0;) { s += y[i] * y[i]; e[i] = s; }
    const double total = e[0];
    double t5 = -1, t25 = -1;
    for (std::size_t i = 0; i < e.size(); ++i) {
        const double db = 10 * std::log10(e[i] / total + 1e-30);
        if (t5 < 0 && db <= -5) t5 = double(i) / SR;
        if (t25 < 0 && db <= -25) { t25 = double(i) / SR; break; }
    }
    return t25 > 0 && t5 >= 0 ? 3.0 * (t25 - t5) : -1.0;
}
static void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w = std::polar(1.0, -2 * pi / double(len));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> wn = 1;
            for (std::size_t k = 0; k < len / 2; ++k) { const auto u = a[i + k], v = a[i + k + len / 2] * wn; a[i + k] = u + v; a[i + k + len / 2] = u - v; wn *= w; }
        }
    }
}
// Ring: the highest spectral peak over the median, 3-6 kHz, of the tail from `from` (32768 samples, Hann).
static double ring(const std::vector<float>& x, std::size_t from) {
    const std::size_t n = 32768;
    std::vector<std::complex<double>> a(n);
    for (std::size_t i = 0; i < n; ++i) a[i] = x[from + i] * (0.5 - 0.5 * std::cos(2 * pi * double(i) / double(n)));
    fft(a);
    std::vector<double> mag;
    const auto k0 = static_cast<std::size_t>(3000.0 * n / SR), k1 = static_cast<std::size_t>(6000.0 * n / SR);
    for (std::size_t k = k0; k < k1; ++k) mag.push_back(std::abs(a[k]));
    auto sorted = mag;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    return *std::max_element(mag.begin(), mag.end()) / sorted[sorted.size() / 2];
}

// Freeverb as JUCE builds it (juce::Reverb: 8 combs, 4 allpasses, tunings at 44.1 kHz scaled), left side, for the
// ring comparison: room size -> feedback 0.7 + 0.28 x size, damping x 0.4.
static std::vector<float> freeverbImpulse(float size, float damping, int n) {
    static const int combT[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617}, apT[4] = {556, 441, 341, 225};
    struct C { std::vector<float> b; std::size_t i = 0; float last = 0; };
    std::vector<C> combs(8);
    std::vector<std::pair<std::vector<float>, std::size_t>> aps(4);
    for (int k = 0; k < 8; ++k) combs[static_cast<std::size_t>(k)].b.assign(static_cast<std::size_t>(combT[k] * SR / 44100.0), 0.0f);
    for (int k = 0; k < 4; ++k) aps[static_cast<std::size_t>(k)].first.assign(static_cast<std::size_t>(apT[k] * SR / 44100.0), 0.0f);
    const float fb = 0.7f + 0.28f * size, damp = damping * 0.4f;
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int t = 0; t < n; ++t) {
        const float in = (t == 0 ? 1.0f : 0.0f) * 0.015f;
        float o = 0;
        for (auto& c : combs) {
            const float y = c.b[c.i];
            c.last = y * (1 - damp) + c.last * damp;
            c.b[c.i] = in + c.last * fb;
            c.i = c.i + 1 == c.b.size() ? 0 : c.i + 1;
            o += y;
        }
        for (auto& [b, i] : aps) {
            const float y = b[i];
            b[i] = o + y * 0.5f;
            i = i + 1 == b.size() ? 0 : i + 1;
            o = y - o;
        }
        out[static_cast<std::size_t>(t)] = o;
    }
    return out;
}

int main() {
    const int n = static_cast<int>(SR * 14);
    // Decay time follows Size; finite; Damping 0 -> 8 kHz decays as 1 kHz.
    for (const float size : {0.0f, 0.5f, 1.0f}) {
        const auto ir = impulse(size, 0.0f, 1.0f, size < 0.9f ? n / 2 : n);
        bool finite = true; float peak = 0;
        for (std::size_t i = 0; i < ir.l.size(); ++i) { finite = finite && std::isfinite(ir.l[i]) && std::isfinite(ir.r[i]); peak = std::max(peak, std::fabs(ir.l[i])); }
        const double t = rt60(band(ir.l, 1000.0)), want = HallReverb::rt60(size);
        std::printf("size %.1f: RT60 at 1 kHz %.2f s (mapping %.2f s), peak %.3f\n", double(size), t, want, double(peak));
        CHECK(finite && peak < 2.0f);
        CHECK(t > 0.75 * want && t < 1.25 * want);
    }
    // Damping 100 %: the highs die at least 3 x faster.
    {
        const auto ir = impulse(0.6f, 1.0f, 1.0f, n / 2);
        const double t1 = rt60(band(ir.l, 1000.0)), t8 = rt60(band(ir.l, 8000.0));
        std::printf("damping 100 %%: RT60 1 kHz %.2f s, 8 kHz %.2f s (ratio %.1f)\n", t1, t8, t1 / t8);
        CHECK(t8 > 0 && t1 / t8 >= 3.0);
        const auto ir0 = impulse(0.6f, 0.0f, 1.0f, n / 2);
        const double r0 = rt60(band(ir0.l, 1000.0)) / rt60(band(ir0.l, 8000.0));
        std::printf("damping 0: ratio %.2f\n", r0);
        CHECK(r0 < 2.0);                                       // about equal: the modulated read darkens a little
    }
    // Width 0: mono; Width 1: the sides differ; deterministic.
    {
        const auto a = impulse(0.5f, 0.5f, 0.0f, 48000), b = impulse(0.5f, 0.5f, 1.0f, 48000), c = impulse(0.5f, 0.5f, 1.0f, 48000);
        CHECK(a.l == a.r);
        CHECK(b.l != b.r);
        CHECK(b.l == c.l && b.r == c.r);
    }
    // Level: steady white noise in, wet RMS at Size 0 / 50 / 100 % within 3 dB of each other.
    {
        double lv[3]; int k = 0;
        for (const float size : {0.0f, 0.5f, 1.0f}) {
            HallReverb h; h.setParameters(size, 0.5f, 1.0f); h.prepare(SR);
            unsigned s = 12345; double e = 0, ein = 0; int cnt = 0;
            for (int i = 0; i < static_cast<int>(SR * 8); ++i) {
                s = s * 1664525u + 1013904223u;
                const float x = (static_cast<float>(s >> 8) / 16777216.0f - 0.5f) * 0.5f;
                float l, r; h.process(x, x, l, r);
                if (i > SR * 5) { e += double(l) * l; ein += double(x) * x; ++cnt; }
            }
            lv[k++] = 10 * std::log10(e / ein);
        }
        std::printf("level (wet / in): size 0 %.1f dB, 50 %% %.1f dB, 100 %% %.1f dB\n", lv[0], lv[1], lv[2]);
        CHECK(std::fabs(lv[0] - lv[1]) < 3.0 && std::fabs(lv[2] - lv[1]) < 3.0);
    }
    // Ring of the tail.
    {
        const auto ir = impulse(0.85f, 0.5f, 1.0f, static_cast<int>(SR * 3));
        const double r = ring(ir.l, static_cast<std::size_t>(SR * 1.0));
        const auto fv = freeverbImpulse(0.85f, 0.5f, static_cast<int>(SR * 3));
        const double rf = ring(fv, static_cast<std::size_t>(SR * 1.0));
        std::printf("ring (3-6 kHz peak / median, 1 s on, Size 85 %%, Damping 50 %%): Hall %.1f, Classic (Freeverb) %.1f\n", r, rf);
        CHECK(r < 0.5 * rf);
    }
    if (failures == 0) std::printf("T-S7.15-reverb: all passed\n");
    return failures == 0 ? 0 : 1;
}
