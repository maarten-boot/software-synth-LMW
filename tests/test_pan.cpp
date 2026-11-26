// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.25-pan: stereo and pan per voice in OUT. Defaults give the mono sum on both sides exactly; Pan +-1 puts a
// voice on one side at the same power (equal power, centre = 1 on both sides); Spread by Keys and by Voices;
// out.pan moves a voice; global sources on out.in stay centred; ENSEMBLE keeps a one-sided input on its side.
#include "dsp/Engine.h"
#include "dsp/Ensemble.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.4f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 48000.0;

struct Out2 { std::vector<float> l, r; };
static Out2 render(Engine& e, int n) {
    Out2 o{std::vector<float>(static_cast<size_t>(n)), std::vector<float>(static_cast<size_t>(n))};
    e.render(o.l.data(), o.r.data(), n);
    return o;
}
static Engine makeEngine(const ModParams& m, float gain = 0.3f) {
    Engine e; e.prepare(SR); e.setGain(gain);              // low gain: the limiter stays linear
    VoiceParams p; p.waves[0] = Waveform::Saw; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p); e.setModParams(m);
    return e;
}
static double energy(const std::vector<float>& x, size_t from) {
    double s = 0; for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i]; return s;
}

int main() {
    // Defaults: left = right = the mono render, exactly (a chord).
    {
        Engine a = makeEngine(ModParams{}), b = makeEngine(ModParams{});
        for (int n : {48, 55, 64}) { a.noteOn(n, 0.9f); b.noteOn(n, 0.9f); }
        const auto st = render(a, 9600);
        std::vector<float> mono(9600);
        b.render(mono.data(), nullptr, 9600);
        CHECK(st.l == st.r && st.l == mono);
    }
    // Pan -1 / +1: one side only (the other exactly 0), at the same power as the centre.
    {
        ModParams c, l, r; l.pan = -1.0f; r.pan = 1.0f;
        Engine ec = makeEngine(c), el = makeEngine(l), er = makeEngine(r);
        for (auto* e : {&ec, &el, &er}) e->noteOn(57, 0.9f);
        const auto oc = render(ec, 9600), ol = render(el, 9600), orr = render(er, 9600);
        double rl = 0, lr = 0; for (size_t i = 2000; i < 9600; ++i) { rl = std::fmax(rl, std::fabs(ol.r[i])); lr = std::fmax(lr, std::fabs(orr.l[i])); }
        CHECK(rl == 0.0 && lr == 0.0);
        const double pc = energy(oc.l, 2000) + energy(oc.r, 2000), pl = energy(ol.l, 2000) + energy(ol.r, 2000);
        std::printf("pan -1: power %.4f of the centre\n", pl / pc);
        CHECKV(std::fabs(pl / pc - 1.0) < 0.01, pl / pc);
    }
    // Spread Keys 100 %: C2 left, C4 centre, C6 right.
    {
        ModParams m; m.spread = 1.0f; m.spreadMode = SpreadMode::Keys;
        double bal[3];
        int k = 0;
        for (int note : {36, 60, 84}) {
            Engine e = makeEngine(m);
            e.noteOn(note, 0.9f);
            const auto o = render(e, 9600);
            const double L = energy(o.l, 2000), R = energy(o.r, 2000);
            bal[k++] = (R - L) / (R + L);
        }
        std::printf("spread keys: C2 %.3f, C4 %.3f, C6 %.3f (-1 left .. +1 right)\n", bal[0], bal[1], bal[2]);
        CHECK(bal[0] < -0.5 && std::fabs(bal[1]) < 1e-9 && bal[2] > 0.5);
    }
    // Spread Voices 100 %: the first voice slot left only, the second right only; with one voice: centre.
    {
        ModParams m; m.spread = 1.0f; m.spreadMode = SpreadMode::Voices;
        Engine e = makeEngine(m);
        e.noteOn(57, 0.9f);
        const auto o1 = render(e, 4800);
        double r1 = 0; for (size_t i = 1000; i < o1.r.size(); ++i) r1 = std::fmax(r1, std::fabs(o1.r[i]));
        CHECK(r1 == 0.0 && energy(o1.l, 1000) > 0.0);          // slot 1: left
        Engine e2 = makeEngine(m);
        e2.noteOn(57, 0.9f); e2.noteOn(64, 0.9f); e2.noteOff(57);
        const auto o2 = render(e2, 9600);                        // 57 released; 64 (slot 2) rings: right
        double l2 = 0; for (size_t i = 4800; i < o2.l.size(); ++i) l2 = std::fmax(l2, std::fabs(o2.l[i]));
        CHECK(l2 == 0.0 && energy(o2.r, 4800) > 0.0);
        Engine one = makeEngine(m);
        one.setVoiceCount(1);
        one.noteOn(57, 0.9f);
        const auto o3 = render(one, 4800);
        CHECK(o3.l == o3.r);                                     // one voice: centre
    }
    // out.pan = lfo2.out (square, 1 Hz, starts at +1): the note sits right for the first half second.
    {
        ModParams m; m.lfo2Rate = 1.0f; m.lfo2Wave = LfoWave::Square;
        Engine e = makeEngine(m);
        e.connect(In::OutPan, Out::Lfo2);
        e.noteOn(57, 0.9f);
        const auto o = render(e, 48000);
        double lFirst = 0, rSecond = 0;
        for (size_t i = 1000; i < 23000; ++i) lFirst = std::fmax(lFirst, std::fabs(o.l[i]));
        for (size_t i = 25000; i < 47000; ++i) rSecond = std::fmax(rSecond, std::fabs(o.r[i]));
        CHECK(lFirst == 0.0 && rSecond == 0.0 && energy(o.r, 1000) > 0.0 && energy(o.l, 25000) > 0.0);
    }
    // A global source straight to out.in (LFO 1) stays in the centre, also with Pan at the side.
    {
        ModParams m; m.pan = -1.0f; m.lfoRate = 10.0f;
        Engine e = makeEngine(m);
        e.connect(In::OutIn, Out::Lfo1);
        const auto o = render(e, 4800);
        CHECK(o.l == o.r && energy(o.l, 0) > 0.0);
    }
    // ENSEMBLE: left = right gives equal sides as before; a left-only input stays on the left.
    {
        Ensemble a, b;
        a.prepare(SR); b.prepare(SR);
        a.setParameters(1.0f, 0.6, 1.0f); b.setParameters(1.0f, 0.6, 1.0f);
        double lb = 0, rb = 0;
        bool same = true;
        for (int i = 0; i < 48000; ++i) {
            const float x = static_cast<float>(0.3 * std::sin(2 * 3.141592653589793 * 480.0 * i / SR));
            float l1, r1, l2, r2;
            a.process(x, x, l1, r1);
            b.process(x, 0.0f, l2, r2);
            if (i > 2000) { lb += double(l2) * l2; rb += double(r2) * r2; }
            same = same && std::isfinite(l1) && std::isfinite(r1);
        }
        CHECK(same && lb > 0.0 && rb == 0.0);
    }
    if (failures == 0) std::printf("T-S5.25-pan: all passed\n");
    return failures == 0 ? 0 : 1;
}
