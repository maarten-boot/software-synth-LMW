// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.26-unused (unit): which units have an effect (src/dsp/Patch.h unitUse). The default sound: VCO 2, VCO 3,
// NOISE, LFO 1, LFO 2, ENV 2 unused (their levels 0); FORMANT and the effects passing (Mix 0); the rest used.
// Raising a level, a cable at 0, a chain through an unused unit, an effect's Mix, the reasons.
#include "dsp/Patch.h"
#include <cstdio>
#include <string>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

// The gains of the default parameters (S5a.1): mix.vco1 1, the other levels, FM, PM, Env, Tremolo and FORMANT Mix 0.
static UseParams defaults() {
    UseParams u;
    for (const In i : {In::MixIn2, In::MixIn3, In::MixIn4, In::Vco1Fm, In::Vco2Fm, In::Vco3Fm, In::Vco1Pm, In::Vco2Pm,
                       In::Vco3Pm, In::Vcf1Cutoff, In::Vcf2Cutoff, In::FormantVowel, In::Mix2In2, In::Mix2In3, In::VcaMod})
        u.gain[static_cast<std::size_t>(i)] = 0.0f;
    for (const Out o : {Out::Formant, Out::CombOut, Out::EnsembleOut, Out::DelayOut, Out::ReverbOut})
        u.passing[static_cast<std::size_t>(o)] = true;
    return u;
}
static std::string gainName(In) { return "level"; }
static const char* name(Use u) { return u == Use::Used ? "used" : u == Use::Passing ? "passing" : "unused"; }

int main() {
    Patch p;
    auto u = defaults();
    auto r = unitUse(p, u);
    for (const char* unit : {"keys", "vco1", "vco2", "vco3", "noise", "lfo1", "lfo2", "env1", "env2", "mix", "vcf1", "vcf2",
                             "formant", "mix2", "vca", "out", "comb", "ensemble", "delay", "reverb", "main"})
        std::printf("%-9s %s\n", unit, name(r.unit(unit)));
    for (const char* unit : {"vco2", "vco3", "noise", "lfo1", "lfo2", "env2"}) CHECK(r.unit(unit) == Use::Unused);
    for (const char* unit : {"formant", "comb", "ensemble", "delay", "reverb"}) CHECK(r.unit(unit) == Use::Passing);
    for (const char* unit : {"keys", "vco1", "mix", "vcf1", "vcf2", "mix2", "vca", "out", "env1", "main"}) CHECK(r.unit(unit) == Use::Used);
    const auto why = unusedReason(p, u, r, "vco2", gainName);
    std::printf("vco2: %s\nlfo1: %s\n", why.c_str(), unusedReason(p, u, r, "lfo1", gainName).c_str());
    CHECK(why == "goes to mix.in2 (level 0)");

    // mix.vco2 up: VCO 2 used. vco1.fm up: LFO 1 used. REVERB Mix up: REVERB used.
    u.gain[static_cast<std::size_t>(In::MixIn2)] = 0.5f;
    u.gain[static_cast<std::size_t>(In::Vco1Fm)] = 0.1f;
    u.passing[static_cast<std::size_t>(Out::ReverbOut)] = false;
    r = unitUse(p, u);
    CHECK(r.unit("vco2") == Use::Used && r.unit("lfo1") == Use::Used && r.unit("reverb") == Use::Used);
    CHECK(r.unit("vco3") == Use::Unused);

    // A cable at 0 does not count: VCO 3 -> vcf1.in at 0 % replaces the normal mix.out, so VCO 1, 2 and MIXER lose.
    u = defaults();
    CHECK(p.connect(In::Vcf1In, Out::Vco3, 0.0f));
    r = unitUse(p, u);
    CHECK(r.unit("vco3") == Use::Unused && r.unit("mix") == Use::Unused && r.unit("vco1") == Use::Unused);
    std::printf("vco3 with a 0 %% cable: %s\n", unusedReason(p, u, r, "vco3", gainName).c_str());
    CHECK(unusedReason(p, u, r, "vco3", gainName) == "goes to mix.in3 (level 0); vcf1.in (cable at 0 %)");
    p.clear();

    // A unit feeding only an unused unit is unused: LFO 2 -> vco2.fm (FM depth up) while VCO 2's level is 0.
    u = defaults();
    u.gain[static_cast<std::size_t>(In::Vco2Fm)] = 1.0f;
    CHECK(p.connect(In::Vco2Fm, Out::Lfo2, 1.0f));
    r = unitUse(p, u);
    CHECK(r.unit("lfo2") == Use::Unused);
    std::printf("lfo2 into an unused VCO 2: %s\n", unusedReason(p, u, r, "lfo2", gainName).c_str());
    u.gain[static_cast<std::size_t>(In::MixIn2)] = 1.0f;
    CHECK(unitUse(p, u).unit("lfo2") == Use::Used);
    p.clear();

    // Not connected at all: NOISE cable to nothing (its normal goes to mix.in4): a cable from VCO 1 replaces it.
    u = defaults();
    CHECK(p.connect(In::MixIn4, Out::Vco1, 1.0f));
    r = unitUse(p, u);
    CHECK(unusedReason(p, u, r, "noise", gainName) == "not connected to MAIN");
    if (failures == 0) std::printf("T-S6.26-unused: all passed\n");
    return failures == 0 ? 0 : 1;
}
