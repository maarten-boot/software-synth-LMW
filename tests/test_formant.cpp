// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.16: FORMANT module. (a) formant table: rising formants, baritone between bass and tenor, child
// above soprano, glides between vowels; (b) the filter peaks at the vowel's first two formants and
// dips between them; (c) a saw keeps about its loudness at Mix 100 % for every voice and vowel;
// (d) engine: Mix 0 (default) is bit-identical to bypassing FORMANT by cable; Mix 1 changes the sound;
// the Vowel input adds 4 x its signal to the fader; voices and vowels sound different.
#include "dsp/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static double rms(const std::vector<float>& x, std::size_t from = 0) {
    double s = 0; for (std::size_t i = from; i < x.size(); ++i) s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / static_cast<double>(x.size() - from));
}
// Steady-state gain of the filter for a sine at `hz`.
static double sineGain(FormantVoice v, float vowel, double hz) {
    FormantFilter f; f.setSampleRate(SR); f.reset(); f.setTarget(v, vowel, 0.5f);
    std::vector<float> y(24000);
    for (std::size_t i = 0; i < y.size(); ++i)
        y[i] = f.process(static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / SR)));
    return rms(y, 12000) * std::sqrt(2.0);
}
static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(const ModParams& m) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p); e.setModParams(m);
    return e;
}

