// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.6-comb: the COMB effect's filter. Impulse response: echoes every D = sampleRate / frequency samples,
// each scaled by the feedback (alternating sign when negative), all times sqrt(1 - g^2); damping softens
// the echoes; white noise keeps about its loudness at high feedback; a new frequency glides (no jump);
// a non-finite input does not poison the delay line.
#include "dsp/Comb.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

int main() {
    // 480 Hz at 48 kHz: D = 100 samples exactly.
    {
        Comb c; c.prepare(SR); c.setParameters(480.0, 0.5f, 0.0f);
        std::vector<float> y(400);
        for (std::size_t i = 0; i < y.size(); ++i) y[i] = c.process(i == 0 ? 1.0f : 0.0f);
        const float s = std::sqrt(1.0f - 0.25f);
        CHECK(std::fabs(y[0] - s) < 1e-6f && std::fabs(y[100] - 0.5f * s) < 1e-5f && std::fabs(y[200] - 0.25f * s) < 1e-5f);
        CHECK(std::fabs(y[50]) < 1e-9f && std::fabs(y[150]) < 1e-9f);
    }
    // Negative feedback: the echoes alternate in sign.
    {
        Comb c; c.prepare(SR); c.setParameters(480.0, -0.5f, 0.0f);
        std::vector<float> y(300);
        for (std::size_t i = 0; i < y.size(); ++i) y[i] = c.process(i == 0 ? 1.0f : 0.0f);
        CHECK(y[100] < 0.0f && y[200] > 0.0f);
    }
    // Damping: the first echo is smaller and spread out.
    {
        Comb a, b; a.prepare(SR); b.prepare(SR);
        a.setParameters(480.0, 0.9f, 0.0f); b.setParameters(480.0, 0.9f, 0.6f);
        float ya = 0, yb = 0;
        for (int i = 0; i <= 100; ++i) { ya = a.process(i == 0 ? 1.0f : 0.0f); yb = b.process(i == 0 ? 1.0f : 0.0f); }
        CHECK(yb < ya * 0.6f && yb > 0.0f);
    }
    // White noise at feedback 0.9: within 3 dB of its own loudness.
    {
        Comb c; c.prepare(SR); c.setParameters(220.0, 0.9f, 0.0f);
        std::mt19937 rng(1); std::uniform_real_distribution<float> u(-1.0f, 1.0f);
        double in = 0, out = 0;
        for (int i = 0; i < 96000; ++i) { const float x = u(rng); const float y = c.process(x); if (i > 9600) { in += x * x; out += y * y; } }
        const double db = 10.0 * std::log10(out / in);
        std::printf("noise through comb (fb 0.9): %.2f dB\n", db);
        CHECK(std::fabs(db) < 3.0);
    }
    // A new frequency glides: one sample later the delay has hardly moved, after 100 ms it has arrived.
    {
        Comb c; c.prepare(SR); c.setParameters(480.0, 0.5f, 0.0f);
        c.process(0.0f);
        c.setParameters(240.0, 0.5f, 0.0f);
        c.process(0.0f);
        CHECK(c.delaySamples() < 101.0);
        for (int i = 0; i < 4800; ++i) c.process(0.0f);
        CHECK(std::fabs(c.delaySamples() - 200.0) < 0.01);
    }
    // Infinity in: the delay line stays finite.
    {
        Comb c; c.prepare(SR); c.setParameters(1000.0, 0.9f, 0.2f);
        c.process(INFINITY);
        bool finite = true;
        for (int i = 0; i < 2000; ++i) finite = finite && std::isfinite(c.process(0.1f));
        CHECK(finite);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S7.6-comb: all passed\n", failures);
    return failures ? 1 : 0;
}
