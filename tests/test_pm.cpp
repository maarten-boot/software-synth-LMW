// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.20: phase modulation of VCO 1-3. The PM input has no normalled source; with the PM fader at 0, or
// nothing patched, the VCO is bit-identical to before; Oscillator::next(offset) shifts the phase by
// offset cycles; a sine modulated by a sine of the same pitch gains harmonics as the index grows (Bessel
// sidebands: the 2nd harmonic about half the fundamental's power at index 1, far above it at index 2); self-feedback stays finite.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(float index) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves = {Waveform::Sine, Waveform::Sine, Waveform::Sine};
    p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p);
    ModParams m; m.pmIndex[0] = index;              // VCO 1 heard (mixer default), VCO 2 only as modulator
    e.setModParams(m);
    return e;
}
static double power(const std::vector<float>& x, double hz, std::size_t from) {   // Goertzel
    const double w = 2.0 * std::numbers::pi * hz / SR, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (std::size_t i = from; i < x.size(); ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

int main() {
    CHECK(normalled[static_cast<std::size_t>(In::Vco2Pm)] == noSource && inputNames[static_cast<std::size_t>(In::Vco3Pm)] == "vco3.pm");
    CHECK(vcoPmIn(1) == In::Vco2Pm && vcoPitchIn(2) == In::Vco3Pitch && vcoFmIn(2) == In::Vco3Fm);
    // Oscillator: an offset of a quarter cycle on a sine at phase 0 gives 1; offset 0 is the plain path.
    {
        Oscillator a; a.setSampleRate(SR); a.setFrequency(100.0); a.setWaveform(Waveform::Sine);
        CHECK(std::fabs(a.next(0.25) - 1.0f) < 1e-6f);
        Oscillator b, c; for (auto* o : {&b, &c}) { o->setSampleRate(SR); o->setFrequency(123.0); o->setWaveform(Waveform::Saw); }
        bool same = true; for (int i = 0; i < 4800; ++i) same = same && b.next() == c.next(0.0);
        CHECK(same);
    }
    // Bit-identical while off: fader 0 with a cable, or fader up with nothing patched.
    {
        Engine plain = makeEngine(0.0f), cabled = makeEngine(0.0f), raised = makeEngine(3.0f);
        cabled.connect(In::Vco1Pm, Out::Vco2);
        for (Engine* e : {&plain, &cabled, &raised}) e->noteOn(57, 0.8f);
        const auto x = render(plain, 24000);
        CHECK(x == render(cabled, 24000));
        CHECK(x == render(raised, 24000));
    }
    // Sine by sine, same pitch (220 Hz): the 2nd harmonic grows with the index.
    {
        double ratio[3]{};
        const float idx[3] = {0.0f, 1.0f, 2.0f};
        for (int k = 0; k < 3; ++k) {
            Engine e = makeEngine(idx[k]);
            e.connect(In::Vco1Pm, Out::Vco2);
            e.noteOn(57, 0.8f);
            const auto x = render(e, 48000);
            ratio[k] = power(x, 440.0, 4800) / power(x, 220.0, 4800);
        }
        std::printf("2nd / 1st harmonic power: index 0 %.2g, index 1 %.3f, index 2 %.3f\n", ratio[0], ratio[1], ratio[2]);
        CHECK(ratio[0] < 1e-4 && ratio[1] > 0.05 && ratio[2] > ratio[1] && ratio[2] > 0.5);
    }
    // Self-feedback (vco1.out -> vco1.pm): a different, finite, bounded sound.
    {
        Engine a = makeEngine(1.5f), b = makeEngine(1.5f);
        b.connect(In::Vco1Pm, Out::Vco1);
        a.noteOn(45, 0.8f); b.noteOn(45, 0.8f);
        const auto xa = render(a, 24000), xb = render(b, 24000);
        CHECK(xa != xb);
        float peak = 0; bool finite = true;
        for (float v : xb) { finite = finite && std::isfinite(v); peak = std::fmax(peak, std::fabs(v)); }
        CHECK(finite && peak > 0.01f && peak < 1.0f);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S5.20-pm: all passed\n", failures);
    return failures ? 1 : 0;
}
