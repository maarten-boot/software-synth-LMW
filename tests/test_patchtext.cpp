// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.4-text: cables as text (saved state; later patch files and 9P).
#include "dsp/PatchText.h"
#include <cstdio>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    Patch p;
    CHECK(patchToText(p).empty());
    p.connect(In::VcfIn, Out::Vcf);          // feedback cable
    p.connect(In::VcoFm, Out::Env2);
    p.connect(In::OutIn, Out::Noise);
    CHECK(patchToText(p) == "vco.fm=env2.out;vcf.in=vcf.out;out.in=noise.out");

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

    CHECK(!patchFromText("vco.fm").has_value());                    // no '='
    CHECK(!patchFromText("vco.fm=nothing.out").has_value());        // unknown output
    CHECK(!patchFromText("vco.out=lfo.out").has_value());           // an output is not an input
    CHECK(!patchFromText("vco.fm=lfo.out;vco.fm=env1.out").has_value());   // input twice
    CHECK(!patchFromText("vco.fm=lfo.out;").has_value());           // trailing separator
    CHECK(!patchFromText(";").has_value());
    CHECK(inputByName("vca.cv") == In::VcaCv);
    CHECK(outputByName("mix.out") == Out::Mix);

    if (failures == 0) std::printf("T-S5.4-text: all passed\n");
    return failures == 0 ? 0 : 1;
}
