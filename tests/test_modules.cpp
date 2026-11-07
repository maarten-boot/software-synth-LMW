// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 3 tests (Spec S1.2, S5.2–S5.7, S5a.3, S3.5): VCF, LFO, Noise, Mixer, ENV2, normalled
// patch and cable override.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;
static constexpr double pi = 3.14159265358979323846;

static double rms(const std::vector<float>& x, size_t from = 0) {
    double s = 0; for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(x.size() - from));
}
static int crossings(const std::vector<float>& x) {
    int z = 0; for (size_t i = 1; i < x.size(); ++i) z += (x[i - 1] < 0 && x[i] >= 0) ? 1 : 0; return z;
}
// Steady-state gain of the ladder for a sine of `hz` (rms out / rms in), after 0.5 s settling.
static double ladderGain(double cutoff, float res, double hz) {
    Ladder f; f.setSampleRate(SR); f.setCutoff(cutoff); f.setResonance(res);
    std::vector<float> in(48000), out(48000);
    for (size_t i = 0; i < in.size(); ++i) { in[i] = 0.1f * float(std::sin(2 * pi * hz * double(i) / SR)); out[i] = f.process(in[i]); }
    return rms(out, 24000) / rms(in, 24000);
}
static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static Engine makeEngine(Waveform w = Waveform::Sine) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.wave = w; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
    e.setModParams(ModParams{});
    return e;
}

