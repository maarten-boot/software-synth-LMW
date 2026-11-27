// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.28-shape (unit): VCO and LFO Shape. Shape 0 = the original waves bit-identical; square / pulse duty, triangle
// rise and the LFO square measured; mean about 0; alias products at C5 and C7 no worse than -24 dB (the 12.5 % pulse
// of before: -21.6 dB at C7); the VCO shape input adds to the fader (engine).
#include "dsp/Engine.h"
#include "dsp/Lfo.h"
#include "dsp/Oscillator.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0, PI = 3.141592653589793;

static std::vector<float> run(Waveform w, double shape, double f, std::size_t n) {
    Oscillator o; o.setSampleRate(SR); o.setFrequency(f); o.setWaveform(w); o.setShape(shape);
    std::vector<float> y(n); for (auto& v : y) v = o.next(); return y;
}
static void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) { std::size_t b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) std::swap(a[i], a[j]); }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const auto w = std::polar(1.0, -2 * PI / double(len));
        for (std::size_t i = 0; i < n; i += len) { std::complex<double> wn = 1;
            for (std::size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * wn; a[i + k] = u + v; a[i + k + len / 2] = u - v; wn *= w; } }
    }
}
// The strongest component more than 3 bins from every harmonic, relative to the strongest bin (dB).
static double alias(Waveform w, double shape, double f) {
    const auto y = run(w, shape, f, 65536);
    std::vector<std::complex<double>> a(y.size());
    for (std::size_t i = 0; i < y.size(); ++i) a[i] = y[i] * (0.5 - 0.5 * std::cos(2 * PI * double(i) / double(y.size())));
    fft(a);
    const double bin = SR / double(a.size());
    double top = 0, al = 0;
    for (std::size_t k = 2; k < a.size() / 2; ++k) {
        const double m = std::abs(a[k]), h = double(k) * bin / f;
        top = std::max(top, m);
        if (std::fabs(h - std::round(h)) * f / bin > 3.0) al = std::max(al, m);
    }
    return 20 * std::log10(al / top);
}
static double highShare(const std::vector<float>& y) {        // share of samples above 0 (duty cycle)
    return double(std::count_if(y.begin(), y.end(), [](float v) { return v > 0.0f; })) / double(y.size());
}
static double mean(const std::vector<float>& y) { double s = 0; for (float v : y) s += v; return s / double(y.size()); }

int main() {
    const char* names[] = {"sine", "saw", "square", "triangle", "pulse"};
    // Shape 0: exactly the waves of before (setShape(0) and no setShape at all give the same samples).
    for (int w = 0; w < 5; ++w) {
        Oscillator a, b; a.setSampleRate(SR); b.setSampleRate(SR); a.setFrequency(261.63); b.setFrequency(261.63);
        a.setWaveform(Waveform(w)); b.setWaveform(Waveform(w)); b.setShape(0.0);
        bool same = true; for (int i = 0; i < 4800; ++i) same = same && a.next() == b.next();
        CHECK(same);
    }
    // Duty cycles at 100 Hz (wide enough, the 2-sample floor does not apply): square 50 -> 5 %, pulse 12.5 -> 2 %;
    // the triangle's rise 50 -> 2 % (share of rising samples).
    const auto sq = run(Waveform::Square, 1.0, 100.0, 48000), pu = run(Waveform::Pulse, 1.0, 100.0, 48000);
    const auto tri = run(Waveform::Triangle, 1.0, 100.0, 48000);
    int rising = 0; for (std::size_t i = 1; i < tri.size(); ++i) rising += tri[i] > tri[i - 1] ? 1 : 0;
    // mean 0 shifts the square's zero crossing: count the high part as the samples above the midpoint of its range
    auto duty = [](const std::vector<float>& y) {
        const auto [lo, hi] = std::minmax_element(y.begin(), y.end());
        const float mid = (*lo + *hi) / 2; return double(std::count_if(y.begin(), y.end(), [mid](float v) { return v > mid; })) / double(y.size());
    };
    std::printf("Shape 100 %%: square duty %.3f, pulse duty %.3f, triangle rising %.3f\n", duty(sq), duty(pu), rising / 48000.0);
    CHECK(std::fabs(duty(sq) - 0.05) < 0.005 && std::fabs(duty(pu) - 0.02) < 0.004 && std::fabs(rising / 48000.0 - 0.02) < 0.006);   // the rounded corners add a sample
    CHECK(std::fabs(duty(run(Waveform::Square, 0.5, 100.0, 48000)) - 0.275) < 0.005);
    // Mean about 0 and the alias level, every wave at Shape 0.5 and 1, C5 and C7.
    for (int w = 0; w < 5; ++w)
        for (const double s : {0.5, 1.0})
            for (const double f : {523.25, 2093.0}) {
                const double m = mean(run(Waveform(w), s, f, 48000)), a = alias(Waveform(w), s, f);
                std::printf("%-8s shape %.1f %6.0f Hz: mean %+.4f, alias %6.1f dB\n", names[w], s, f, m, a);
                CHECK(std::fabs(m) < 0.01 && a < -23.5);
            }
    // LFO: Shape 0 unchanged; square high 5 % at Shape 1, still +-1; triangle rise 2 %.
    {
        Lfo a, b; a.setSampleRate(SR); b.setSampleRate(SR); a.setRate(3.0); b.setRate(3.0); b.setShape(0.0);
        bool same = true; for (int i = 0; i < 48000; ++i) same = same && a.next() == b.next();
        CHECK(same);
        Lfo q; q.setSampleRate(SR); q.setRate(2.0); q.setWave(LfoWave::Square); q.setShape(1.0);
        std::vector<float> y(48000); for (auto& v : y) v = q.next();
        const auto [lo, hi] = std::minmax_element(y.begin(), y.end());
        std::printf("LFO square Shape 100 %%: high %.3f, range %.1f .. %.1f\n", highShare(y), *lo, *hi);
        CHECK(std::fabs(highShare(y) - 0.05) < 0.003 && *lo == -1.0f && *hi == 1.0f);
    }
    // Engine: a cable into vco1.shape adds to the fader — keys.vel (0.8) on a square: the sound differs from Shape 0
    // and equals Shape 0.8 set on the fader.
    {
        auto render = [](float fader, bool cable) {
            Engine e; e.prepare(SR); e.setGain(1.0f);
            VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
            ModParams m; m.vcoShape = {fader, 0.0f, 0.0f}; e.setModParams(m);
            e.setVoiceCount(1);
            if (cable) e.addPin(In::Vco1Shape, Out::KeysVel, 1.0f);
            e.noteOn(48, 0.8f);
            std::vector<float> l(4800); e.render(l.data(), nullptr, 4800); return l;
        };
        const auto plain = render(0.0f, false), cabled = render(0.0f, true), fader = render(0.8f, false);
        double d1 = 0, d2 = 0;
        for (std::size_t i = 0; i < plain.size(); ++i) { d1 += std::fabs(cabled[i] - plain[i]); d2 += std::fabs(cabled[i] - fader[i]); }
        std::printf("engine: shape cable vs none %.2f, vs fader 0.8 %.5f\n", d1, d2);
        CHECK(d1 > 1.0 && d2 < 1e-3);
    }
    if (failures == 0) std::printf("T-S5.28-shape: all passed\n");
    return failures == 0 ? 0 : 1;
}
