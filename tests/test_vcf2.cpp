// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.11: VCF 2 in series after VCF 1 — exact bypass when open, click-free engage, series filtering.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.4f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static double rms(const std::vector<float>& x) { double s = 0; for (float v : x) s += double(v) * v; return std::sqrt(s / double(x.size())); }
static Engine makeEngine(Waveform w, const ModParams& m) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves = {w, w, w}; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p);
    e.setModParams(m);
    return e;
}

int main() {
    // T-S5.11a: open VCF 2 (default) is an exact bypass: same samples as skipping it with a cable.
    {
        ModParams m; m.cutoff = 3000.0f; m.resonance = 0.3f; m.envOctaves = 1.0f;
        Engine a = makeEngine(Waveform::Saw, m), b = makeEngine(Waveform::Saw, m);
        b.connect(In::VcaIn, Out::Vcf1);                 // VCF 1 straight to the VCA
        a.noteOn(48, 0.8f); b.noteOn(48, 0.8f); a.noteOn(55, 0.6f); b.noteOn(55, 0.6f);
        CHECK(render(a, 24000) == render(b, 24000));
        // Positive Env modulation on VCF 2 only pushes its cutoff above the maximum: still open.
        ModParams m2 = m; m2.vcf2EnvOctaves = 3.0f;
        a.setModParams(m2); b.setModParams(m2);
        CHECK(render(a, 9600) == render(b, 9600));
    }
    // T-S5.11a: lowering VCF 2's cutoff on a sounding note crossfades it in: no jump, then filtered.
    {
        ModParams m;
        Engine e = makeEngine(Waveform::Sine, m);
        e.noteOn(69, 1.0f);                              // 440 Hz sine
        render(e, 4800);                                 // past the engine's 20 ms gain fade-in
        const auto before = render(e, 4800);
        float prev = before.back(), maxStep = 0;
        m.vcf2Cutoff = 100.0f;                           // 2+ octaves below: strongly damped
        e.setModParams(m);
        const auto after = render(e, 9600);
        for (float v : after) { maxStep = std::fmax(maxStep, std::fabs(v - prev)); prev = v; }
        float natural = 0;
        for (size_t i = 1; i < before.size(); ++i) natural = std::fmax(natural, std::fabs(before[i] - before[i - 1]));
        CHECKV(maxStep <= 1.05f * natural, maxStep / natural);   // never faster than the sine itself
        const auto settled = render(e, 4800);
        CHECKV(rms(settled) < 0.1 * rms(before), rms(settled) / rms(before));
        // and back to open: once the 20 ms cutoff glide and fade are over, the full level returns
        m.vcf2Cutoff = 18000.0f; e.setModParams(m);
        render(e, 4800);
        CHECKV(std::fabs(rms(render(e, 4800)) / rms(before) - 1.0) < 1e-3, rms(render(e, 4800)) / rms(before));
    }
    // T-S5.11b: in series, VCF 1 highpass + VCF 2 lowpass at 1 kHz make a bandpass.
    {
        auto level = [](int note) {
            ModParams m;
            m.cutoff = 1000.0f; m.vcfType = FilterType::Highpass;
            m.vcf2Cutoff = 1000.0f; m.vcf2Type = FilterType::Lowpass;
            Engine e = makeEngine(Waveform::Sine, m);
            e.noteOn(note, 1.0f);
            render(e, 9600);
            return 20 * std::log10(rms(render(e, 24000)));
        };
        const double low = level(43), mid = level(83), high = level(119);   // ~98 Hz, ~988 Hz, ~7.9 kHz
        CHECKV(mid - low > 20.0, mid - low);
        CHECKV(mid - high > 20.0, mid - high);
    }
    if (failures == 0) std::printf("T-S5.11-vcf2: all passed\n");
    return failures == 0 ? 0 : 1;
}
