// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 1 engine tests (Spec S5.1): oscillator pitch, ADSR timing, voice/engine output.
#include "dsp/Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static constexpr double SR = 48000.0;

static int risingZeroCrossings(Waveform w, double hz, int samples) {
    Oscillator o; o.setSampleRate(SR); o.setFrequency(hz); o.setWaveform(w);
    int count = 0; float prev = o.next();
    for (int i = 1; i < samples; ++i) { float v = o.next(); if (prev < 0 && v >= 0) ++count; prev = v; }
    return count;
}

int main() {
    // T-S5.1-osc: pitch of every waveform = 440 Hz (+-1 cycle over 1 s), output bounded.
    for (auto w : {Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle}) {
        const int z = risingZeroCrossings(w, 440.0, static_cast<int>(SR));
        CHECK(z >= 439 && z <= 441);
        Oscillator o; o.setSampleRate(SR); o.setFrequency(3000.0); o.setWaveform(w);
        float peak = 0; for (int i = 0; i < 48000; ++i) peak = std::fmax(peak, std::fabs(o.next()));
        CHECK(peak <= 1.2f && peak > 0.5f);
    }
    CHECK(std::fabs(midiNoteToHz(69) - 440.0) < 1e-9);
    CHECK(std::fabs(midiNoteToHz(57) - 220.0) < 1e-9);

    // T-S5.1-adsr: attack 10 ms reaches 1.0 at ~480 samples, decays to sustain, releases to 0.
    {
        Adsr e; e.setSampleRate(SR); e.setParameters(0.01f, 0.02f, 0.5f, 0.05f);
        CHECK(!e.isActive());
        e.noteOn();
        int i = 0; while (e.getStage() == Adsr::Stage::Attack && i < 10000) { e.next(); ++i; }
        CHECK(std::abs(i - 480) <= 2);
        while (e.getStage() == Adsr::Stage::Decay && i < 100000) { e.next(); ++i; }
        CHECK(e.getStage() == Adsr::Stage::Sustain && std::fabs(e.getLevel() - 0.5f) < 1e-4f);
        e.noteOff();
        int r = 0; while (e.isActive() && r < 100000) { e.next(); ++r; }
        CHECK(std::abs(r - 2400) <= 2);   // 50 ms
        CHECK(e.getLevel() == 0.0f);
    }
    // Zero times do not hang or produce NaN.
    {
        Adsr e; e.setSampleRate(SR); e.setParameters(0, 0, 1, 0);
        e.noteOn(); CHECK(e.next() == 1.0f);
        e.noteOff(); e.next(); CHECK(!e.isActive());
    }

    // T-S5.1-engine: silence before note, sound during, silence after release.
    {
        Engine eng; eng.prepare(SR);
        VoiceParams p; p.attack = 0.005f; p.decay = 0.01f; p.sustain = 0.8f; p.release = 0.02f;
        eng.setParams(p); eng.setGain(1.0f);
        std::vector<float> l(4800), r(4800);
        eng.render(l.data(), r.data(), 4800);
        bool silent = true; for (float v : l) silent &= v == 0.0f; CHECK(silent);

        eng.noteOn(60, 1.0f);
        eng.render(l.data(), r.data(), 4800);
        float peak = 0; bool finite = true, same = true;
        for (int i = 0; i < 4800; ++i) { peak = std::fmax(peak, std::fabs(l[i])); finite &= std::isfinite(l[i]); same &= l[i] == r[i]; }
        CHECK(peak > 0.05f && peak <= Engine::headroom * 1.2f);
        CHECK(finite && same);

        eng.noteOff(61);                                   // wrong note: keeps sounding
        CHECK(eng.voice(0).isActive());
        eng.noteOff(60);
        eng.render(l.data(), r.data(), 4800);              // 100 ms > 20 ms release
        CHECK(!eng.voice(0).isActive());
        CHECK(l.back() == 0.0f);
        eng.render(l.data(), nullptr, 4800);               // mono output path
    }

    // T-S4.1: voice count N allows exactly N simultaneous notes.
    for (int n : {1, 8, 32}) {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(n);
        for (int k = 0; k < n; ++k) eng.noteOn(40 + k, 1.0f);
        std::vector<float> l(64); eng.render(l.data(), nullptr, 64);
        CHECK(eng.activeVoices() == n);
        eng.noteOn(100, 1.0f);                         // N+1
        CHECK(eng.activeVoices() == n);
    }
    // Range clamps to 1..32.
    { Engine e; e.setVoiceCount(0); CHECK(e.getVoiceCount() == 1); e.setVoiceCount(99); CHECK(e.getVoiceCount() == 32); }

    // T-S4.2: N+1 notes steal the oldest held voice.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(3);
        eng.noteOn(60, 1); eng.noteOn(62, 1); eng.noteOn(64, 1);
        eng.noteOn(65, 1);
        bool has60 = false, has65 = false;
        for (int i = 0; i < 3; ++i) { has60 |= eng.voice(i).getNote() == 60; has65 |= eng.voice(i).getNote() == 65; }
        CHECK(!has60 && has65);
    }
    // ... but a released voice is stolen before an older held one.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(3);
        VoiceParams p; p.release = 2.0f; eng.setParams(p);
        eng.noteOn(60, 1); eng.noteOn(62, 1); eng.noteOn(64, 1);
        eng.noteOff(62);                               // 62 rings out
        eng.noteOn(67, 1);
        bool has60 = false, has62 = false;
        for (int i = 0; i < 3; ++i) { has60 |= eng.voice(i).getNote() == 60; has62 |= eng.voice(i).getNote() == 62; }
        CHECK(has60 && !has62);
    }
    // Same note retriggers its own voice instead of doubling.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(4);
        eng.noteOn(60, 1); eng.noteOn(60, 1);
        CHECK(eng.activeVoices() == 1);
    }
    // Stealing is click-free: no sample-to-sample jump larger than the waveform allows.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(1); eng.setGain(1.0f);
        VoiceParams p; p.wave = Waveform::Sine; p.attack = 0.001f; eng.setParams(p);
        std::vector<float> l(4800);
        eng.noteOn(60, 1); eng.render(l.data(), nullptr, 4800);
        const float before = l.back();
        eng.noteOn(72, 1); eng.render(l.data(), nullptr, 1);
        CHECK(std::fabs(l[0] - before) < 0.02f);
    }
    // Reducing the voice count releases the excess voices.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(8);
        VoiceParams p; p.release = 0.01f; eng.setParams(p);
        for (int k = 0; k < 8; ++k) eng.noteOn(50 + k, 1);
        eng.setVoiceCount(2);
        std::vector<float> l(4800); eng.render(l.data(), nullptr, 4800);
        CHECK(eng.activeVoices() == 2);
    }
    // 32 voices at full velocity and gain stay finite.
    {
        Engine eng; eng.prepare(SR); eng.setVoiceCount(32); eng.setGain(1.0f);
        for (int k = 0; k < 32; ++k) eng.noteOn(30 + k, 1);
        std::vector<float> l(4800); eng.render(l.data(), nullptr, 4800);
        bool finite = true; for (float v : l) finite &= std::isfinite(v); CHECK(finite);
    }

    std::printf(failures ? "%d failure(s)\n" : "all engine tests passed\n", failures);
    return failures ? 1 : 0;
}
