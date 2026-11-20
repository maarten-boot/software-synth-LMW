// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.17: VCF routing. Serial (default) keeps the normalled sources as before; Parallel normals vcf2.in
// from mix.out and formant.in from vcf1.out and vcf2.out at 50 % each — bit-identical to the same
// connections made with cables; a cable still overrides; serial and parallel sound different.
#include "dsp/Engine.h"
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(VcfRouting r, float cutoff2 = 18000.0f) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p);
    ModParams m; m.vcfRouting = r; m.cutoff = 500.0f; m.resonance = 0.3f;
    m.vcf2Cutoff = cutoff2; m.vcf2Type = FilterType::Highpass;      // a low and a high filter side by side
    e.setModParams(m);
    return e;
}

int main() {
    // Normalled sources.
    for (int i = 0; i < numInputs; ++i) {
        const auto in = static_cast<In>(i);
        const auto s = normals(in, VcfRouting::Serial);
        CHECK(s.n == (normalled[static_cast<std::size_t>(i)] == noSource ? 0 : 1));
        if (in != In::Vcf2In && in != In::FormantIn) {
            const auto p = normals(in, VcfRouting::Parallel);
            CHECK(p.n == s.n && (s.n == 0 || (p.s[0].source == s.s[0].source && p.s[0].level == 1.0f)));
        }
    }
    {
        const auto v = normals(In::Vcf2In, VcfRouting::Parallel), f = normals(In::FormantIn, VcfRouting::Parallel);
        CHECK(v.n == 1 && v.s[0].source == Out::Mix && v.s[0].level == 1.0f);
        CHECK(f.n == 2 && f.s[0].source == Out::Vcf1 && f.s[1].source == Out::Vcf2 && f.s[0].level == 0.5f && f.s[1].level == 0.5f);
        CHECK(normalLevel(In::FormantIn, Out::Vcf2, VcfRouting::Serial) == 1.0f && normalLevel(In::FormantIn, Out::Vcf1, VcfRouting::Serial) == 0.0f);
        Patch p;
        CHECK(p.routing() == VcfRouting::Serial && p.onlySource(In::FormantIn, Out::Vcf2) && p.source(In::Vcf2In) == Out::Vcf1);
        p.setRouting(VcfRouting::Parallel);
        CHECK(!p.onlySource(In::FormantIn, Out::Vcf2) && p.source(In::Vcf2In) == Out::Mix && p.source(In::FormantIn) == Out::Vcf1);
    }
    // Parallel = serial with the same connections as cables, bit for bit.
    {
        Engine a = makeEngine(VcfRouting::Parallel, 2000.0f), b = makeEngine(VcfRouting::Serial, 2000.0f);
        b.connect(In::Vcf2In, Out::Mix);
        b.addPin(In::FormantIn, Out::Vcf1, 0.5f);
        b.addPin(In::FormantIn, Out::Vcf2, 0.5f);
        a.noteOn(45, 0.8f); b.noteOn(45, 0.8f);
        CHECK(render(a, 24000) == render(b, 24000));
    }
    // A cable overrides the parallel normalling: formant.in <- vcf2.out alone.
    {
        Engine a = makeEngine(VcfRouting::Parallel, 2000.0f), b = makeEngine(VcfRouting::Serial, 2000.0f);
        a.connect(In::FormantIn, Out::Vcf2);
        b.connect(In::Vcf2In, Out::Mix);
        b.connect(In::FormantIn, Out::Vcf2);
        a.noteOn(45, 0.8f); b.noteOn(45, 0.8f);
        CHECK(render(a, 24000) == render(b, 24000));
    }
    // Serial and parallel differ; serial is the default.
    {
        Engine a = makeEngine(VcfRouting::Parallel, 2000.0f), b = makeEngine(VcfRouting::Serial, 2000.0f);
        a.noteOn(45, 0.8f); b.noteOn(45, 0.8f);
        CHECK(render(a, 24000) != render(b, 24000));
        CHECK(ModParams{}.vcfRouting == VcfRouting::Serial && Patch{}.routing() == VcfRouting::Serial);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S5.17-routing: all passed\n", failures);
    return failures ? 1 : 0;
}
