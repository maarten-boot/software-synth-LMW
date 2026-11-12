// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.14: the pin matrix. An input sums level x output over its pins (up to 4); the normalled source
// is used while it has none; one keys.pitch / keys.gate pin at 100 % behaves exactly like the normalled
// one; global sources on out.in are output once; pin text with levels round-trips.
#include "dsp/Engine.h"
#include "dsp/PatchText.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(std::array<Waveform, 3> waves = {Waveform::Sine, Waveform::Saw, Waveform::Square}) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves = waves; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
    e.setModParams(ModParams{});
    return e;
}
static double maxDiff(const std::vector<float>& a, const std::vector<float>& b, float scaleB = 1.0f) {
    double m = 0; for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(double(a[i]) - double(b[i]) * scaleB)); return m;
}
static double frequency(const std::vector<float>& x) {
    int z = 0; for (size_t i = 1; i < x.size(); ++i) z += (x[i - 1] < 0 && x[i] >= 0) ? 1 : 0;
    return z * SR / double(x.size());
}

int main() {
    // Patch API: add, set level, remove one pin, at most 4 pins, same pins in any order.
    {
        Patch p;
        CHECK(p.connect(In::Vcf1Cutoff, Out::Env2, 0.5f) && p.connect(In::Vcf1Cutoff, Out::Lfo1));
        CHECK(p.pinCount(In::Vcf1Cutoff) == 2 && p.level(In::Vcf1Cutoff, Out::Env2) == 0.5f);
        CHECK(p.connect(In::Vcf1Cutoff, Out::Env2, 0.25f) && p.pinCount(In::Vcf1Cutoff) == 2 && p.level(In::Vcf1Cutoff, Out::Env2) == 0.25f);
        CHECK(p.connect(In::Vcf1Cutoff, Out::Lfo2) && p.connect(In::Vcf1Cutoff, Out::Noise));
        CHECK(!p.connect(In::Vcf1Cutoff, Out::KeysVel));                 // a fifth pin does not fit
        p.disconnect(In::Vcf1Cutoff, Out::Lfo1);
        CHECK(p.pinCount(In::Vcf1Cutoff) == 3 && p.level(In::Vcf1Cutoff, Out::Lfo1) == 0.0f);
        Patch q; q.connect(In::Vcf1Cutoff, Out::Noise); q.connect(In::Vcf1Cutoff, Out::Env2, 0.25f); q.connect(In::Vcf1Cutoff, Out::Lfo2);
        CHECK(p.samePins(q, In::Vcf1Cutoff));
        p.disconnect(In::Vcf1Cutoff);
        CHECK(!p.isPatched(In::Vcf1Cutoff) && p.source(In::Vcf1Cutoff) == Out::Env2);   // normalled again
    }
    // Level: out.in = vco1.out @ 0.5 is exactly half of out.in = vco1.out (linear range).
    {
        Engine full = makeEngine(), half = makeEngine();
        full.connect(In::OutIn, Out::Vco1);
        half.addPin(In::OutIn, Out::Vco1, 0.5f);
        full.noteOn(60, 0.5f); half.noteOn(60, 0.5f);
        const auto a = render(full, 4800), b = render(half, 4800);
        CHECK(maxDiff(b, a, 0.5f) < 1e-7);
    }
    // Sum: out.in = vco1.out + vco2.out equals the two rendered apart and added (VCO 2 is computed
    // because a pin uses it, although its mixer level is 0).
    {
        Engine both = makeEngine(), one = makeEngine(), two = makeEngine();
        both.addPin(In::OutIn, Out::Vco1, 1.0f); both.addPin(In::OutIn, Out::Vco2, 1.0f);
        one.connect(In::OutIn, Out::Vco1); two.connect(In::OutIn, Out::Vco2);
        for (Engine* e : {&both, &one, &two}) e->noteOn(57, 0.3f);
        const auto s = render(both, 4800), x = render(one, 4800), y = render(two, 4800);
        double m = 0; for (size_t i = 0; i < s.size(); ++i) m = std::max(m, std::fabs(double(s[i]) - x[i] - y[i]));
        CHECK(m < 1e-6);
        double r = 0; for (float v : y) r += double(v) * v;
        CHECK(r > 1.0);                                                   // VCO 2 really sounds
    }
    // Global sources on out.in are output once, whatever the number of voices; with a voice source too.
    {
        Engine lfoOnly = makeEngine(), lfoPlusVoices = makeEngine();
        lfoOnly.addPin(In::OutIn, Out::Lfo2, 0.5f);
        lfoPlusVoices.addPin(In::OutIn, Out::Lfo2, 0.5f); lfoPlusVoices.addPin(In::OutIn, Out::KeysGate, 0.1f);
        for (int n : {48, 52, 55, 60, 64}) lfoPlusVoices.noteOn(n, 1.0f);
        const auto a = render(lfoOnly, 4800), b = render(lfoPlusVoices, 4800);
        // b = a + 5 voices x keys.gate (1) x 0.1, after gain x headroom 0.25 (linear below 0.5)
        double m = 0; for (size_t i = 1200; i < a.size(); ++i) m = std::max(m, std::fabs(double(b[i]) - a[i] - 5 * 0.1 * 0.25));   // after the 20 ms fade-in
        CHECK(m < 1e-5);
        double peak = 0; for (float v : a) peak = std::max(peak, std::fabs(double(v)));
        CHECK(std::fabs(peak - 0.5 * 0.25) < 1e-3);                       // LFO 2 x 0.5, once
    }
    // keys.pitch as one pin at 100 % = the normalled connection (bit-identical); at 50 % = half the
    // pitch distance from C4: C5 sounds 12 x 0.5 = 6 semitones above C4.
    {
        Engine normal = makeEngine(), pinned = makeEngine(), halfPitch = makeEngine();
        pinned.connect(In::Vco1Pitch, Out::KeysPitch);
        halfPitch.addPin(In::Vco1Pitch, Out::KeysPitch, 0.5f);
        for (Engine* e : {&normal, &pinned, &halfPitch}) e->noteOn(72, 0.8f);
        const auto a = render(normal, 48000), b = render(pinned, 48000), c = render(halfPitch, 48000);
        CHECK(a == b);
        const double fa = frequency(a), fc = frequency(c);
        std::printf("keys.pitch pin 100 %%: %.1f Hz, 50 %%: %.1f Hz (C4 x sqrt 2 = %.1f)\n", fa, fc, 261.63 * std::sqrt(2.0));
        CHECK(std::fabs(fc / (261.63 * std::sqrt(2.0)) - 1.0) < 0.01);
    }
    // keys.gate as one pin at 100 %: envelopes follow the keys exactly as normalled.
    {
        Engine normal = makeEngine(), pinned = makeEngine();
        pinned.connect(In::Env1Gate, Out::KeysGate);
        for (Engine* e : {&normal, &pinned}) { e->noteOn(60, 0.8f); }
        auto a = render(normal, 4800), b = render(pinned, 4800);
        normal.noteOn(60, 0.9f); pinned.noteOn(60, 0.9f);           // retrigger
        normal.noteOff(60); pinned.noteOff(60);
        const auto a2 = render(normal, 9600), b2 = render(pinned, 9600);
        CHECK(a == b && a2 == b2);
    }
    // Text: pins with levels round-trip; levels as fraction or percent; errors refused.
    {
        const auto p = patchFromText("vcf1.cutoff=env2.out@0.5+lfo1.out;out.in = vcf2.out @ 25% + noise.out@.125");
        CHECK(p.has_value());
        if (p) {
            CHECK(p->pinCount(In::Vcf1Cutoff) == 2 && p->level(In::Vcf1Cutoff, Out::Env2) == 0.5f && p->level(In::Vcf1Cutoff, Out::Lfo1) == 1.0f);
            CHECK(p->level(In::OutIn, Out::Vcf2) == 0.25f && p->level(In::OutIn, Out::Noise) == 0.125f);
            const auto text = patchToText(*p);
            std::printf("pin text: %s\n", text.c_str());
            CHECK(text == "vcf1.cutoff=env2.out@0.5+lfo1.out;out.in=vcf2.out@0.25+noise.out@0.125");
            const auto q = patchFromText(text);
            CHECK(q.has_value() && q->samePins(*p, In::Vcf1Cutoff) && q->samePins(*p, In::OutIn));
        }
        CHECK(patchFromText("vco1.fm=lfo1.out").has_value());                       // older one-cable text
        CHECK(!patchFromText("vco1.fm=lfo1.out+lfo1.out").has_value());             // output twice
        CHECK(!patchFromText("vco1.fm=lfo1.out+lfo2.out+noise.out+env1.out+env2.out").has_value());   // 5 pins
        CHECK(!patchFromText("vco1.fm=lfo1.out@0").has_value());                    // level 0
        CHECK(!patchFromText("vco1.fm=lfo1.out@1.5").has_value());                  // above 1
        CHECK(!patchFromText("vco1.fm=lfo1.out@x").has_value());
        CHECK(!patchFromText("vco1.fm=").has_value());
        CHECK(!patchFromText("vco1.fm=lfo1.out;vco1.fm=lfo2.out").has_value());     // input twice
        CHECK(pinsFromText("")->empty());
    }
    if (failures == 0) std::printf("T-S5.14-pins: all passed\n");
    return failures == 0 ? 0 : 1;
}
