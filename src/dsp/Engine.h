// SPDX-License-Identifier: AGPL-3.0-or-later
// Polyphonic engine: per voice VCO -> VCA driven by ADSR (Spec S4, S5.1).
// Pure C++ (no JUCE) so it can be unit-tested and later served over 9P.
// All methods are RT-safe: no allocation, locks or I/O.
#pragma once
#include "dsp/Adsr.h"
#include "dsp/Oscillator.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace lmw::dsp {

struct VoiceParams {
    Waveform wave = Waveform::Saw;
    float attack = 0.01f, decay = 0.2f, sustain = 0.7f, release = 0.3f; // seconds / level
};

inline double midiNoteToHz(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

class Voice {
public:
    void prepare(double sr) noexcept { osc.setSampleRate(sr); env.setSampleRate(sr); env.reset(); gate = false; }
    void setParams(const VoiceParams& p) noexcept {
        osc.setWaveform(p.wave);
        env.setParameters(p.attack, p.decay, p.sustain, p.release);
    }
    // Retriggering an active voice keeps oscillator phase and envelope level
    // continuous, so stealing a voice does not click.
    void noteOn(int n, float vel) noexcept {
        if (!env.isActive()) osc.reset();
        note = n; velocity = vel; gate = true;
        osc.setFrequency(midiNoteToHz(n));
        env.noteOn();
    }
    void noteOff() noexcept { gate = false; env.noteOff(); }
    float next() noexcept { return env.isActive() ? osc.next() * env.next() * velocity : 0.0f; }

    bool isActive() const noexcept { return env.isActive(); }
    bool isHeld() const noexcept { return gate && env.isActive(); }   // key still down
    int getNote() const noexcept { return note; }
    const Adsr& envelope() const noexcept { return env; }

private:
    Oscillator osc;
    Adsr env;
    int note = -1;
    float velocity = 0.0f;
    bool gate = false;
};

// Polyphonic engine (Spec S4). Voice count 1..maxVoices, changeable at runtime.
// Allocation: same note already sounding -> reuse that voice; else a free voice;
// else steal the oldest voice, preferring voices already in release.
class Engine {
public:
    static constexpr int maxVoices = 32;
    static constexpr float headroom = 0.25f;

    void prepare(double sr) noexcept {
        for (auto& v : voices) v.prepare(sr);
        order.fill(0);
        counter = 0;
        gainStep = static_cast<float>(1.0 / (0.02 * sr)); // 20 ms gain smoothing
    }
    void setParams(const VoiceParams& p) noexcept { for (auto& v : voices) v.setParams(p); }
    void setGain(float g) noexcept { targetGain = g; }

    void setVoiceCount(int n) noexcept {
        n = n < 1 ? 1 : (n > maxVoices ? maxVoices : n);
        for (int i = n; i < voiceCount; ++i) voices[static_cast<std::size_t>(i)].noteOff(); // let them ring out
        voiceCount = n;
    }
    int getVoiceCount() const noexcept { return voiceCount; }

    void noteOn(int note, float velocity) noexcept {
        const int i = allocate(note);
        voices[static_cast<std::size_t>(i)].noteOn(note, velocity);
        order[static_cast<std::size_t>(i)] = ++counter;
    }
    void noteOff(int note) noexcept {
        for (auto& v : voices) if (v.isHeld() && v.getNote() == note) v.noteOff();
    }
    void allNotesOff() noexcept { for (auto& v : voices) v.noteOff(); }

    // Renders `n` samples into left/right (right may be null). Overwrites output.
    void render(float* left, float* right, int n) noexcept {
        for (int i = 0; i < n; ++i) {
            if (gain < targetGain)      gain = std::fmin(gain + gainStep, targetGain);
            else if (gain > targetGain) gain = std::fmax(gain - gainStep, targetGain);
            float sum = 0.0f;
            for (auto& v : voices) sum += v.next();   // includes voices ringing out after a count reduction
            const float s = sum * gain * headroom;
            left[i] = s;
            if (right) right[i] = s;
        }
    }

    int activeVoices() const noexcept {
        int n = 0; for (const auto& v : voices) n += v.isActive() ? 1 : 0; return n;
    }
    const Voice& voice(int i) const noexcept { return voices[static_cast<std::size_t>(i)]; }

private:
    int allocate(int note) const noexcept {
        for (int i = 0; i < voiceCount; ++i)                       // 1. same note
            if (voices[static_cast<std::size_t>(i)].isActive() && voices[static_cast<std::size_t>(i)].getNote() == note) return i;
        for (int i = 0; i < voiceCount; ++i)                       // 2. free voice
            if (!voices[static_cast<std::size_t>(i)].isActive()) return i;
        int best = -1;                                              // 3. oldest released
        for (int i = 0; i < voiceCount; ++i)
            if (!voices[static_cast<std::size_t>(i)].isHeld() && (best < 0 || order[static_cast<std::size_t>(i)] < order[static_cast<std::size_t>(best)])) best = i;
        if (best >= 0) return best;
        best = 0;                                                   // 4. oldest held
        for (int i = 1; i < voiceCount; ++i)
            if (order[static_cast<std::size_t>(i)] < order[static_cast<std::size_t>(best)]) best = i;
        return best;
    }

    std::array<Voice, maxVoices> voices{};
    std::array<std::uint64_t, maxVoices> order{};
    std::uint64_t counter = 0;
    int voiceCount = 8;
    float gain = 0.0f, targetGain = 1.0f, gainStep = 0.001f;
};

} // namespace lmw::dsp
