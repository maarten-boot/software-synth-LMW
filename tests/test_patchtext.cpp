// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.4-text: cables as text (saved state, patch files and 9P).
#include "dsp/PatchText.h"
#include "app/StateMigration.h"
#include <cstdio>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    Patch p;
    CHECK(patchToText(p).empty());
    p.connect(In::Vcf1In, Out::Vcf1);          // feedback cable
    p.connect(In::Vco1Fm, Out::Env2);
    p.connect(In::OutIn, Out::Noise);
    CHECK(patchToText(p) == "vco1.fm=env2.out;vcf1.in=vcf1.out;out.in=noise.out");

    auto q = patchFromText(patchToText(p));
    CHECK(q.has_value());
    if (q) {
        for (int i = 0; i < numInputs; ++i) {
            const auto in = static_cast<In>(i);
            CHECK(q->isPatched(in) == p.isPatched(in));
            CHECK(q->source(in) == p.source(in));
        }
    }
    auto empty = patchFromText("");
    CHECK(empty.has_value() && patchToText(*empty).empty());

    CHECK(!patchFromText("vco1.fm").has_value());                    // no '='
    CHECK(!patchFromText("vco1.fm=nothing.out").has_value());        // unknown output
    CHECK(!patchFromText("vco1.out=lfo1.out").has_value());           // an output is not an input
    CHECK(!patchFromText("vco1.fm=lfo1.out;vco1.fm=env1.out").has_value());   // input twice
    CHECK(!patchFromText("vco1.fm=lfo1.out;").has_value());           // trailing separator
    CHECK(!patchFromText(";").has_value());
    CHECK(inputByName("vca.cv") == In::VcaCv);
    CHECK(outputByName("mix.out") == Out::Mix);

    // T-S5.10b, T-S5.11b: cable lists and parameter IDs from older states are translated.
    {
        using namespace lmw::migration;
        // version 1 (single VCO, single VCF; mix.in2 was the noise input)
        CHECK(cables(1, "vco.fm=env2.out;mix.in2=vco.out;vcf.in=vcf.out") == "vco1.fm=env2.out;mix.in4=vco1.out;vcf1.in=vcf1.out");
        CHECK(cables(1, "").empty());
        const auto q2 = patchFromText(cables(1, "vco.pitch=env2.out;out.in=vco.out"));
        CHECK(q2.has_value() && q2->source(In::Vco1Pitch) == Out::Env2 && q2->source(In::OutIn) == Out::Vco1);
        CHECK(parameterId(1, "vco.wave") == "vco1.wave");
        CHECK(parameterId(1, "vco.fm") == "vco1.fm");
        CHECK(parameterId(1, "mix.vco") == "mix.vco1");
        CHECK(parameterId(1, "mix.noise") == "mix.noise");          // unchanged
        CHECK(parameterId(1, "vcf.cutoff") == "vcf1.cutoff");       // via version 2
        // version 2 (VCO 1-3, single VCF): mix.in2 is VCO 2 already and must stay
        CHECK(cables(2, "mix.in2=lfo.out;vca.in=vcf.out;vcf.cutoff=env1.out") == "mix.in2=lfo1.out;vca.in=vcf1.out;vcf1.cutoff=env1.out");
        CHECK(parameterId(2, "vcf.res") == "vcf1.res");
        CHECK(parameterId(2, "vcf.slope") == "vcf1.slope");
        CHECK(parameterId(2, "vco1.fm") == "vco1.fm");
        // version 3 (one LFO): lfo.* -> lfo1.* (T-S5.12)
        CHECK(cables(3, "vcf1.in=vcf2.out;env1.gate=lfo.out") == "vcf1.in=vcf2.out;env1.gate=lfo1.out");
        CHECK(parameterId(3, "lfo.rate") == "lfo1.rate");
        CHECK(parameterId(3, "lfo.wave") == "lfo1.wave");
        CHECK(parameterId(3, "vcf.cutoff") == "vcf.cutoff");
        const auto q3 = patchFromText(cables(3, "env2.gate=lfo.out"));
        CHECK(q3.has_value() && q3->source(In::Env2Gate) == Out::Lfo1);
        // current version: nothing changes
        // Pins with levels (S5.14), e.g. hand-written in an older patch file: every name translated.
        CHECK(cables(3, "vcf1.cutoff=lfo.out@0.5+env2.out;vca.cv = env1.out + lfo.out@50%")
              == "vcf1.cutoff=lfo1.out@0.5+env2.out;vca.cv = env1.out + lfo1.out@50%");
        CHECK(cables(1, "vcf.cutoff=vco.out@.25+lfo.out") == "vcf1.cutoff=vco1.out@.25+lfo1.out");
        CHECK(cables(4, "vca.mod=lfo1.out;out.in=lfo2.out") == "vca.mod=lfo1.out;out.in=lfo2.out");
        CHECK(parameterId(4, "lfo.rate") == "lfo.rate");
        // new ports by name; vca.mod is normalled from lfo2.out
        const auto q4 = patchFromText("vca.mod=lfo1.out");
        CHECK(q4.has_value() && q4->source(In::VcaMod) == Out::Lfo1);
        CHECK(Patch{}.source(In::VcaMod) == Out::Lfo2);
    }
    if (failures == 0) std::printf("T-S5.4-text: all passed\n");
    return failures == 0 ? 0 : 1;
}
