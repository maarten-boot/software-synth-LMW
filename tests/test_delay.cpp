// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.8-delay: the DELAY effect. An impulse comes back after Time, each repeat x Feedback; Spread 0
// keeps each side to itself, Spread 1 ping-pongs (left, right, left); Damping softens repeats; Time
// glides (no jump); a non-finite input does not poison the lines.
#include "dsp/Delay.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

int main() {
    // 10 ms = 480 samples, feedback 0.5, no spread: echoes at 480, 960 on the side they came in.
    {
        Delay d; d.prepare(SR); d.setParameters(0.01, 0.5f, 0.0f, 0.0f);
        std::vector<float> l(1500), r(1500);
        for (std::size_t i = 0; i < l.size(); ++i) d.process(i == 0 ? 1.0f : 0.0f, 0.0f, l[i], r[i]);
        CHECK(std::fabs(l[480] - 1.0f) < 1e-5f && std::fabs(l[960] - 0.5f) < 1e-5f && std::fabs(l[1440] - 0.25f) < 1e-5f);
        CHECK(std::fabs(l[0]) < 1e-9f && std::fabs(l[700]) < 1e-9f);   // wet only, nothing between echoes
        float rmax = 0; for (float v : r) rmax = std::fmax(rmax, std::fabs(v));
        CHECK(rmax < 1e-9f);
    }
    // Ping-pong (spread 1), mono input: first echo left, the next right, then left again.
    {
        Delay d; d.prepare(SR); d.setParameters(0.01, 0.6f, 0.0f, 1.0f);
        std::vector<float> l(1500), r(1500);
        for (std::size_t i = 0; i < l.size(); ++i) d.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, l[i], r[i]);
        std::printf("ping-pong: L %.3f %.3f %.3f, R %.3f %.3f %.3f\n", l[480], l[960], l[1440], r[480], r[960], r[1440]);
        CHECK(l[480] > 0.9f && std::fabs(r[480]) < 1e-6f);
        CHECK(std::fabs(l[960]) < 1e-6f && r[960] > 0.5f);
        CHECK(l[1440] > 0.3f && std::fabs(r[1440]) < 1e-6f);
    }
    // Damping: the second echo is smaller than without.
    {
        Delay a, b; a.prepare(SR); b.prepare(SR);
        a.setParameters(0.01, 0.8f, 0.0f, 0.0f); b.setParameters(0.01, 0.8f, 0.7f, 0.0f);
        float la = 0, lb = 0, rr = 0;
        for (int i = 0; i <= 960; ++i) { a.process(i == 0 ? 1.0f : 0.0f, 0, la, rr); b.process(i == 0 ? 1.0f : 0.0f, 0, lb, rr); }
        CHECK(lb < la * 0.7f && lb > 0.0f);
    }
    // Time glides: 100 -> 200 ms moves gradually, arrives after ~0.5 s.
    {
        Delay d; d.prepare(SR); d.setParameters(0.1, 0.3f, 0.0f, 0.0f);
        float l = 0, r = 0;
        d.process(0, 0, l, r);
        d.setParameters(0.2, 0.3f, 0.0f, 0.0f);
        d.process(0, 0, l, r);
        CHECK(d.delaySamples() < 4900.0);
        for (int i = 0; i < 24000; ++i) d.process(0, 0, l, r);
        CHECK(std::fabs(d.delaySamples() - 9600.0) < 0.5);
    }
    // Infinity in: everything stays finite.
    {
        Delay d; d.prepare(SR); d.setParameters(0.005, 0.9f, 0.2f, 0.5f);
        float l = 0, r = 0;
        d.process(INFINITY, 0, l, r);
        bool finite = true;
        for (int i = 0; i < 2000; ++i) { d.process(0.1f, 0.1f, l, r); finite = finite && std::isfinite(l) && std::isfinite(r); }
        CHECK(finite);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S7.8-delay: all passed\n", failures);
    return failures ? 1 : 0;
}
