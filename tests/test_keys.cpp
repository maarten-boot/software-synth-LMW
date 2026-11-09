// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.8: KEYS module — keys.pitch / keys.gate / keys.vel outputs; vco.pitch, env1.gate and
// env2.gate inputs (normalled from the keys, overridable by cables).
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine() {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves[0] = Waveform::Sine; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.005f;
    p.env2Attack = 0.0f; p.env2Decay = 0.0f; p.env2Sustain = 1.0f; p.env2Release = 0.005f;
    e.setParams(p);
    e.setModParams(ModParams{});
    return e;
}
static int crossings(const std::vector<float>& x) {
    int z = 0; for (size_t i = 1; i < x.size(); ++i) z += (x[i - 1] < 0 && x[i] >= 0) ? 1 : 0; return z;
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

int main() {
    // T-S5.8a: keys outputs per voice.
    {
        Engine e = makeEngine();
        e.setPitchBend(0.5);
        e.noteOn(72, 0.25f);
        render(e, 1);
        const Voice& v = e.voice(0);
        CHECK(near(v.output(Out::KeysPitch), (72 - 60 + 0.5) / 12.0));
        CHECK(near(v.output(Out::KeysGate), 1.0));
        CHECK(near(v.output(Out::KeysVel), 0.25));
        e.setSustain(true);
        e.noteOff(72); render(e, 1);
        CHECK(near(v.output(Out::KeysGate), 1.0));           // held by the pedal
        e.setSustain(false); render(e, 1);
        CHECK(near(v.output(Out::KeysGate), 0.0));
        e.setPitchBend(0.0); e.noteOn(48, 1.0f); render(e, 1);
        CHECK(near(e.voice(1).output(Out::KeysPitch), -1.0));   // C3 = one octave below 0
    }
    // T-S5.8b: cables from the keys behave exactly like the normalled connections.
    {
        Engine a = makeEngine(), b = makeEngine();
        b.connect(In::Vco1Pitch, Out::KeysPitch);
        b.connect(In::Env1Gate, Out::KeysGate);
        b.connect(In::Env2Gate, Out::KeysGate);
        bool same = true;
        auto both = [&](auto f) { f(a); f(b); };
        both([](Engine& e) { e.noteOn(60, 0.7f); });
        same = same && render(a, 4800) == render(b, 4800);
        both([](Engine& e) { e.noteOn(60, 1.0f); e.noteOn(67, 0.5f); e.setPitchBend(-1.0); });   // retrigger + bend
        same = same && render(a, 4800) == render(b, 4800);
        both([](Engine& e) { e.noteOff(60); e.noteOff(67); });
        same = same && render(a, 4800) == render(b, 4800);
        CHECK(same);
        CHECK(a.activeVoices() == 0 && b.activeVoices() == 0);
    }
    // T-S5.8c: LFO square -> env1.gate retriggers ENV 1 on each rising edge; the voice keeps
    // running after the key is released (drone).
    {
        Engine e = makeEngine();
        ModParams m; m.lfoRate = 10.0f; m.lfoWave = LfoWave::Square; e.setModParams(m);
        e.connect(In::Env1Gate, Out::Lfo);
        e.noteOn(60, 1.0f);
        auto countAttacks = [&](int samples) {
            int attacks = 0; bool on = false;
            for (int i = 0; i < samples; ++i) {
                render(e, 1);
                const float env = e.voice(0).output(Out::Env1);
                if (!on && env > 0.5f) { ++attacks; on = true; }
                if (on && env < 0.05f) on = false;
            }
            return attacks;
        };
        const int held = countAttacks(48000);
        CHECK(held >= 9 && held <= 11);                   // 10 Hz square: ~10 attacks per second
        e.noteOff(60);
        const int released = countAttacks(48000);
        CHECK(released >= 9 && released <= 11);           // drone: keeps pulsing after release
        // Gate back to the keys: the envelope is released and the voice ends.
        e.disconnect(In::Env1Gate);
        render(e, 4800);
        CHECK(e.activeVoices() == 0);
    }
    // T-S5.8d: ENV 2 -> vco.pitch: the pitch no longer follows the key (ENV 2 at 1.0 = C5).
    {
        Engine lo = makeEngine(), hi = makeEngine();
        for (Engine* e : {&lo, &hi}) e->connect(In::Vco1Pitch, Out::Env2);
        lo.noteOn(48, 1.0f); hi.noteOn(84, 1.0f);
        render(lo, 480); render(hi, 480);                 // let ENV 2 reach 1.0
        const auto a = render(lo, 48000), b = render(hi, 48000);
        CHECK(std::abs(crossings(a) - 523) <= 2);         // C5 = 523.25 Hz
        CHECK(std::abs(crossings(b) - 523) <= 2);
    }
    // T-S5.8d: keys.vel -> vcf.cutoff (with vcf.env as depth): a harder note is brighter.
    {
        auto brightness = [](float vel) {
            Engine e = makeEngine();
            VoiceParams p; p.waves[0] = Waveform::Saw; p.attack = 0; p.decay = 0; p.sustain = 1; e.setParams(p);
            ModParams m; m.cutoff = 200.0f; m.envOctaves = 5.0f; e.setModParams(m);
            e.connect(In::Vcf1Cutoff, Out::KeysVel);
            e.noteOn(48, vel);
            render(e, 4800);
            const auto x = render(e, 9600);
            // rms of the first difference / rms of the signal: rises with high-frequency content and
            // does not depend on the VCA's velocity scaling.
            double d = 0, s = 0;
            for (size_t i = 1; i < x.size(); ++i) { d += (double(x[i]) - x[i - 1]) * (double(x[i]) - x[i - 1]); s += double(x[i]) * x[i]; }
            return std::sqrt(d / s);
        };
        CHECK(brightness(1.0f) > 2.0 * brightness(0.2f));
    }
    if (failures == 0) std::printf("T-S5.8-keys: all passed\n");
    return failures == 0 ? 0 : 1;
}
