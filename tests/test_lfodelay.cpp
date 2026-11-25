// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.24-lfodelay: LFO Delay (delayed vibrato). Per voice, after every note-on, the LFO has no effect for the
// first half of the Delay and fades in linearly to full at the Delay; Delay 0 is exactly the old sound; a
// later voice has its own delay; the depth holds in release; out.in (the LFO itself, once) is not delayed.
// Probe: vca.cv = lfo1.out (a square LFO at 0.05 Hz stays +1 for 10 s), vca.initial 0, so the voice's level
// is its share of LFO 1.
#include "dsp/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.4f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 48000.0;
static constexpr int ms = 48;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(const ModParams& m, float release = 0.01f) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves[0] = Waveform::Saw; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = release;
    e.setParams(p); e.setModParams(m);
    return e;
}
static ModParams probe(float delay) {
    ModParams m; m.lfoWave = LfoWave::Square; m.lfoRate = 0.05f; m.lfoDelay = {delay, 0.0f};
    return m;
}
// Peak of |x| over [from, to) ms.
static float peak(const std::vector<float>& x, int fromMs, int toMs) {
    float p = 0;
    for (int i = fromMs * ms; i < toMs * ms && i < static_cast<int>(x.size()); ++i) p = std::max(p, std::fabs(x[static_cast<size_t>(i)]));
    return p;
}

int main() {
    // Delay 0 (default): the vibrato sound is exactly as before; LFO 2's delay leaves LFO 1 alone.
    {
        ModParams a, b; a.fmSemitones[0] = 0.5f; b.fmSemitones[0] = 0.5f; b.lfoDelay = {0.0f, 2.0f};
        Engine e1 = makeEngine(a), e2 = makeEngine(b);
        e1.noteOn(57, 0.8f); e2.noteOn(57, 0.8f);
        CHECK(render(e1, 24000) == render(e2, 24000));
    }
    // Delay 1 s: nothing to 0.5 s, half at 0.75 s, full from 1 s.
    {
        Engine e = makeEngine(probe(1.0f));
        e.connect(In::VcaCv, Out::Lfo1);
        e.noteOn(57, 1.0f);
        const auto x = render(e, 1500 * ms);
        const float full = peak(x, 1100, 1400);
        std::printf("delay 1 s: peak 0-490 ms %.5f, 740-760 ms %.4f, full %.4f\n", peak(x, 0, 490), peak(x, 740, 760), full);
        CHECK(full > 0.1f);
        CHECK(peak(x, 0, 490) == 0.0f);
        CHECKV(std::fabs(peak(x, 740, 760) / full - 0.5f) < 0.04f, peak(x, 740, 760) / full);
        CHECKV(std::fabs(peak(x, 1000, 1100) / full - 1.0f) < 0.02f, peak(x, 1000, 1100) / full);
    }
    // A later voice has its own delay: until half its delay it adds exactly nothing to the first one.
    {
        Engine e = makeEngine(probe(1.0f)), ref = makeEngine(probe(1.0f));
        e.connect(In::VcaCv, Out::Lfo1); ref.connect(In::VcaCv, Out::Lfo1);
        e.noteOn(57, 1.0f); ref.noteOn(57, 1.0f);
        auto a = render(e, 1200 * ms), b = render(ref, 1200 * ms);
        CHECK(a == b);
        e.noteOn(64, 1.0f);                                 // second voice at 1.2 s
        a = render(e, 800 * ms); b = render(ref, 800 * ms);
        CHECK(std::equal(a.begin(), a.begin() + 490 * ms, b.begin()));          // 1.2 .. 1.69 s: the same
        float diff = 0; for (int i = 600 * ms; i < 800 * ms; ++i) diff = std::max(diff, std::fabs(a[static_cast<size_t>(i)] - b[static_cast<size_t>(i)]));
        CHECK(diff > 0.05f);                                // from 1.7 s the second voice is heard
    }
    // In release the depth stays where it was (key up at 0.75 s, a long release).
    {
        Engine e = makeEngine(probe(1.0f), 10.0f);
        e.connect(In::VcaCv, Out::Lfo1);
        e.noteOn(57, 1.0f);
        const auto x1 = render(e, 750 * ms);
        e.noteOff(57);
        const auto x2 = render(e, 1000 * ms);
        const float atOff = peak(x1, 730, 750), later = peak(x2, 800, 1000);
        std::printf("release: at key-up %.4f, 0.8-1 s later %.4f\n", atOff, later);
        CHECKV(later / atOff > 0.95f && later / atOff < 1.1f, later / atOff);   // stays about half (no rise to full)
    }
    // out.in = lfo1.out: the LFO itself (output once) is not delayed.
    {
        ModParams m; m.lfoRate = 10.0f; m.lfoWave = LfoWave::Square; m.lfoDelay = {2.0f, 2.0f};
        Engine e = makeEngine(m);
        e.connect(In::OutIn, Out::Lfo1);
        const auto x = render(e, 9000);
        int flips = 0; for (size_t i = 1; i < x.size(); ++i) flips += ((x[i - 1] > 0) != (x[i] > 0)) ? 1 : 0;
        CHECK(flips == 3);
        std::printf("out.in: peak in the first 40 ms %.4f\n", peak(x, 0, 40));
        CHECK(peak(x, 0, 40) > 0.1f);                       // heard from the start
    }
    if (failures == 0) std::printf("T-S5.24-lfodelay: all passed\n");
    return failures == 0 ? 0 : 1;
}
