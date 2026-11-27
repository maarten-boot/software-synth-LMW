// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.28-load (unit): the load meter (src/dsp/Load.h) — smoothing, the 5 s peak, overruns and the warning, the 9P
// /load text — and the offline benchmark (Spec S6.28 D a): the engine with 1 / 8 / 16 / 32 voices (3 VCOs each,
// no effects) as % of one core; it fails only on a gross regression: 8 voices above 50 % of a core.
#include "dsp/Engine.h"
#include "dsp/Load.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    {
        LoadMeter m;
        m.prepare(48000.0);
        const int n = 480;                                  // 10 ms blocks
        double t = 0.0;
        for (int i = 0; i < 300; ++i) { m.block(0.001, n, t); t += 0.01; }   // 3 s at 10 %
        std::printf("steady 10 %%: %.1f %%, peak %.1f %%\n", m.current() * 100, m.highest() * 100);
        CHECK(std::fabs(m.current() - 0.1) < 0.005 && std::fabs(m.highest() - 0.1) < 1e-9);
        CHECK(m.overruns() == 0 && !m.warning(t));
        m.block(0.012, n, t); t += 0.01;                    // one block too slow: an overrun, warning for 5 s
        CHECK(m.overruns() == 1 && m.warning(t) && m.highest() > 1.19);
        for (int i = 0; i < 600; ++i) { m.block(0.001, n, t); t += 0.01; }   // 6 s later: peak and warning gone
        std::printf("after the overrun + 6 s: %.1f %%, peak %.1f %%, overruns %u\n", m.current() * 100, m.highest() * 100, m.overruns());
        CHECK(!m.warning(t) && m.highest() < 0.11 && m.overruns() == 1);
        for (int i = 0; i < 300; ++i) { m.block(0.0075, n, t); t += 0.01; }   // 75 %: warning without an overrun
        CHECK(m.warning(t) && m.overruns() == 1);
    }
    {
        LoadInfo l;
        l.cpu = 0.072; l.peak = 0.12; l.voices = 3; l.overruns = 0; l.xruns = 2; l.memoryMB = 84.24;
        const auto text = loadText(l);
        std::printf("%s", text.c_str());
        CHECK(text == "cpu 7.2\npeak 12.0\nvoices 3\noverruns 0\nxruns 2\nmemory 84.2\n");
    }
    // Benchmark: n voices held (3 VCOs each, all mixed in), 2 s of audio at 48 kHz in 256-sample blocks.
    double eight = 0.0;
    for (const int voices : {1, 8, 16, 32}) {
        Engine e;
        e.prepare(48000.0);
        e.setVoiceCount(voices);
        ModParams mp; mp.mixVco = {1.0f, 1.0f, 1.0f}; mp.cutoff = 3000.0f; mp.resonance = 0.3f;
        e.setModParams(mp);
        for (int v = 0; v < voices; ++v) e.noteOn(36 + v, 0.8f);
        std::vector<float> l(256), r(256);
        const int blocks = 2 * 48000 / 256;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.render(l.data(), r.data(), 256);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double share = seconds / 2.0 * 100.0;
        std::printf("benchmark: %2d voices %5.1f %% of one core (active %d)\n", voices, share, e.activeVoices());
        if (voices == 8) eight = share;
    }
    CHECK(eight < 50.0);                                     // a gross regression only (about 7 % on the 2026-11-24 VM)
    if (failures == 0) std::printf("T-S6.28-load: all passed\n");
    return failures == 0 ? 0 : 1;
}
