// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.27-hint (unit): the hover line naming the normal connection a cable replaced (src/dsp/Patch.h replacedNote)
// and silent loops in the unit rule (S6.26): vcf1.out -> vcf1.in alone carries no sound, nor does the chain after it.
#include "dsp/Ladder.h"
#include "dsp/Patch.h"
#include <cmath>
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
    CHECK(one == "Replaces the normal connection mix.out \xe2\x86\x92 vcf1.in; to keep it, add mix.out as a cable too.");
    CHECK(p.connect(In::Vcf1In, Out::Mix, 1.0f));
    CHECK(replacedNote(p, In::Vcf1In).empty());
    p.clear();
    // Two normal sources: FORMANT in parallel routing (vcf1.out and vcf2.out at 50 %).
    p.setRouting(VcfRouting::Parallel);
    CHECK(p.connect(In::FormantIn, Out::Vco2, 1.0f));
    const auto two = replacedNote(p, In::FormantIn);
    std::printf("%s\n", two.c_str());
    CHECK(two == "Replaces the normal connections vcf1.out and vcf2.out \xe2\x86\x92 formant.in; to keep them, add vcf1.out and vcf2.out as cables too.");
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
    // Choice C (Ladder.h): with no input at all, a VCF above the threshold starts ringing on its own; below it, and at
    // the default (Res 0), it stays exactly 0.
    for (const float res : {1.0f, 0.95f, 0.9f, 0.0f}) {
        lmw::dsp::Ladder f;
        f.setSampleRate(48000.0);
        f.setResonance(res);
        f.setCutoff(1000.0);
        float peak = 0.0f; int at = -1;
        for (int i = 0; i < 48000; ++i) { const float y = f.process(0.0f); peak = std::max(peak, std::fabs(y)); if (at < 0 && std::fabs(y) > 0.05f) at = i; }
        std::printf("VCF alone, Res %3.0f %%, 1 kHz: peak %.3f, 0.05 after %.1f ms\n", res * 100.0, double(peak), at < 0 ? -1.0 : at / 48.0);
        if (res > 0.91f) CHECK(peak > 0.3f && at >= 0 && at < 48 * 150);
        else CHECK(peak == 0.0f);
    }
    // Its own output back into its input at level L takes about L off the feedback: Res 100 % rings with L 0.3, not 0.5.
    for (const float L : {0.3f, 0.5f}) {
        lmw::dsp::Ladder f;
        f.setSampleRate(48000.0);
        f.setResonance(1.0f);
        f.setCutoff(1000.0);
        float y = 0.0f, peak = 0.0f;
        for (int i = 0; i < 48000; ++i) { y = f.process(L * y); if (i > 24000) peak = std::max(peak, std::fabs(y)); }
        std::printf("VCF loop at %.0f %%, Res 100 %%: peak %.3f\n", L * 100.0, double(peak));
        CHECK(L < 0.4f ? peak > 0.05f : peak < 1e-3f);
    }
    if (failures == 0) std::printf("T-S6.27-hint: all passed\n");
    return failures == 0 ? 0 : 1;
}