int main() {
    // T-S5.7-vcf: 24 dB/oct lowpass — passband flat, -3 dB-ish at cutoff, >= 60 dB down 3 octaves up.
    {
        CHECK(std::fabs(20 * std::log10(ladderGain(1000, 0, 100))) < 0.5);
        const double atCut = 20 * std::log10(ladderGain(1000, 0, 1000));
        CHECK(atCut < -6.0 && atCut > -18.0);                 // 4 poles at fc: ~ -12 dB
        CHECK(20 * std::log10(ladderGain(1000, 0, 8000)) < -60.0);
        CHECK(ladderGain(1000, 0.8f, 1000) > 2.0 * ladderGain(1000, 0, 1000));   // resonance peak
        // Top resonance: after one impulse it builds up to a steady oscillation, bounded by tanh.
        Ladder f; f.setSampleRate(SR); f.setCutoff(500); f.setResonance(1.0f);
        f.process(1.0f); float peak = 0, late = 0;
        for (int i = 0; i < 96000; ++i) { const float y = f.process(0.0f); peak = std::fmax(peak, std::fabs(y)); if (i > 90000) late = std::fmax(late, std::fabs(y)); }
        CHECK(std::isfinite(peak) && peak < 2.0f); CHECK(late > 0.1f);
        Ladder d; d.setSampleRate(SR); d.setCutoff(500); d.setResonance(0.8f);   // below threshold: dies out
        d.process(1.0f); float tail = 0;
        for (int i = 0; i < 96000; ++i) { const float y = d.process(0.0f); if (i > 90000) tail = std::fmax(tail, std::fabs(y)); }
        CHECK(tail < 1e-4f);
    }
    // T-S5.1-lfo: rate and range of every shape.
    for (auto w : {LfoWave::Sine, LfoWave::Triangle, LfoWave::Saw, LfoWave::Square}) {
        Lfo l; l.setSampleRate(SR); l.setRate(5.0); l.setWave(w);
        std::vector<float> x(48000); float lo = 1, hi = -1;
        for (auto& v : x) { v = l.next(); lo = std::fmin(lo, v); hi = std::fmax(hi, v); }
        CHECK(hi <= 1.0f && lo >= -1.0f && hi > 0.9f && lo < -0.9f);
        const int z = crossings(x); CHECK(z >= 4 && z <= 6);
    }
    // T-S5.1-noise: uniform ±1 — bounded, mean ~0, rms ~0.577, not constant.
    {
        Noise n; std::vector<float> x(200000); double mean = 0; bool bounded = true;
        for (auto& v : x) { v = n.next(); bounded &= v >= -1.0f && v < 1.0f; mean += v; }
        mean /= double(x.size());
        CHECK(bounded); CHECK(std::fabs(mean) < 0.01); CHECK(std::fabs(rms(x) - 0.5774) < 0.01);
    }
    // T-S1.2a: with no cables the default patch sounds on a note, and is silent before it.
    {
        Engine e = makeEngine(Waveform::Saw);
        CHECK(rms(render(e, 4800)) == 0.0);
        e.noteOn(60, 1.0f);
        CHECK(rms(render(e, 4800)) > 0.05);
        for (int i = 0; i < numInputs; ++i) CHECK(!e.getPatch().isPatched(static_cast<In>(i)));
        CHECK(e.getPatch().source(In::VcfIn) == Out::Mix && e.getPatch().source(In::VcaCv) == Out::Env1);
    }
    // T-S1.2b: a cable replaces (not adds to) the normalled source; removing it restores it.
    {
        Engine e = makeEngine(Waveform::Sine);
        e.noteOn(45, 1.0f);                                    // 110 Hz sine
        render(e, 4800);
        int z = crossings(render(e, 48000)); CHECK(z >= 108 && z <= 112);
        e.connect(In::VcfIn, Out::Noise);                      // vcf.in <- noise (mix.noise is 0!)
        render(e, 4800);
        z = crossings(render(e, 48000)); CHECK(z > 3000);      // noise only: the VCO is gone
        e.disconnect(In::VcfIn);
        render(e, 4800);
        z = crossings(render(e, 48000)); CHECK(z >= 108 && z <= 112);
        // Via the command used by the lock-free queue.
        e.apply(PatchCommand{In::MixIn1, static_cast<std::int8_t>(Out::Noise)});
        CHECK(e.getPatch().source(In::MixIn1) == Out::Noise);
        e.apply(PatchCommand{In::MixIn1, Patch::none});
        CHECK(e.getPatch().source(In::MixIn1) == Out::Vco);
    }
    // Mixer levels act on patched and normalled signals; both at 0 = silence.
    {
        Engine e = makeEngine(Waveform::Saw);
        ModParams m; m.mixVco = 0.0f; m.mixNoise = 0.0f; e.setModParams(m);
        e.noteOn(60, 1.0f); render(e, 480);
        CHECK(rms(render(e, 4800)) < 1e-6);
        m.mixNoise = 1.0f; e.setModParams(m);                  // noise on (normalled mix.in2)
        render(e, 480);
        CHECK(crossings(render(e, 48000)) > 3000);
    }
    // S5.6: out.in <- vcf.out bypasses VCA/ENV1 — the voice keeps sounding after release (drone).
    {
        Engine e = makeEngine(Waveform::Saw);
        e.connect(In::OutIn, Out::Vcf);
        e.noteOn(60, 1.0f); render(e, 4800); e.noteOff(60);
        render(e, 48000);                                      // release (10 ms) long over
        CHECK(e.activeVoices() == 0);
        CHECK(rms(render(e, 4800)) > 0.05);
        e.disconnect(In::OutIn);
        CHECK(rms(render(e, 4800)) == 0.0);
        // A global source on out.in is output once, even with no notes: LFO at 5 Hz.
        e.connect(In::OutIn, Out::Lfo);
        const int z = crossings(render(e, 48000)); CHECK(z >= 4 && z <= 6);
    }
    // Feedback cable (vco.fm <- vca.out) reads the previous sample and stays finite/bounded.
    {
        Engine e = makeEngine(Waveform::Saw);
        ModParams m; m.fmSemitones = 12.0f; e.setModParams(m);
        e.connect(In::VcoFm, Out::Vca);
        e.noteOn(60, 1.0f);
        const auto x = render(e, 48000);
        bool ok = true; for (float v : x) ok &= std::isfinite(v) && std::fabs(v) < 1.0f; CHECK(ok);
        CHECK(rms(x) > 0.01);
    }
    // T-S3.5: mod wheel scales the normalled LFO into vco.fm (vibrato); 0 = steady pitch.
    {
        auto periodSpread = [](Engine& e) {                    // max - min distance between rising zero crossings
            const auto x = render(e, 48000); int last = -1, lo = 1 << 30, hi = 0;
            for (size_t i = 1; i < x.size(); ++i) if (x[i - 1] < 0 && x[i] >= 0) {
                if (last >= 0) { lo = std::min(lo, int(i) - last); hi = std::max(hi, int(i) - last); } last = int(i); }
            return hi - lo;
        };
        Engine e = makeEngine(Waveform::Sine);
        ModParams m; m.fmSemitones = 2.0f; m.lfoRate = 5.0f; e.setModParams(m);
        e.noteOn(69, 1.0f); render(e, 480);
        CHECK(periodSpread(e) <= 1);                           // wheel at 0: no vibrato
        e.setModWheel(1.0f);
        CHECK(periodSpread(e) >= 10);                          // ±2 st on 109 samples/period ≈ ±13
    }
    // S5.2: ENV2 -> VCF cutoff — with vcf.env +5 oct the attack is brighter than the sustain.
    {
        Engine e = makeEngine(Waveform::Saw);
        VoiceParams p; p.wave = Waveform::Saw; p.attack = 0; p.decay = 0; p.sustain = 1; p.release = 0.01f;
        p.env2Attack = 0.0f; p.env2Decay = 0.2f; p.env2Sustain = 0.0f; e.setParams(p);
        ModParams m; m.cutoff = 150.0f; m.envOctaves = 5.0f; e.setModParams(m);
        e.prepare(SR);                                         // cutoff smoothing starts at 150 Hz
        e.setModParams(m); e.setParams(p); e.setGain(1.0f);
        e.noteOn(48, 1.0f);
        render(e, 960);                                        // skip filter start-up
        const auto early = render(e, 960);                     // 20-40 ms: ENV2 ~0.85, cutoff ~ 2.8 kHz
        render(e, 19200);
        const auto late = render(e, 960);                      // ENV2 at 0: cutoff 150 Hz
        auto hf = [](const std::vector<float>& x) { double s = 0; for (size_t i = 1; i < x.size(); ++i) s += std::fabs(x[i] - x[i - 1]); return s; };
        CHECK(hf(early) > 3.0 * hf(late));
    }
    // T-S5a.3: a sustain change on a held note glides (max step 1/(20 ms)), no jump.
    {
        Adsr a; a.setSampleRate(SR); a.setParameters(0, 0, 0.8f, 0.1f); a.noteOn();
        for (int i = 0; i < 10; ++i) a.next();
        CHECK(a.getStage() == Adsr::Stage::Sustain);
        a.setParameters(0, 0, 0.2f, 0.1f);
        float prev = a.getLevel(), maxStep = 0;
        for (int i = 0; i < 2000; ++i) { const float v = a.next(); maxStep = std::fmax(maxStep, std::fabs(v - prev)); prev = v; }
        CHECK(maxStep <= 1.0f / 960.0f + 1e-6f);
        CHECK(std::fabs(a.getLevel() - 0.2f) < 1e-4f);
    }
    // Defaults (filter open, no noise, no vibrato) stay close to the Phase-2 voice: VCO * ENV1.
    {
        Engine e = makeEngine(Waveform::Sine);
        e.noteOn(57, 1.0f); render(e, 4800);
        const double r = rms(render(e, 48000));
        CHECK(std::fabs(r - 0.7071 * Engine::headroom) < 0.01); // 220 Hz sine through open VCF
    }
    // Port names are unique (they become 9P file names).
    for (int i = 0; i < numInputs; ++i) for (int j = i + 1; j < numInputs; ++j) CHECK(inputNames[size_t(i)] != inputNames[size_t(j)]);
    for (int i = 0; i < numOutputs; ++i) for (int j = i + 1; j < numOutputs; ++j) CHECK(outputNames[size_t(i)] != outputNames[size_t(j)]);

    std::printf(failures ? "%d failure(s)\n" : "all module tests passed\n", failures);
    return failures ? 1 : 0;
}
