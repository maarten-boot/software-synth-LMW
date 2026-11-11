// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.12: LFO 2 and tremolo. lfo2.out is normalled to vca.mod; vca.tremolo sets the depth.
// Depth 0 leaves the sound exactly as it was; depth 1 with a square LFO 2 swings the gain between
// full and silent; a cable on vca.mod replaces LFO 2; lfo2.out can be heard on out.in.
#include "dsp/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(const ModParams& m) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves[0] = Waveform::Saw; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p); e.setModParams(m);
    return e;
}
// Peak level per 10 ms window.
static std::vector<float> envelope(const std::vector<float>& x) {
    std::vector<float> w;
    for (size_t i = 0; i + 480 <= x.size(); i += 480) {
        float p = 0; for (size_t k = i; k < i + 480; ++k) p = std::max(p, std::fabs(x[k])); w.push_back(p);
    }
    return w;
}

int main() {
    // Depth 0 (default): identical to an engine where LFO 2 has a different rate/wave.
    {
        ModParams a, b; b.lfo2Rate = 13.0f; b.lfo2Wave = LfoWave::Square;
        Engine e1 = makeEngine(a), e2 = makeEngine(b);
        e1.noteOn(57, 0.8f); e2.noteOn(57, 0.8f);
        const auto x1 = render(e1, 24000), x2 = render(e2, 24000);
        CHECK(x1 == x2);
    }
    // Depth 1, square LFO 2 at 2 Hz: the level alternates between full and (near) silent.
    {
        ModParams m; m.tremolo = 1.0f; m.lfo2Rate = 2.0f; m.lfo2Wave = LfoWave::Square;
        Engine e = makeEngine(m);
        e.noteOn(57, 1.0f);
        const auto w = envelope(render(e, 48000));
        const float hi = *std::max_element(w.begin(), w.end()), lo = *std::min_element(w.begin() + 2, w.end());
        std::printf("tremolo depth 1: window peak max %.3f, min %.5f\n", hi, lo);
        CHECK(hi > 0.05f);
        CHECK(lo < hi * 0.02f);
    }
    // Depth 0.5: the low phase is half the high phase.
    {
        ModParams m; m.tremolo = 0.5f; m.lfo2Rate = 2.0f; m.lfo2Wave = LfoWave::Square;
        Engine e = makeEngine(m);
        e.noteOn(57, 1.0f);
        const auto w = envelope(render(e, 48000));
        // windows fully inside the first high (0..250 ms) and low (250..500 ms) half periods
        const float hi = *std::max_element(w.begin() + 5, w.begin() + 20), lo = *std::max_element(w.begin() + 30, w.begin() + 45);
        std::printf("tremolo depth 0.5: high %.4f, low %.4f, ratio %.3f\n", hi, lo, lo / hi);
        CHECK(std::fabs(lo / hi - 0.5f) < 0.05f);
    }
    // A cable on vca.mod replaces LFO 2: keys.gate (+1 while held) means no reduction at all.
    {
        ModParams m; m.tremolo = 1.0f; m.lfo2Rate = 2.0f; m.lfo2Wave = LfoWave::Square;
        Engine e = makeEngine(m), ref = makeEngine(ModParams{});
        e.connect(In::VcaMod, Out::KeysGate);
        e.noteOn(57, 1.0f); ref.noteOn(57, 1.0f);
        CHECK(render(e, 24000) == render(ref, 24000));
    }
    // lfo2.out patched to out.in: the LFO itself is heard (a global source, output once).
    {
        ModParams m; m.lfo2Rate = 10.0f; m.lfo2Wave = LfoWave::Square;
        Engine e = makeEngine(m);
        e.connect(In::OutIn, Out::Lfo2);
        const auto x = render(e, 9000);                      // 10 Hz square from +1: flips at 50, 100, 150 ms
        int flips = 0; for (size_t i = 1; i < x.size(); ++i) flips += ((x[i - 1] > 0) != (x[i] > 0)) ? 1 : 0;
        CHECK(flips == 3);
    }
    if (failures == 0) std::printf("T-S5.12-lfo2: all passed\n");
    return failures == 0 ? 0 : 1;
}
