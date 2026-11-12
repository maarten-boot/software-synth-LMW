// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.13: VCF key tracking (vcfN.key) and the resonance input (vcfN.res). Measured with the
// filter self-oscillating (resonance 1, a little noise to start it): it sings at its cutoff, so the
// pitch of out.in = vcf1.out shows the cutoff.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static double rms(const std::vector<float>& x) { double s = 0; for (float v : x) s += double(v) * v; return std::sqrt(s / double(x.size())); }
static double frequency(const std::vector<float>& x) {           // rising zero crossings per second
    int z = 0; for (size_t i = 1; i < x.size(); ++i) z += (x[i - 1] < 0 && x[i] >= 0) ? 1 : 0;
    return z * SR / double(x.size());
}
// VCF 1 alone on the output: no VCO, a trace of noise, cutoff 500 Hz.
static Engine makeEngine(float key, float res, bool resFromVelocity) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
    ModParams m; m.mixVco = {0.0f, 0.0f, 0.0f}; m.mixNoise = 0.001f; m.cutoff = 500.0f; m.resonance = res; m.keyTrack = key;
    e.setModParams(m);
    e.connect(In::OutIn, Out::Vcf1);
    if (resFromVelocity) e.connect(In::Vcf1Res, Out::KeysVel);
    return e;
}
static double sungFrequency(float key, int note) {
    Engine e = makeEngine(key, 1.0f, false);
    e.noteOn(note, 1.0f); render(e, 24000);                       // let it build up
    return frequency(render(e, 48000));
}

int main() {
    // Key 0: the cutoff is the same for every note.
    {
        const double c4 = sungFrequency(0.0f, 60), c5 = sungFrequency(0.0f, 72);
        std::printf("key 0 %%: C4 %.1f Hz, C5 %.1f Hz\n", c4, c5);
        CHECK(c4 > 300 && std::fabs(c5 / c4 - 1.0) < 0.02);
    }
    // Key 100 %: one octave up per octave, unchanged at C4.
    {
        const double ref = sungFrequency(0.0f, 60);
        const double c4 = sungFrequency(1.0f, 60), c5 = sungFrequency(1.0f, 72), c3 = sungFrequency(1.0f, 48);
        std::printf("key 100 %%: C3 %.1f Hz, C4 %.1f Hz, C5 %.1f Hz\n", c3, c4, c5);
        CHECK(std::fabs(c4 / ref - 1.0) < 0.01);
        CHECK(std::fabs(c5 / c4 - 2.0) < 0.1);
        CHECK(std::fabs(c4 / c3 - 2.0) < 0.1);
    }
    // Key 50 %: half an octave per octave.
    {
        const double c4 = sungFrequency(0.5f, 60), c5 = sungFrequency(0.5f, 72);
        std::printf("key 50 %%: C5/C4 = %.3f (sqrt 2 = 1.414)\n", c5 / c4);
        CHECK(std::fabs(c5 / c4 - std::sqrt(2.0)) < 0.07);
    }
    // Res input from keys.vel, Reso fader 0: a hard note self-oscillates, a soft one does not.
    {
        Engine hard = makeEngine(0.0f, 0.0f, true), soft = makeEngine(0.0f, 0.0f, true), none = makeEngine(0.0f, 0.0f, false);
        hard.noteOn(60, 1.0f); soft.noteOn(60, 0.2f); none.noteOn(60, 1.0f);
        render(hard, 24000); render(soft, 24000); render(none, 24000);
        const double h = rms(render(hard, 24000)), s = rms(render(soft, 24000)), n = rms(render(none, 24000));
        std::printf("res from velocity: vel 1.0 rms %.4f, vel 0.2 rms %.6f, unpatched rms %.6f\n", h, s, n);
        CHECK(h > 0.05);
        CHECK(s < h * 0.05);
        CHECK(n < h * 0.05);
    }
    // Unpatched vcfN.res has no source and reads 0; it can take a cable by name.
    CHECK(Patch{}.source(In::Vcf1Res) == noSource && Patch{}.source(In::Vcf2Res) == noSource);
    CHECK(sourceName(noSource) == "none");
    CHECK(sourceName(Out::KeysVel) == "keys.vel");
    if (failures == 0) std::printf("T-S5.13-vcfkey: all passed\n");
    return failures == 0 ? 0 : 1;
}
