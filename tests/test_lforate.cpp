// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.19: LFO rate inputs. Unpatched (and after unplugging) the LFOs run exactly at the fader's rate;
// +1.0 doubles the rate, 0.5 raises it half an octave; per-voice sources come from the newest note,
// shared ones (LFO 2) directly; the modulated rate stays within 0.01..50 Hz; the new inputs have no
// normalled source.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(float lfo1Rate) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p);
    ModParams m; m.lfoRate = lfo1Rate; m.lfoWave = LfoWave::Sine;
    e.setModParams(m);
    e.connect(In::OutIn, Out::Lfo1);              // hear LFO 1 alone
    return e;
}
static int risingCrossings(const std::vector<float>& x) {
    int n = 0; for (std::size_t i = 1; i < x.size(); ++i) n += (x[i - 1] < 0.0f && x[i] >= 0.0f) ? 1 : 0; return n;
}

int main() {
    CHECK(normalled[static_cast<std::size_t>(In::Lfo1Rate)] == noSource && normalled[static_cast<std::size_t>(In::Lfo2Rate)] == noSource);
    CHECK(inputNames[static_cast<std::size_t>(In::Lfo1Rate)] == "lfo1.rate" && numInputs == 29);   // 29 since S5.20
    // Unpatched = the fader's rate (5 Hz: 10 cycles in 2 s); plugging and unplugging leaves it exactly so.
    {
        Engine a = makeEngine(5.0f), b = makeEngine(5.0f);
        b.connect(In::Lfo1Rate, Out::KeysVel);
        b.disconnect(In::Lfo1Rate);
        a.noteOn(60, 1.0f); b.noteOn(60, 1.0f);
        const auto x = render(a, 96000);
        CHECK(x == render(b, 96000));
        const int c = risingCrossings(x);
        CHECK(c >= 9 && c <= 11);
    }
    // keys.vel of the newest note (velocity 1 -> +1.0): double rate; at 50 %: half an octave up.
    {
        Engine e = makeEngine(5.0f);
        e.connect(In::Lfo1Rate, Out::KeysVel);
        e.noteOn(60, 1.0f);
        const int c = risingCrossings(render(e, 96000));
        std::printf("lfo1 5 Hz + 1.0: %d cycles in 2 s\n", c);
        CHECK(c >= 19 && c <= 21);
        Engine h = makeEngine(5.0f);
        h.addPin(In::Lfo1Rate, Out::KeysVel, 0.5f);
        h.noteOn(60, 1.0f);
        const int ch = risingCrossings(render(h, 96000));
        CHECK(ch >= 13 && ch <= 15);                     // 5 x 1.414 = 7.07 Hz: 14 cycles
    }
    // Newest note decides: an older loud note and a newer quiet one (velocity 0.25: +0.25 octave).
    {
        Engine e = makeEngine(8.0f);
        e.connect(In::Lfo1Rate, Out::KeysVel);
        e.noteOn(60, 1.0f);
        e.noteOn(64, 0.25f);
        const int c = risingCrossings(render(e, 96000));
        std::printf("lfo1 8 Hz, newest note velocity 0.25: %d cycles in 2 s\n", c);
        CHECK(c >= 18 && c <= 20);                       // 8 x 2^0.25 = 9.51 Hz: 19 cycles
    }
    // A shared source (LFO 2) modulates the rate even with no note; the result differs from the plain LFO.
    {
        Engine a = makeEngine(5.0f), b = makeEngine(5.0f);
        b.connect(In::Lfo1Rate, Out::Lfo2);
        CHECK(render(a, 96000) != render(b, 96000));
    }
    // Clamped at 50 Hz: base 20 Hz + 4 octaves (four pins of keys.vel) would be 320 Hz.
    {
        Engine e = makeEngine(20.0f);
        for (Out o : {Out::KeysVel, Out::KeysGate, Out::Env1, Out::Env2}) e.addPin(In::Lfo1Rate, o, 1.0f);
        e.noteOn(60, 1.0f);
        render(e, 4800);                                 // ENV 1/2 reach their sustain
        const int c = risingCrossings(render(e, 48000));
        std::printf("lfo1 clamped: %d cycles in 1 s\n", c);
        CHECK(c >= 49 && c <= 51);
    }
    // No note ever played: per-voice sources read 0, the rate stays the fader's.
    {
        Engine a = makeEngine(5.0f), b = makeEngine(5.0f);
        b.connect(In::Lfo2Rate, Out::KeysVel);
        CHECK(render(a, 48000) == render(b, 48000));
    }
    std::printf(failures ? "%d FAILED\n" : "T-S5.19-lforate: all passed\n", failures);
    return failures ? 1 : 0;
}
