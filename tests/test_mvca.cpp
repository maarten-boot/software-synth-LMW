// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.32-mvca (unit): the mini VCAs. out = in x clamp(Level + cv, 0, 1), per voice, nothing until patched.
// Measured through the main VCA (one sample later: the mini VCAs run before the VCOs): vca.in = mvca1.out (ENV 1 at sustain 1, so the VCA passes it at the velocity gain);
// each case compared with the same VCO cabled straight to vca.in at the expected gain.
// The user's example: vco2.out -> mvca1.in, env2.out -> mvca1.cv, mvca1.out -> vco1.pm — PM that follows ENV 2.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;
static constexpr int N = 4800;

static double diff(const std::vector<float>& a, const std::vector<float>& b, int from = 0, int to = N) {
    double d = 0; for (int i = from; i < to; ++i) d += std::fabs(a[static_cast<std::size_t>(i)] - b[static_cast<std::size_t>(i)]); return d;
}
// The mini VCAs run before the VCOs (an ENV at CV reaches a VCO's PM in the same sample), so a VCO through one comes
// one sample later: `a` compared with `b` one sample earlier.
static double diff1(const std::vector<float>& a, const std::vector<float>& b) {
    double d = 0; for (int i = 1; i < N; ++i) d += std::fabs(a[static_cast<std::size_t>(i)] - b[static_cast<std::size_t>(i - 1)]); return d;
}
static double energy(const std::vector<float>& a, int from = 0, int to = N) {
    double d = 0; for (int i = from; i < to; ++i) d += std::fabs(a[static_cast<std::size_t>(i)]); return d;
}

struct Setup {
    float level = 0.0f;
    std::vector<std::tuple<In, Out, float>> pins;
    ModParams mod{};
    VoiceParams voice{};
    Setup() { voice.attack = 0.0f; voice.decay = 0.0f; voice.sustain = 1.0f; voice.release = 0.01f; }
};

static std::vector<float> render(const Setup& s) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    e.setParams(s.voice);
    ModParams m = s.mod; m.mvcaLevel = {s.level, 0.0f, 0.0f}; e.setModParams(m);
    e.setVoiceCount(1);
    for (const auto& [in, out, lvl] : s.pins) e.addPin(in, out, lvl);
    e.noteOn(57, 0.8f);
    std::vector<float> l(N); e.render(l.data(), nullptr, N); return l;
}

