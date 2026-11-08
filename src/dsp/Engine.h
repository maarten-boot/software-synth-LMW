// SPDX-License-Identifier: AGPL-3.0-or-later
// Polyphonic semi-modular engine (Spec S4, S5). Per voice: VCO, ENV1, ENV2, Mixer, VCF, VCA;
// shared: LFO, Noise. Every module input reads its patch cable if present, else its normalled
// source (Spec S5.3, S5.4). Pure C++ (no JUCE) so it can be unit-tested and served over 9P.
// All methods are RT-safe: no allocation, locks or I/O.
#pragma once
#include "dsp/Adsr.h"
#include "dsp/Ladder.h"
#include "dsp/Lfo.h"
#include "dsp/Noise.h"
#include "dsp/Oscillator.h"
#include "dsp/Patch.h"
#include "dsp/SoftLimiter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace lmw::dsp {

// Per-voice parameters (Spec S5a.1). Field names without prefix belong to ENV1.
struct VoiceParams {
    Waveform wave = Waveform::Saw;
    float attack = 0.01f, decay = 0.2f, sustain = 0.7f, release = 0.3f;              // ENV1 -> VCA
    float env2Attack = 0.01f, env2Decay = 0.3f, env2Sustain = 0.0f, env2Release = 0.3f; // ENV2 -> VCF
};

// Module parameters shared by all voices (Spec S5a.1).
struct ModParams {
    float fmSemitones = 0.0f;                   // vco.fm (base depth; the mod wheel adds up to +1 st)
    float mixVco = 1.0f, mixNoise = 0.0f;       // mix.vco, mix.noise
    float cutoff = 18000.0f;                    // vcf.cutoff (Hz)
    float resonance = 0.0f;                     // vcf.res
    float envOctaves = 0.0f;                    // vcf.env
    float vcaInitial = 0.0f;                    // vca.initial (base gain added to vca.cv)
    float lfoRate = 5.0f;                       // lfo.rate (Hz)
    LfoWave lfoWave = LfoWave::Sine;            // lfo.wave
};

