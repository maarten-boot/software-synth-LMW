// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.10: VCO 1-3 per voice (Octave, Tune, FM, Wave) and the 4-input mixer.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.3f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static std::vector<float> render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); return l; }
static int crossings(const std::vector<float>& x) {
    int z = 0; for (size_t i = 1; i < x.size(); ++i) z += (x[i - 1] < 0 && x[i] >= 0) ? 1 : 0; return z;
}
static Engine makeEngine(const ModParams& m) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.waves = {Waveform::Sine, Waveform::Sine, Waveform::Sine};
    p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p);
    e.setModParams(m);
    return e;
}
// Frequency (Hz, by zero crossings over one second) of note 69 (A4) with only VCO `n` audible.
static double vcoFrequency(int n, int octave, float tune) {
    ModParams m;
    m.mixVco = {0.0f, 0.0f, 0.0f};
    m.mixVco[static_cast<size_t>(n)] = 1.0f;
    m.octave[static_cast<size_t>(n)] = octave;
    m.tune[static_cast<size_t>(n)] = tune;
    Engine e = makeEngine(m);
    e.noteOn(69, 1.0f);
    render(e, 4800);
    return crossings(render(e, 48000));
}

int main() {
    // T-S5.10a: Octave and Tune on each VCO.
    for (int n = 0; n < numVcos; ++n) {
        CHECKV(std::fabs(vcoFrequency(n, 0, 0.0f) - 440.0) <= 2, vcoFrequency(n, 0, 0.0f));
        CHECKV(std::fabs(vcoFrequency(n, 2, 0.0f) - 1760.0) <= 2, vcoFrequency(n, 2, 0.0f));
        CHECKV(std::fabs(vcoFrequency(n, -2, 0.0f) - 110.0) <= 2, vcoFrequency(n, -2, 0.0f));
        CHECKV(std::fabs(vcoFrequency(n, 0, 7.0f) - 659.26) <= 2, vcoFrequency(n, 0, 7.0f));     // a fifth up
        CHECKV(std::fabs(vcoFrequency(n, 0, -7.0f) - 293.66) <= 2, vcoFrequency(n, 0, -7.0f));   // a fifth down
    }
    // VCO 2 and 3 at level 0 change nothing, whatever their settings (and are not computed).
    {
        ModParams a, b;
        b.octave = {0, 2, -1}; b.tune = {0.0f, 3.3f, -5.0f}; b.fmSemitones = {0.0f, 5.0f, 9.0f};
        Engine ea = makeEngine(a), eb = makeEngine(b);
        ea.noteOn(60, 0.9f); eb.noteOn(60, 0.9f);
        CHECK(render(ea, 9600) == render(eb, 9600));
    }
    // Mixer: VCO 2 at full level adds a second tone (two frequencies -> beating, larger peak).
    {
        ModParams m; m.mixVco = {0.5f, 0.5f, 0.0f}; m.octave[1] = 1;
        Engine e = makeEngine(m);
        e.noteOn(57, 1.0f);
        render(e, 4800);
        const auto x = render(e, 48000);
        // Sum of 220 Hz and 440 Hz sines: positive zero crossings at 220 Hz (the 440 Hz part does
        // not add crossings at equal levels), clearly more energy than one sine at level 0.5.
        double s2 = 0; for (float v : x) s2 += double(v) * v;
        ModParams one; one.mixVco = {0.5f, 0.0f, 0.0f};
        Engine e1 = makeEngine(one); e1.noteOn(57, 1.0f); render(e1, 4800);
        const auto y = render(e1, 48000);
        double s1 = 0; for (float v : y) s1 += double(v) * v;
        CHECKV(s2 > 1.8 * s1, s2 / s1);
    }
    // A VCO at mixer level 0 is still computed when its output is patched elsewhere:
    // vco2.out -> out.in plays VCO 2 directly.
    {
        ModParams m; m.mixVco = {1.0f, 0.0f, 0.0f}; m.octave[1] = 1;
        Engine e = makeEngine(m);
        e.connect(In::OutIn, Out::Vco2);
        e.noteOn(69, 1.0f);
        render(e, 4800);
        CHECKV(std::abs(crossings(render(e, 48000)) - 880) <= 2, crossings(render(e, 48000)));
    }
    // vco3.pitch takes its own source: ENV 2 (at 1.0) -> vco3.pitch = C5 whatever the key.
    {
        ModParams m; m.mixVco = {0.0f, 0.0f, 1.0f};
        Engine e = makeEngine(m);
        VoiceParams p; p.waves = {Waveform::Sine, Waveform::Sine, Waveform::Sine};
        p.attack = 0; p.decay = 0; p.sustain = 1; p.env2Attack = 0; p.env2Decay = 0; p.env2Sustain = 1; e.setParams(p);
        e.connect(In::Vco3Pitch, Out::Env2);
        e.noteOn(40, 1.0f);
        render(e, 4800);
        CHECKV(std::abs(crossings(render(e, 48000)) - 523) <= 2, crossings(render(e, 48000)));
    }
    if (failures == 0) std::printf("T-S5.10-vcos: all passed\n");
    return failures == 0 ? 0 : 1;
}
