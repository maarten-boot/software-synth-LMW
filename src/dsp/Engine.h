// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 1 engine: one voice, VCO -> VCA driven by ADSR (Spec S5.1).
// Pure C++ (no JUCE) so it can be unit-tested and later served over 9P.
// All methods are RT-safe: no allocation, locks or I/O.
#pragma once
#include "dsp/Adsr.h"
#include "dsp/Oscillator.h"
#include <cmath>

namespace lmw::dsp {

struct VoiceParams {
    Waveform wave = Waveform::Saw;
    float attack = 0.01f, decay = 0.2f, sustain = 0.7f, release = 0.3f; // seconds / level
};

inline double midiNoteToHz(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

class Voice {
public:
    void prepare(double sr) noexcept { osc.setSampleRate(sr); env.setSampleRate(sr); env.reset(); }
    void setParams(const VoiceParams& p) noexcept {
        osc.setWaveform(p.wave);
        env.setParameters(p.attack, p.decay, p.sustain, p.release);
    }
    void noteOn(int n, float vel) noexcept {
        if (!env.isActive()) osc.reset();
        note = n; velocity = vel;
        osc.setFrequency(midiNoteToHz(n));
        env.noteOn();
    }
    void noteOff() noexcept { env.noteOff(); }
    float next() noexcept { return env.isActive() ? osc.next() * env.next() * velocity : 0.0f; }

    bool isActive() const noexcept { return env.isActive(); }
    int getNote() const noexcept { return note; }
    const Adsr& envelope() const noexcept { return env; }

private:
    Oscillator osc;
    Adsr env;
    int note = -1;
    float velocity = 0.0f;
};

class Engine {
public:
    static constexpr float headroom = 0.25f;

    void prepare(double sr) noexcept {
        sampleRate = sr;
        voice.prepare(sr);
        gainStep = static_cast<float>(1.0 / (0.02 * sr)); // 20 ms gain smoothing
    }
    void setParams(const VoiceParams& p) noexcept { voice.setParams(p); }
    void setGain(float g) noexcept { targetGain = g; }

    void noteOn(int note, float velocity) noexcept { voice.noteOn(note, velocity); }
    void noteOff(int note) noexcept { if (voice.getNote() == note) voice.noteOff(); }
    void allNotesOff() noexcept { voice.noteOff(); }

    // Renders `n` samples into left/right (right may be null). Overwrites output.
    void render(float* left, float* right, int n) noexcept {
        for (int i = 0; i < n; ++i) {
            if (gain < targetGain)      gain = std::fmin(gain + gainStep, targetGain);
            else if (gain > targetGain) gain = std::fmax(gain - gainStep, targetGain);
            const float s = voice.next() * gain * headroom;
            left[i] = s;
            if (right) right[i] = s;
        }
    }

    const Voice& getVoice() const noexcept { return voice; }

private:
    Voice voice;
    double sampleRate = 48000.0;
    float gain = 0.0f, targetGain = 1.0f, gainStep = 0.001f;
};

} // namespace lmw::dsp