int main() {
    const auto all = {FormantVoice::Bass, FormantVoice::Baritone, FormantVoice::Tenor,
                      FormantVoice::Alto, FormantVoice::Soprano, FormantVoice::Child};
    // (a) table
    for (auto v : all)
        for (int vowel = 0; vowel < numVowels; ++vowel) {
            const auto s = formants(v, static_cast<float>(vowel));
            for (std::size_t f = 1; f < numFormants; ++f) CHECK(s.freq[f] > s.freq[f - 1]);
            CHECK(s.gainDb[0] == 0.0f);
        }
    CHECK(formants(FormantVoice::Bass, 0).freq[0] == 600.0f && formants(FormantVoice::Tenor, 2).freq[1] == 1870.0f);
    {
        const float b = formants(FormantVoice::Baritone, 0).freq[0];
        CHECK(b > 600.0f && b < 650.0f);
        CHECK(formants(FormantVoice::Child, 0).freq[0] > formants(FormantVoice::Soprano, 0).freq[0]);
        // halfway A -> E: F2 the geometric mean of 1080 and 1700 (tenor)
        const float mid = formants(FormantVoice::Tenor, 0.5f).freq[1];
        CHECK(std::fabs(mid - std::sqrt(1080.0f * 1700.0f)) < 1.0f);
        CHECK(formants(FormantVoice::Tenor, -3.0f).freq[0] == 650.0f && formants(FormantVoice::Tenor, 9.0f).freq[0] == 350.0f);
        CHECK(formants(FormantVoice::Tenor, std::nanf("")).freq[0] == 650.0f);   // NaN: A, no index out of range
    }
    // (b) response: peaks at F1 and F2, a dip between them (tenor A: 650, 1080; I: 290, 1870)
    for (float vowel : {0.0f, 2.0f}) {
        const auto s = formants(FormantVoice::Tenor, vowel);
        const double f1 = sineGain(FormantVoice::Tenor, vowel, s.freq[0]), f2 = sineGain(FormantVoice::Tenor, vowel, s.freq[1]);
        const double between = sineGain(FormantVoice::Tenor, vowel, std::sqrt(s.freq[0] * s.freq[1]));
        const double far = sineGain(FormantVoice::Tenor, vowel, 100.0);
        std::printf("tenor vowel %.0f: F1 %.2f, F2 %.2f, between %.2f, 100 Hz %.3f\n", vowel, f1, f2, between, far);
        CHECK(f1 > 2.0 * between && f2 > 1.2 * between && f1 > 5.0 * far);
    }
    // (c) loudness of a saw (C3) at Mix 100 %, every voice and vowel: within -12..+6 dB of the dry saw
    {
        double lo = 1e9, hi = 0;
        for (auto v : all)
            for (int vowel = 0; vowel < numVowels; ++vowel) {
                FormantFilter f; f.setSampleRate(SR); f.reset(); f.setTarget(v, static_cast<float>(vowel), 0.5f);
                Oscillator o; o.setSampleRate(SR); o.setWaveform(Waveform::Saw); o.setFrequency(130.81);
                std::vector<float> dry(24000), wet(24000);
                for (std::size_t i = 0; i < dry.size(); ++i) { dry[i] = o.next(); wet[i] = f.process(dry[i]); }
                const double db = 20.0 * std::log10(rms(wet, 4800) / rms(dry, 4800));
                lo = std::min(lo, db); hi = std::max(hi, db);
            }
        std::printf("saw through FORMANT: %.1f .. %.1f dB\n", lo, hi);
        CHECK(lo > -12.0 && hi < 6.0);
    }
    // (d) engine. Mix 0: the same as a cable vca.in <- vcf2.out (FORMANT bypassed), bit for bit.
    {
        ModParams m; m.formantVowel = 3.0f; m.formantVoice = FormantVoice::Soprano;   // ignored at Mix 0
        Engine a = makeEngine(ModParams{}), b = makeEngine(m);
        b.connect(In::VcaIn, Out::Vcf2);
        a.noteOn(48, 0.8f); b.noteOn(48, 0.8f);
        CHECK(render(a, 24000) == render(b, 24000));
    }
    // Mix 1: different from the dry sound; each vowel differs from the others.
    {
        std::vector<std::vector<float>> outs;
        Engine dry = makeEngine(ModParams{});
        dry.noteOn(48, 0.8f);
        const auto d = render(dry, 24000);
        for (int vowel = 0; vowel < numVowels; ++vowel) {
            ModParams m; m.formantMix = 1.0f; m.formantVowel = static_cast<float>(vowel);
            Engine e = makeEngine(m);
            e.noteOn(48, 0.8f);
            outs.push_back(render(e, 24000));
            CHECK(outs.back() != d);
            CHECK(rms(outs.back(), 4800) > 0.05 * rms(d, 4800));
        }
        for (std::size_t i = 0; i < outs.size(); ++i)
            for (std::size_t j = i + 1; j < outs.size(); ++j) CHECK(outs[i] != outs[j]);
    }
    // Vowel input: fader 0 + keys.vel (velocity 1) at 50 % = position 2 (I), the same as fader 2.
    {
        ModParams a; a.formantMix = 1.0f; a.formantVowel = 2.0f;
        ModParams b; b.formantMix = 1.0f; b.formantVowel = 0.0f;
        Engine e1 = makeEngine(a), e2 = makeEngine(b);
        e2.addPin(In::FormantVowel, Out::KeysVel, 0.5f);
        e1.noteOn(50, 1.0f); e2.noteOn(50, 1.0f);
        CHECK(render(e1, 24000) == render(e2, 24000));
        // clamped: fader 4 (U) plus a positive signal stays U
        ModParams c; c.formantMix = 1.0f; c.formantVowel = 4.0f;
        Engine e3 = makeEngine(c), e4 = makeEngine(c);
        e4.addPin(In::FormantVowel, Out::KeysVel, 1.0f);
        e3.noteOn(50, 1.0f); e4.noteOn(50, 1.0f);
        CHECK(render(e3, 24000) == render(e4, 24000));
    }
    // Voice types differ (same vowel).
    {
        ModParams a; a.formantMix = 1.0f; a.formantVoice = FormantVoice::Bass;
        ModParams b = a; b.formantVoice = FormantVoice::Soprano;
        Engine e1 = makeEngine(a), e2 = makeEngine(b);
        e1.noteOn(55, 0.8f); e2.noteOn(55, 0.8f);
        CHECK(render(e1, 24000) != render(e2, 24000));
    }
    // Default patch: formant.in <- vcf2.out, vca.in <- formant.out, formant.vowel has no source.
    CHECK(normalled[static_cast<std::size_t>(In::FormantIn)] == Out::Vcf2);
    CHECK(normalled[static_cast<std::size_t>(In::VcaIn)] == Out::Formant);
    CHECK(normalled[static_cast<std::size_t>(In::FormantVowel)] == noSource);
    CHECK(numInputs == 33 && numOutputs == 21);   // 33 / 21 since S7.8 (delay ports)

    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
