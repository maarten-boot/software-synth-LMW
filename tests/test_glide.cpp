// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.24-playing (engine part): glide (S5.27) and the keys.mod output. Glide 0: a new note starts on its pitch;
// Glide 1 s (Always): the new voice's key pitch slides from the last note, half way in octaves at 0.5 s, there at
// 1 s; Legato: only while another key is held; keys.mod carries the mod wheel (a shared source on out.in).
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.4f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static void render(Engine& e, int n) { std::vector<float> l(static_cast<size_t>(n)); e.render(l.data(), nullptr, n); }
static Engine makeEngine(const ModParams& m) {
    Engine e; e.prepare(SR); e.setGain(1.0f);
    VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f;
    e.setParams(p); e.setModParams(m);
    return e;
}
// The key pitch (octaves from C4) of the voice playing `note`.
static float pitchOf(const Engine& e, int note) {
    for (int i = 0; i < 8; ++i) if (e.voice(i).isActive() && e.voice(i).getNote() == note) return e.voice(i).output(Out::KeysPitch);
    return 1e9f;
}

int main() {
    {   // Glide 0: the second note is on its own pitch at once
        Engine e = makeEngine(ModParams{});
        e.noteOn(48, 0.8f); render(e, 480); e.noteOn(60, 0.8f); render(e, 1);
        CHECK(pitchOf(e, 60) == 0.0f);
    }
    {   // Glide 1 s, Always: from C3 (-1 octave) to C4 — half way at 0.5 s, there at 1 s
        ModParams m; m.glide = 1.0f;
        Engine e = makeEngine(m);
        e.noteOn(48, 0.8f); render(e, 480);
        e.noteOff(48);
        e.noteOn(60, 0.8f);
        render(e, 1);
        CHECKV(std::fabs(pitchOf(e, 60) + 1.0f) < 0.001f, pitchOf(e, 60));
        render(e, 24000);
        CHECKV(std::fabs(pitchOf(e, 60) + 0.5f) < 0.002f, pitchOf(e, 60));
        render(e, 24000);
        CHECKV(pitchOf(e, 60) == 0.0f, pitchOf(e, 60));
    }
    {   // Legato: no glide after the key was let go, a glide while it is held
        ModParams m; m.glide = 1.0f; m.glideMode = GlideMode::Legato;
        Engine e = makeEngine(m);
        e.noteOn(48, 0.8f); render(e, 480); e.noteOff(48); render(e, 4800);
        e.noteOn(60, 0.8f); render(e, 1);
        CHECK(pitchOf(e, 60) == 0.0f);                       // detached: starts on its pitch
        e.noteOn(72, 0.8f); render(e, 1);                    // 60 still held: slides up from C4
        CHECKV(pitchOf(e, 72) < 0.01f, pitchOf(e, 72));
    }
    {   // keys.mod = the mod wheel, a shared source: out.in hears it once
        Engine e = makeEngine(ModParams{});
        e.connect(In::OutIn, Out::KeysMod);
        e.setModWheel(0.7f);
        std::vector<float> l(2000);                          // past the 20 ms fade-in of the output gain
        e.render(l.data(), nullptr, 2000);
        CHECKV(std::fabs(l[1999] - 0.7f * 0.25f) < 1e-6f, l[1999]);   // gain 1 x headroom 0.25, below the limiter
        CHECK(typeOf(Out::KeysMod) == SignalType::Cv && isGlobal(Out::KeysMod));
    }
    if (failures == 0) std::printf("T-S6.24-playing (glide, keys.mod): all passed\n");
    return failures == 0 ? 0 : 1;
}