int main() {
    // Straight: vco1.out -> vca.in at `gain` (the reference).
    auto straight = [](float gain) {
        Setup s; if (gain > 0.0f) s.pins = {{In::VcaIn, Out::Vco1, gain}}; else s.pins = {{In::VcaIn, Out::KeysGate, 0.0f}};
        return render(s);
    };
    auto through = [](float level, std::vector<std::tuple<In, Out, float>> extra = {}) {
        Setup s; s.level = level;
        s.pins = {{In::Mvca1In, Out::Vco1, 1.0f}, {In::VcaIn, Out::Mvca1, 1.0f}};
        for (auto& p : extra) s.pins.push_back(p);
        return render(s);
    };
    const auto ref1 = straight(1.0f), ref05 = straight(0.5f), ref07 = straight(0.7f);
    CHECK(energy(ref1) > 100.0);
    const double tol = 1e-3 * energy(ref1);   // the VCA's envelope is one sample ahead of the delayed input: tiny differences at the start
    std::printf("reference energy %.1f; level 1 through vs straight %.5f\n", energy(ref1), diff1(through(1.0f), ref1));
    // Level alone: 0 = silence, 1 = the input exactly, 0.5 = half.
    CHECK(energy(through(0.0f)) == 0.0);
    CHECK(diff1(through(1.0f), ref1) < tol);
    CHECK(diff1(through(0.5f), ref05) < tol);
    // CV adds: keys.vel 0.8 at 50 % = +0.4; Level 0.3 -> gain 0.7.
    const auto cv = through(0.3f, {{In::Mvca1Cv, Out::KeysVel, 0.5f}});
    std::printf("Level 0.3 + cv 0.4 vs straight 0.7: %.6f\n", diff1(cv, ref07));
    CHECK(diff1(cv, ref07) < tol);
    // Clamped at 1 (0.8 + 0.4) and at 0 (0 - 0.4).
    CHECK(diff1(through(0.8f, {{In::Mvca1Cv, Out::KeysVel, 0.5f}}), ref1) < tol);
    CHECK(energy(through(0.0f, {{In::Mvca1Cv, Out::KeysVel, -0.5f}})) == 0.0);
    // Nothing at In: 0 whatever the Level and CV.
    {
        Setup s; s.level = 1.0f; s.pins = {{In::VcaIn, Out::Mvca1, 1.0f}, {In::Mvca1Cv, Out::KeysVel, 1.0f}};
        CHECK(energy(render(s)) == 0.0);
    }
    // A bipolar LFO on CV with Level 0 gates on its positive half: about half the samples silent over whole cycles.
    {
        Setup s; s.mod.lfo2Rate = 10.0f;
        s.pins = {{In::Mvca1In, Out::Vco1, 1.0f}, {In::VcaIn, Out::Mvca1, 1.0f}, {In::Mvca1Cv, Out::Lfo2, 1.0f}};
        const auto x = render(s);
        int silent = 0; for (const float v : x) silent += v == 0.0f;
        std::printf("LFO 2 on cv, Level 0: %d of %d samples silent\n", silent, N);
        CHECK(silent > N * 4 / 10 && silent < N * 6 / 10);
    }
    // The other two units are the same and independent: MVCA 2 at Level 1 passes, MVCA 1 at Level 0 does not.
    {
        Setup s; s.mod.mvcaLevel = {0.0f, 1.0f, 0.0f};
        s.pins = {{In::Mvca2In, Out::Vco1, 1.0f}, {In::VcaIn, Out::Mvca2, 1.0f}};
        Engine e; e.prepare(SR); e.setGain(1.0f); e.setParams(s.voice); e.setModParams(s.mod); e.setVoiceCount(1);
        for (const auto& [in, out, lvl] : s.pins) e.addPin(in, out, lvl);
        e.noteOn(57, 0.8f);
        std::vector<float> l(N); e.render(l.data(), nullptr, N);
        CHECK(diff1(l, ref1) < tol);
    }
    // The user's example: PM through MVCA 1 with ENV 2 (attack 0, decay 50 ms, sustain 0) at CV, Level 0. At the start
    // (envelope ~1) it sounds as plain PM; once the envelope has decayed, as no PM at all.
    {
        auto pmRender = [](int mode) {   // 0: no PM, 1: plain PM (vco2 -> vco1.pm), 2: through MVCA 1 with ENV 2
            Setup s; s.voice.env2Attack = 0.0f; s.voice.env2Decay = 0.05f; s.voice.env2Sustain = 0.0f;
            s.mod.pmIndex = {3.0f, 0.0f, 0.0f};
            if (mode == 1) s.pins = {{In::Vco1Pm, Out::Vco2, 1.0f}};
            if (mode == 2) s.pins = {{In::Mvca1In, Out::Vco2, 1.0f}, {In::Mvca1Cv, Out::Env2, 1.0f}, {In::Vco1Pm, Out::Mvca1, 1.0f}};
            return render(s);
        };
        const auto none = pmRender(0), plain = pmRender(1), env = pmRender(2);
        const double early = diff(env, plain, 0, 48) / (diff(none, plain, 0, 48) + 1e-9);
        const double late = diff(env, none, 4000, N), lateRef = diff(plain, none, 4000, N);
        std::printf("example: first 1 ms vs plain PM %.3f (of the no-PM difference); last 17 ms vs no PM %.5f (plain PM %.2f)\n",
                    early, late, lateRef);
        CHECK(early < 0.2);
        CHECK(lateRef > 1.0 && late < 0.01 * lateRef);
    }
    // Ports: names, types, no normals, Matrix places.
    CHECK(inputNames[static_cast<std::size_t>(In::Mvca2Cv)] == "mvca2.cv" && outputNames[static_cast<std::size_t>(Out::Mvca3)] == "mvca3.out");
    for (int n = 0; n < numMvcas; ++n) {
        CHECK(typeOf(mvcaIn(n)) == SignalType::Audio && typeOf(mvcaCv(n)) == SignalType::Cv && typeOf(mvcaOut(n)) == SignalType::Audio);
        CHECK(normalled[static_cast<std::size_t>(mvcaIn(n))] == noSource && normalled[static_cast<std::size_t>(mvcaCv(n))] == noSource);
        CHECK(bipolar(mvcaCv(n)) && !bipolar(mvcaIn(n)) && audioInput(mvcaIn(n)) && !soundSource(mvcaOut(n)));
    }
    {
        const auto si = synthMatrixInputs(), so = synthMatrixOutputs();
        std::size_t mix1 = 0, m1 = 0, mixOut = 0, mo = 0;
        for (std::size_t k = 0; k < si.size(); ++k) { if (si[k] == static_cast<int>(In::MixIn1)) mix1 = k; if (si[k] == static_cast<int>(In::Mvca1In)) m1 = k; }
        for (std::size_t k = 0; k < so.size(); ++k) { if (so[k] == static_cast<int>(Out::Mix)) mixOut = k; if (so[k] == static_cast<int>(Out::Mvca1)) mo = k; }
        CHECK(m1 + 6 == mix1 && mo + 3 == mixOut);                 // just before MIXER, as in the rack
    }
    // Default patch: the mini VCAs are not connected, so they are no effect (S6.26).
    {
        Patch p; UseParams up;
        const auto r = unitUse(p, up);
        CHECK(r.unit("mvca1") == Use::Unused && r.unit("vco1") == Use::Used);
    }
    // The example patched (PM 3, MVCA 1 Level 0, ENV 2 at CV): MVCA 1, VCO 2 and ENV 2 count.
    {
        Patch p;
        CHECK(p.connect(In::Mvca1In, Out::Vco2) && p.connect(In::Mvca1Cv, Out::Env2) && p.connect(In::Vco1Pm, Out::Mvca1));
        UseParams up;
        up.gain[static_cast<std::size_t>(In::Vco1Pm)] = 3.0f;
        up.gain[static_cast<std::size_t>(In::MixIn2)] = 0.0f; up.gain[static_cast<std::size_t>(In::MixIn3)] = 0.0f;
        up.gain[static_cast<std::size_t>(In::Mvca1In)] = 1.0f;   // the processor: Level 0 but CV patched
        up.gain[static_cast<std::size_t>(In::Mvca1Cv)] = 1.0f;
        const auto r = unitUse(p, up);
        std::printf("example use: mvca1 %d vco2 %d env2 %d\n", static_cast<int>(r.unit("mvca1")), static_cast<int>(r.unit("vco2")), static_cast<int>(r.unit("env2")));
        CHECK(r.unit("mvca1") == Use::Used && r.unit("vco2") == Use::Used && r.unit("env2") == Use::Used);
    }
    if (failures == 0) std::printf("T-S5.32-mvca: all passed\n");
    return failures == 0 ? 0 : 1;
}
