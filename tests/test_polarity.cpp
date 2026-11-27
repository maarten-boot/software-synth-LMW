// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.30-polarity and T-S5.29-formantres (unit).
// S5.30: LFO Polarity. Bipolar = the wave exactly; Positive 0..+1 and Negative -1..0 for every wave at Shape 0 / 50 /
// 100 %, the shape kept (half the span); in the engine, lfo2.out Negative on vco1.shape (fader 0) has no effect
// (the shape stays clamped at 0: the plain square exactly), Positive does; the normal LFO 1 -> VCO FM follows.
// S5.29: a cable on formant.res adds to the Reso fader (keys.vel 0.8 at 37.5 % = +0.3 equals the fader 0.3 higher),
// clamped at 1.
#include "dsp/Engine.h"
#include "dsp/Lfo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static double diff(const std::vector<float>& a, const std::vector<float>& b) {
    double d = 0; for (std::size_t i = 0; i < a.size(); ++i) d += std::fabs(a[i] - b[i]); return d;
}

int main() {
    // LFO values for each polarity.
    const char* waves[] = {"sine", "triangle", "saw", "square"};
    for (int w = 0; w < 4; ++w)
        for (const double shape : {0.0, 0.5, 1.0}) {
            Lfo l; l.setSampleRate(SR); l.setRate(5.0); l.setWave(LfoWave(w)); l.setShape(shape);
            float lo = 1e9f, hi = -1e9f, plo = 1e9f, phi = -1e9f, nlo = 1e9f, nhi = -1e9f;
            bool same = true, halfP = true, halfN = true;
            for (int i = 0; i < 48000; ++i) {
                const float x = l.next();
                const float b = polarize(x, LfoPolarity::Bipolar), p = polarize(x, LfoPolarity::Positive), n = polarize(x, LfoPolarity::Negative);
                same = same && b == x;
                halfP = halfP && std::fabs(p - 0.5f * (x + 1.0f)) < 1e-7f;
                halfN = halfN && std::fabs(n - 0.5f * (x - 1.0f)) < 1e-7f;
                lo = std::min(lo, x); hi = std::max(hi, x);
                plo = std::min(plo, p); phi = std::max(phi, p); nlo = std::min(nlo, n); nhi = std::max(nhi, n);
            }
            std::printf("%-8s shape %.1f: +-1 %.3f..%.3f, + %.3f..%.3f, - %.3f..%.3f\n", waves[w], shape, lo, hi, plo, phi, nlo, nhi);
            CHECK(same && halfP && halfN);
            CHECK(plo >= 0.0f && phi <= 1.0f && nlo >= -1.0f && nhi <= 0.0f);
            CHECK(phi - plo > 0.45f * (hi - lo) && phi - plo < 0.55f * (hi - lo));   // half the span: the shape kept
        }

    // Engine: lfo2.out on vco1.shape (a square, fader 0) — Negative never lifts the shape above 0 (the plain square
    // exactly), Positive does; Bipolar too (half the time).
    {
        auto render = [](bool cable, LfoPolarity pol) {
            Engine e; e.prepare(SR); e.setGain(1.0f);
            VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; p.waves[0] = Waveform::Square; e.setParams(p);
            ModParams m; m.lfo2Rate = 3.0f; m.lfoPolarity = {LfoPolarity::Bipolar, pol}; e.setModParams(m);
            e.setVoiceCount(1);
            if (cable) e.addPin(In::Vco1Shape, Out::Lfo2, 1.0f);
            e.noteOn(48, 0.8f);
            std::vector<float> l(9600); e.render(l.data(), nullptr, 9600); return l;
        };
        const auto plain = render(false, LfoPolarity::Bipolar);
        const double dn = diff(render(true, LfoPolarity::Negative), plain), dp = diff(render(true, LfoPolarity::Positive), plain),
                     db = diff(render(true, LfoPolarity::Bipolar), plain);
        std::printf("engine: lfo2 on shape vs none: negative %.4f, positive %.2f, bipolar %.2f\n", dn, dp, db);
        CHECK(dn == 0.0 && dp > 1.0 && db > 1.0);
    }
    // Engine: the normal LFO 1 -> VCO FM follows the polarity (choice C a).
    {
        auto render = [](LfoPolarity pol) {
            Engine e; e.prepare(SR); e.setGain(1.0f);
            VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
            ModParams m; m.fmSemitones = {1.0f, 0.0f, 0.0f}; m.lfoPolarity = {pol, LfoPolarity::Bipolar}; e.setModParams(m);
            e.setVoiceCount(1);
            e.noteOn(60, 0.8f);
            std::vector<float> l(9600); e.render(l.data(), nullptr, 9600); return l;
        };
        const auto b = render(LfoPolarity::Bipolar);
        const double dp = diff(render(LfoPolarity::Positive), b), dn = diff(render(LfoPolarity::Negative), b);
        std::printf("engine: vibrato positive / negative vs bipolar %.2f / %.2f\n", dp, dn);
        CHECK(dp > 1.0 && dn > 1.0);
    }

    // S5.29: formant.res cable adds to the Reso fader.
    {
        auto render = [](float res, float level) {
            Engine e; e.prepare(SR); e.setGain(1.0f);
            VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
            ModParams m; m.formantMix = 1.0f; m.formantVowel = 0.0f; m.formantRes = res; e.setModParams(m);
            e.setVoiceCount(1);
            if (level != 0.0f) e.addPin(In::FormantRes, Out::KeysVel, level);
            e.noteOn(48, 0.8f);
            std::vector<float> l(9600); e.render(l.data(), nullptr, 9600); return l;
        };
        const auto plain = render(0.5f, 0.0f), cabled = render(0.5f, 0.375f), fader = render(0.8f, 0.0f);
        const auto down = render(0.5f, -0.375f), faderDown = render(0.2f, 0.0f);
        const auto over = render(0.9f, 0.375f), top = render(1.0f, 0.0f);
        std::printf("formant.res: cable vs none %.2f, vs fader 0.8 %.5f; -0.3 vs fader 0.2 %.5f; clamp %.5f\n",
                    diff(cabled, plain), diff(cabled, fader), diff(down, faderDown), diff(over, top));
        CHECK(diff(cabled, plain) > 1.0 && diff(cabled, fader) < 1e-3 && diff(down, faderDown) < 1e-3 && diff(over, top) < 1e-3);
        CHECK(bipolar(In::FormantRes) && typeOf(In::FormantRes) == SignalType::Cv && normalled[static_cast<std::size_t>(In::FormantRes)] == noSource);
        CHECK(inputNames[static_cast<std::size_t>(In::FormantRes)] == "formant.res");
    }
    if (failures == 0) std::printf("T-S5.30-polarity, T-S5.29-formantres: all passed\n");
    return failures == 0 ? 0 : 1;
}
