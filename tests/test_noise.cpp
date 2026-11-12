// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.15: noise colours and the sample & hold random source. Spectral tilt measured with the
// Goertzel algorithm, averaged over many segments: white is flat, pink about -3 dB per octave, brown
// about -6 dB per octave. Random: steps at Rate within ±1; Smooth glides.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static double goertzelPower(const std::vector<float>& x, double hz) {
    const double w = 2.0 * std::numbers::pi * hz / SR, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
// Average power at `hz` over 400 segments of 4800 samples (Hann window).
static double bandPower(NoiseColour colour, double hz) {
    Noise n; n.setColour(colour);
    std::vector<float> seg(4800);
    double sum = 0;
    for (int k = 0; k < 400; ++k) {
        for (size_t i = 0; i < seg.size(); ++i)
            seg[i] = n.next() * static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * double(i) / double(seg.size())));
        sum += goertzelPower(seg, hz);
    }
    return sum / 400.0;
}
static double rms(NoiseColour colour) {
    Noise n; n.setColour(colour);
    double s = 0; for (int i = 0; i < 480000; ++i) { const double v = n.next(); s += v * v; }
    return std::sqrt(s / 480000.0);
}

int main() {
    // Spectral slope between 200 Hz and 3200 Hz (4 octaves).
    {
        const double white = 10 * std::log10(bandPower(NoiseColour::White, 3200) / bandPower(NoiseColour::White, 200));
        const double pink  = 10 * std::log10(bandPower(NoiseColour::Pink, 3200) / bandPower(NoiseColour::Pink, 200));
        const double brown = 10 * std::log10(bandPower(NoiseColour::Brown, 3200) / bandPower(NoiseColour::Brown, 200));
        std::printf("200 Hz -> 3200 Hz (4 octaves): white %+.1f dB, pink %+.1f dB, brown %+.1f dB\n", white, pink, brown);
        CHECK(std::fabs(white) < 1.5);
        CHECK(std::fabs(pink + 12.0) < 2.0);
        CHECK(std::fabs(brown + 24.0) < 3.0);
        std::printf("rms: white %.3f, pink %.3f, brown %.3f\n", rms(NoiseColour::White), rms(NoiseColour::Pink), rms(NoiseColour::Brown));
        CHECK(rms(NoiseColour::Pink) > 0.1 && rms(NoiseColour::Brown) > 0.1);
    }
    // White is the plain generator: the same as before colours (a fresh Noise without setColour).
    {
        Noise a, b; b.setColour(NoiseColour::Pink); b.setColour(NoiseColour::White);
        bool same = true; for (int i = 0; i < 1000; ++i) same = same && a.next() == b.next();
        CHECK(same);
    }
    // Random, Smooth 0: a new value 10 times per second, held in between, within ±1, not all equal.
    {
        Random r; r.setSampleRate(SR); r.setRate(10.0); r.setSmooth(0.0); r.reset();
        int changes = 0; float prev = r.next(), lo = prev, hi = prev;
        for (int i = 1; i < 48000; ++i) {
            const float v = r.next();
            changes += v != prev ? 1 : 0; prev = v; lo = std::min(lo, v); hi = std::max(hi, v);
        }
        std::printf("random 10 Hz, smooth 0: %d changes in 1 s, range %.2f .. %.2f\n", changes, lo, hi);
        CHECK(changes >= 9 && changes <= 10);
        CHECK(lo >= -1.0f && hi <= 1.0f && hi - lo > 0.5f);
    }
    // Smooth 1: no jumps (largest step per sample small), still moves over the full range.
    {
        Random r; r.setSampleRate(SR); r.setRate(10.0); r.setSmooth(1.0); r.reset();
        float prev = r.next(), maxStep = 0, lo = prev, hi = prev;
        for (int i = 1; i < 96000; ++i) {
            const float v = r.next();
            maxStep = std::max(maxStep, std::fabs(v - prev)); prev = v; lo = std::min(lo, v); hi = std::max(hi, v);
        }
        std::printf("random 10 Hz, smooth 1: largest step %.5f, range %.2f .. %.2f\n", maxStep, lo, hi);
        CHECK(maxStep < 0.01f);
        CHECK(hi - lo > 0.5f);
    }
    // In the engine: noise.rand on out.in is heard once (global) and steps at its rate.
    {
        Engine e; e.prepare(SR); e.setGain(1.0f);
        ModParams m; m.randRate = 8.0f; e.setModParams(m);
        e.connect(In::OutIn, Out::Rand);
        std::vector<float> x(48000); e.render(x.data(), nullptr, 48000);
        int changes = 0; for (size_t i = 2000; i < x.size(); ++i) changes += x[i] != x[i - 1] ? 1 : 0;   // after the fade-in
        std::printf("engine noise.rand at 8 Hz: %d changes after the fade-in\n", changes);
        CHECK(changes >= 6 && changes <= 8);
    }
    if (failures == 0) std::printf("T-S5.15-noise: all passed\n");
    return failures == 0 ? 0 : 1;
}