inline double midiNoteToHz(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

// Spec S5.8: keys.pitch 0 = C4 (MIDI 60), 1.0 per octave; a gate is "on" above 0.5.
inline constexpr int keysPitchZeroNote = 60;
inline constexpr float gateThreshold = 0.5f;

// Mod wheel at full adds this much FM depth (Spec S3.5).
inline constexpr float modWheelFmSemitones = 1.0f;

// Values of the shared modules for the current sample.
struct GlobalSignals {
    float lfo = 0.0f, noise = 0.0f, modWheel = 0.0f;
};

class Voice {
public:
    void prepare(double sr) noexcept {
        osc.setSampleRate(sr); env1.setSampleRate(sr); env2.setSampleRate(sr); vcf.setSampleRate(sr);
        env1.reset(); env2.reset(); vcf.reset(); out.fill(0.0f);
        gate = keyDown = false; played = false; lastPitch = 1e9;
        env1GateWasOn = env2GateWasOn = false; env1KeyGated = env2KeyGated = true;
    }
    void setParams(const VoiceParams& p) noexcept {
        osc.setWaveform(p.wave);
        env1.setParameters(p.attack, p.decay, p.sustain, p.release);
        env2.setParameters(p.env2Attack, p.env2Decay, p.env2Sustain, p.env2Release);
    }
    void setResonance(float r) noexcept { vcf.setResonance(r); }

    // Retriggering an active voice keeps oscillator phase, filter state and envelope level
    // continuous, so stealing a voice does not click. An envelope whose gate input is keys.gate
    // (normalled or by cable) follows the key events directly, so a repeated note retriggers it;
    // an envelope gated by another signal follows that signal's edges in process() (Spec S5.8).
    void noteOn(int n, float vel, const Patch& patch) noexcept {
        if (!env1.isActive()) { osc.reset(); vcf.reset(); }
        note = n; velocity = vel; gate = true; keyDown = true; played = true;
        lastPitch = 1e9;                          // force frequency update
        if (keyGated(In::Env1Gate, patch)) env1.noteOn();
        if (keyGated(In::Env2Gate, patch)) env2.noteOn();
    }
    void noteOff(const Patch& patch) noexcept {
        gate = false; keyDown = false;
        if (keyGated(In::Env1Gate, patch)) env1.noteOff();
        if (keyGated(In::Env2Gate, patch)) env2.noteOff();
    }
    // Key released: with the sustain pedal down the voice keeps sounding (gate stays open).
    void releaseKey(bool sustainPedal, const Patch& patch) noexcept { keyDown = false; if (!sustainPedal) noteOff(patch); }
    void setPitchBend(double semitones) noexcept { bend = semitones; }

    // One sample through the voice's modules, in order KEYS, ENV1, ENV2, VCO, MIX, VCF, VCA.
    // Returns this voice's contribution to out.in (only meaningful for per-voice sources).
    float process(const GlobalSignals& g, const Patch& patch, const ModParams& mp,
                  double cutoffLog2) noexcept {
        const double keySemitones = static_cast<double>(note - keysPitchZeroNote) + bend;   // Spec S5.8
        out[o(Out::KeysPitch)] = static_cast<float>(keySemitones / 12.0);
        out[o(Out::KeysGate)] = gate ? 1.0f : 0.0f;
        out[o(Out::KeysVel)] = velocity;

        followGate(env1, In::Env1Gate, env1GateWasOn, env1KeyGated, g, patch);
        followGate(env2, In::Env2Gate, env2GateWasOn, env2KeyGated, g, patch);
        out[o(Out::Env1)] = env1.next();
        out[o(Out::Env2)] = env2.next();

        // vco.pitch: from the keys it is used in full precision; any other source is 1.0 per octave.
        const double semitones = patch.source(In::VcoPitch) == Out::KeysPitch
            ? keySemitones : 12.0 * static_cast<double>(read(In::VcoPitch, g, patch));
        const float fmDepth = mp.fmSemitones + g.modWheel * modWheelFmSemitones;   // Spec S3.5
        const double pitch = semitones + static_cast<double>(read(In::VcoFm, g, patch) * fmDepth);
        if (pitch < lastPitch || pitch > lastPitch) { lastPitch = pitch;   // changed (exact cache check)
            osc.setFrequency(midiNoteToHz(keysPitchZeroNote) * std::exp2(pitch / 12.0)); }
        out[o(Out::Vco)] = osc.next();

        out[o(Out::Mix)] = read(In::MixIn1, g, patch) * mp.mixVco + read(In::MixIn2, g, patch) * mp.mixNoise;

        vcf.setCutoff(std::exp2(cutoffLog2 + static_cast<double>(read(In::VcfCutoff, g, patch) * mp.envOctaves)));
        out[o(Out::Vcf)] = vcf.process(read(In::VcfIn, g, patch));

        const float vcaGain = std::clamp(mp.vcaInitial + read(In::VcaCv, g, patch), 0.0f, 1.0f);   // Spec S5.5
        out[o(Out::Vca)] = read(In::VcaIn, g, patch) * vcaGain * velocity;

        return read(In::OutIn, g, patch);
    }

    bool isActive() const noexcept { return env1.isActive(); }
    bool isHeld() const noexcept { return gate && env1.isActive(); }   // key down or sustained by pedal
    bool isKeyDown() const noexcept { return keyDown && env1.isActive(); }
    bool hasPlayed() const noexcept { return played; }
    int getNote() const noexcept { return note; }
    const Adsr& envelope() const noexcept { return env1; }
    float output(Out which) const noexcept { return out[o(which)]; }

private:
    static constexpr std::size_t o(Out x) noexcept { return static_cast<std::size_t>(x); }
    static bool keyGated(In gateIn, const Patch& patch) noexcept { return patch.source(gateIn) == Out::KeysGate; }

    // Spec S5.8: an envelope gated by a signal other than keys.gate starts on a rising edge
    // (crossing 0.5 upward) and releases on a falling one. When the gate goes back to the keys, an
    // envelope left open while the key is up is released.
    void followGate(Adsr& env, In gateIn, bool& wasOn, bool& wasKeyGated, const GlobalSignals& g,
                    const Patch& patch) noexcept {
        const bool on = read(gateIn, g, patch) > gateThreshold;
        const bool keys = keyGated(gateIn, patch);
        if (!keys) {
            if (on && !wasOn) env.noteOn();
            else if (!on && wasOn) env.noteOff();
        } else if (!wasKeyGated && !gate && env.isActive()) {
            env.noteOff();
        }
        wasOn = on;
        wasKeyGated = keys;
    }

    float read(In in, const GlobalSignals& g, const Patch& patch) const noexcept {
        const Out src = patch.source(in);
        if (src == Out::Lfo)   return g.lfo;
        if (src == Out::Noise) return g.noise;
        return out[o(src)];                    // computed earlier this sample, or previous sample (feedback)
    }

    Oscillator osc;
    Adsr env1, env2;
    Ladder vcf;
    std::array<float, numOutputs> out{};
    int note = 60;
    float velocity = 0.0f;
    double bend = 0.0, lastPitch = 1e9;        // semitones
    bool gate = false;                         // ENV1 open (key down or sustained)
    bool keyDown = false;                      // physical key still down
    bool played = false;                       // has had a note since prepare()
    bool env1GateWasOn = false, env2GateWasOn = false;     // gate level last sample (edge detection)
    bool env1KeyGated = true, env2KeyGated = true;         // gate source was keys.gate last sample
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
        lfo.setSampleRate(sr); lfo.reset();
        order.fill(0);
        counter = 0;
        sustainPedal = false;
        gainStep = static_cast<float>(1.0 / (0.02 * sr));                     // 20 ms gain smoothing
        cutoffStep = (std::log2(Ladder::maxCutoff) - std::log2(Ladder::minCutoff)) / (0.02 * sr);
        cutoffLog2 = cutoffTarget;
    }
    void setParams(const VoiceParams& p) noexcept { for (auto& v : voices) v.setParams(p); }
    void setModParams(const ModParams& p) noexcept {
        mod = p;
        updateDrone();
        cutoffTarget = std::log2(std::clamp(static_cast<double>(p.cutoff), Ladder::minCutoff, Ladder::maxCutoff));
        lfo.setRate(p.lfoRate); lfo.setWave(p.lfoWave);
        for (auto& v : voices) v.setResonance(p.resonance);
    }
    void setGain(float g) noexcept { targetGain = g; }
    void setModWheel(float v) noexcept { modWheel = std::clamp(v, 0.0f, 1.0f); }   // Spec S3.5

    // Patch (Spec S5.4). Call on the audio thread (the processor feeds it from a lock-free queue).
    void connect(In in, Out from) noexcept { patch.connect(in, from); updateDrone(); }
    void disconnect(In in) noexcept { patch.disconnect(in); updateDrone(); }
    void apply(const PatchCommand& c) noexcept {
        if (c.output == Patch::none) disconnect(c.input);
        else connect(c.input, static_cast<Out>(c.output));
    }
    const Patch& getPatch() const noexcept { return patch; }

    void setVoiceCount(int n) noexcept {
        n = n < 1 ? 1 : (n > maxVoices ? maxVoices : n);
        for (int i = n; i < voiceCount; ++i) voices[static_cast<std::size_t>(i)].noteOff(patch); // let them ring out
        voiceCount = n;
    }
    int getVoiceCount() const noexcept { return voiceCount; }

    void noteOn(int note, float velocity) noexcept {
        const int i = allocate(note);
        voices[static_cast<std::size_t>(i)].noteOn(note, velocity, patch);
        order[static_cast<std::size_t>(i)] = ++counter;
    }
    void noteOff(int note) noexcept {
        for (auto& v : voices) if (v.isKeyDown() && v.getNote() == note) v.releaseKey(sustainPedal, patch);
    }
    void allNotesOff() noexcept { sustainPedal = false; for (auto& v : voices) v.noteOff(patch); }

    // Spec S3.3: sustain pedal (CC64). Releasing the pedal releases voices whose key is up.
    void setSustain(bool down) noexcept {
        sustainPedal = down;
        if (!down) for (auto& v : voices) if (v.isHeld() && !v.isKeyDown()) v.noteOff(patch);
    }
    bool isSustainDown() const noexcept { return sustainPedal; }

    // Spec S3.4: pitch bend in semitones, applied to all voices (sounding and new).
    void setPitchBend(double semitones) noexcept { for (auto& v : voices) v.setPitchBend(semitones); }

    // Renders `n` samples into left/right (right may be null). Overwrites output.
    void render(float* left, float* right, int n) noexcept {
        const Out outSrc = patch.source(In::OutIn);
        const bool globalOut = isGlobal(outSrc);
        for (int i = 0; i < n; ++i) {
            if (gain < targetGain)      gain = std::fmin(gain + gainStep, targetGain);
            else if (gain > targetGain) gain = std::fmax(gain - gainStep, targetGain);
            if (cutoffLog2 < cutoffTarget)      cutoffLog2 = std::fmin(cutoffLog2 + cutoffStep, cutoffTarget);
            else if (cutoffLog2 > cutoffTarget) cutoffLog2 = std::fmax(cutoffLog2 - cutoffStep, cutoffTarget);

            const GlobalSignals g{lfo.next(), noise.next(), modWheel};
            float sum = 0.0f;
            for (auto& v : voices)                    // includes voices ringing out after a count reduction
                if (v.isActive() || (drone && v.hasPlayed())) sum += v.process(g, patch, mod, cutoffLog2);
            if (globalOut) sum = outSrc == Out::Lfo ? g.lfo : g.noise;   // a global source is output once

            const float s = SoftLimiter::process(sum * gain * headroom);   // Spec S4.6
            left[i] = s;
            if (right) right[i] = s;
        }
    }

    int activeVoices() const noexcept {
        int n = 0; for (const auto& v : voices) n += v.isActive() ? 1 : 0; return n;
    }
    const Voice& voice(int i) const noexcept { return voices[static_cast<std::size_t>(i)]; }

private:
    // Spec S5.6, S5.8: with out.in or vca.cv patched, a VCA initial gain, or ENV1 gated by something
    // other than the keys, the sound no longer ends with the key, so voices that have played keep running.
    void updateDrone() noexcept {
        drone = patch.isPatched(In::OutIn) || patch.isPatched(In::VcaCv) || mod.vcaInitial > 0.0f
             || patch.source(In::Env1Gate) != Out::KeysGate;
    }

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
    Patch patch;
    ModParams mod;
    Lfo lfo;
    Noise noise;
    std::uint64_t counter = 0;
    int voiceCount = 8;
    bool sustainPedal = false, drone = false;
    float gain = 0.0f, targetGain = 1.0f, gainStep = 0.001f, modWheel = 0.0f;
    double cutoffLog2 = std::log2(18000.0), cutoffTarget = std::log2(18000.0), cutoffStep = 0.01;
};

} // namespace lmw::dsp
