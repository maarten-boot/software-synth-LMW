// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.27-hint (unit): the hover line naming the normal connection a cable replaced (src/dsp/Patch.h replacedNote)
// and silent loops in the unit rule (S6.26): vcf1.out -> vcf1.in alone carries no sound, nor does the chain after it.
#include "dsp/Patch.h"
#include <cstdio>
#include <string>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static UseParams defaults() {                            // as test_unused: the default faders
    UseParams u;
    for (const In i : {In::MixIn2, In::MixIn3, In::MixIn4, In::Vco1Fm, In::Vco2Fm, In::Vco3Fm, In::Vco1Pm, In::Vco2Pm,
                       In::Vco3Pm, In::Vcf1Cutoff, In::Vcf2Cutoff, In::FormantVowel, In::Mix2In2, In::Mix2In3, In::VcaMod})
        u.gain[static_cast<std::size_t>(i)] = 0.0f;
    for (const Out o : {Out::Formant, Out::CombOut, Out::EnsembleOut, Out::DelayOut, Out::ReverbOut})
        u.passing[static_cast<std::size_t>(o)] = true;
    return u;
}
static std::string gainName(In) { return "level"; }

int main() {
    Patch p;
    // Hover: nothing while unpatched; one replaced source; the normal source also as a cable: nothing.
    CHECK(replacedNote(p, In::Vcf1In).empty());
    CHECK(p.connect(In::Vcf1In, Out::Vcf1, 1.0f));
    const auto one = replacedNote(p, In::Vcf1In);
    std::printf("%s\n", one.c_str());
    CHECK(one == "Replaces the normal connection mix.out → vcf1.in; to keep it, add mix.out as a cable too.");
    CHECK(p.connect(In::Vcf1In, Out::Mix, 1.0f));
    CHECK(replacedNote(p, In::Vcf1In).empty());
    p.clear();
    // Two normal sources: FORMANT in parallel routing (vcf1.out and vcf2.out at 50 %).
    p.setRouting(VcfRouting::Parallel);
    CHECK(p.connect(In::FormantIn, Out::Vco2, 1.0f));
    const auto two = replacedNote(p, In::FormantIn);
    std::printf("%s\n", two.c_str());
    CHECK(two == "Replaces the normal connections vcf1.out and vcf2.out → formant.in; to keep them, add vcf1.out and vcf2.out as cables too.");
    p.clear();
    p.setRouting(VcfRouting::Serial);

    // Silent loop: vcf1.out -> vcf1.in alone. VCF 1 and everything after it silent (no effect); the hover says why.
    const auto u = defaults();
    CHECK(unitUse(p, u).unit("vcf1") == Use::Used);
    CHECK(p.connect(In::Vcf1In, Out::Vcf1, 1.0f));
    auto r = unitUse(p, u);
    for (const char* unit : {"vcf1", "vcf2", "mix2", "vca", "out", "formant", "comb", "reverb", "vco1", "mix", "env1"})
        CHECK(r.unit(unit) == Use::Unused);
    const auto why = unusedReason(p, u, r, "vcf1", gainName), why2 = unusedReason(p, u, r, "vcf2", gainName);
    std::printf("vcf1: %s\nvcf2: %s\nenv1: %s\n", why.c_str(), why2.c_str(), unusedReason(p, u, r, "env1", gainName).c_str());
    CHECK(why == "silent: no sound reaches it (vcf1.in gets only vcf1.out)");
    CHECK(why2 == "silent: no sound reaches it (vcf2.in gets only vcf1.out)");
    // mix.out added back: sound again.
    CHECK(p.connect(In::Vcf1In, Out::Mix, 1.0f));
    r = unitUse(p, u);
    CHECK(r.unit("vcf1") == Use::Used && r.unit("vca") == Use::Used && r.unit("vco1") == Use::Used);
    // Choice C hook: a VCF counted as sounding on its own (self-oscillation) makes the loop heard.
    p.clear();
    CHECK(p.connect(In::Vcf1In, Out::Vcf1, 1.0f));
    auto self = u;
    self.selfSounding[static_cast<std::size_t>(Out::Vcf1)] = true;
    CHECK(unitUse(p, self).unit("vcf1") == Use::Used && unitUse(p, self).unit("vca") == Use::Used);
    if (failures == 0) std::printf("T-S6.27-hint: all passed\n");
    return failures == 0 ? 0 : 1;
}
