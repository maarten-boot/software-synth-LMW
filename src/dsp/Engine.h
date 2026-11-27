// SPDX-License-Identifier: AGPL-3.0-or-later
// Polyphonic semi-modular engine (Spec S4, S5). Per voice: VCO 1-3, ENV1, ENV2, Mixer, VCF 1-2, FORMANT, VCA;
// shared: LFO 1-2, Noise, Random (S&H). Every module input reads its patch cable if present, else its normalled
// source (Spec S5.3, S5.4). Pure C++ (no JUCE) so it can be unit-tested and served over 9P.
// All methods are RT-safe: no allocation, locks or I/O.
#pragma once
#include "dsp/Adsr.h"
#include "dsp/Formant.h"
#include "dsp/Ladder.h"
#include "dsp/Lfo.h"
#include "dsp/Noise.h"
#include "dsp/Oscillator.h"
#include "dsp/Patch.h"
#include "dsp/Random.h"
#include "dsp/SoftLimiter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace lmw::dsp {

// Per-voice parameters (Spec S5a.1). Field names without prefix belong to ENV1.
struct VoiceParams {
    std::array<Waveform, numVcos> waves{Waveform::Saw, Waveform::Saw, Waveform::Saw};   // vcoN.wave
    float attack = 0.01f, decay = 0.2f, sustain = 0.7f, release = 0.3f;              // ENV1 -> VCA
    float env2Attack = 0.01f, env2Decay = 0.3f, env2Sustain = 0.0f, env2Release = 0.3f; // ENV2 -> VCF
};

// Spec S5.27: glide on every new note, or only while another key is held (legato).
enum class GlideMode : int { Always, Legato };

// Spec S5.25: how Spread places the voices — by pitch (as on a piano) or by voice slot (alternating).
enum class SpreadMode : int { Keys, Voices };

// Module parameters shared by all voices (Spec S5a.1).
struct ModParams {
    std::array<float, numVcos> fmSemitones{};   // vcoN.fm (base depth; the mod wheel adds up to +1 st)
    std::array<float, numVcos> pmIndex{};       // vcoN.pm: phase-modulation index in radians per 1.0 at vcoN.pm (S5.20)
    std::array<float, numVcos> vcoShape{};      // vcoN.shape 0..1 (S5.28)
    float lfoShape = 0.0f, lfo2Shape = 0.0f;    // lfo1.shape, lfo2.shape 0..1 (S5.28)
    std::array<int, numVcos> octave{};          // vcoN.octave, -2..+2 (Spec S5.10)
    std::array<float, numVcos> tune{};          // vcoN.tune, semitones
    std::array<float, numVcos> mixVco{1.0f, 0.0f, 0.0f};   // mix.vco1..3
    float mixNoise = 0.0f;                      // mix.noise
    float cutoff = 18000.0f;                    // vcf1.cutoff (Hz)
    float resonance = 0.0f;                     // vcf1.res
    float envOctaves = 0.0f;                    // vcf1.env
    float keyTrack = 0.0f;                      // vcf1.key: 1 = cutoff follows the keys 1 oct/oct around C4 (S5.13)
    FilterSlope vcfSlope = FilterSlope::Db24;   // vcf1.slope (Spec S5.9)
    FilterType vcfType = FilterType::Lowpass;   // vcf1.type
    // VCF 2 (Spec S5.11), in series after VCF 1; defaults = open (bypassed).
    float vcf2Cutoff = 18000.0f, vcf2Resonance = 0.0f, vcf2EnvOctaves = 0.0f, vcf2KeyTrack = 0.0f;
    FilterSlope vcf2Slope = FilterSlope::Db24;
    FilterType vcf2Type = FilterType::Lowpass;
    // FORMANT (Spec S5.16), after VCF 2; Mix 0 (the default) = bypassed exactly.
    float formantVowel = 0.0f;                  // formant.vowel: 0 = A, 1 = E, 2 = I, 3 = O, 4 = U (between: a glide)
    FormantVoice formantVoice = FormantVoice::Tenor;   // formant.voice
    float formantRes = 0.5f;                    // formant.res: 0.5 = the table's bandwidths
    float formantMix = 0.0f;                    // formant.mix: 0 = dry .. 1 = only the formants
    VcfRouting vcfRouting = VcfRouting::Serial;  // vcf2.routing (Spec S5.17)
    int effectOrder = 0;                         // effects.order (Spec S7.14), lmw::dsp::effectOrder
    float vcaInitial = 0.0f;                    // vca.initial (base gain added to vca.cv)
    float lfoRate = 5.0f;                       // lfo1.rate (Hz)
    LfoWave lfoWave = LfoWave::Sine;            // lfo1.wave
    float lfo2Rate = 4.0f;                      // lfo2.rate (Hz), Spec S5.12
    LfoWave lfo2Wave = LfoWave::Sine;           // lfo2.wave
    std::array<float, 2> lfoDelay{};            // lfo1.delay, lfo2.delay (s, Spec S5.24): 0 = no delay
    std::array<LfoPolarity, 2> lfoPolarity{};   // lfo1.polarity, lfo2.polarity (Spec S5.30): Bipolar = as before
    float pan = 0.0f;                           // out.pan, -1 (left) .. +1 (right) (Spec S5.25)
    float spread = 0.0f;                        // out.spread, 0 .. 1
    SpreadMode spreadMode = SpreadMode::Keys;   // out.spreadmode
    std::array<float, 3> mix2Level{1.0f, 0.0f, 0.0f};   // mix2.level1..3 (Spec S5.26)
    std::array<float, 3> mix2Pan{};                      // mix2.pan1..3, -1 .. +1
    float glide = 0.0f;                                  // keys.glide (s, Spec S5.27): 0 = off
    GlideMode glideMode = GlideMode::Always;             // keys.glidemode
    bool mix2Panned() const noexcept {                   // a MIX 2 input off the centre: the voice is stereo
        for (std::size_t k = 0; k < 3; ++k) if (mix2Pan[k] != 0.0f && mix2Level[k] != 0.0f) return true;
        return false;
    }
    float tremolo = 0.0f;                       // vca.tremolo: depth of vca.mod, 0..1 (0 = off)
    float vcaVelocity = 1.0f;                   // vca.velocity (S5.22): how much vca.vel sets the gain, 0..1
    NoiseColour noiseColour = NoiseColour::White;   // noise.colour (Spec S5.15)
    float randRate = 4.0f;                      // noise.rate: sample & hold steps per second
    float randSmooth = 0.0f;                    // noise.smooth: 0 = steps .. 1 = glides
};

inline double midiNoteToHz(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

// Spec S5.8: keys.pitch 0 = C4 (MIDI 60), 1.0 per octave; a gate is "on" above 0.5.
inline constexpr int keysPitchZeroNote = 60;
inline constexpr float gateThreshold = 0.5f;

// Mod wheel at full adds this much FM depth (Spec S3.5).
inline constexpr float modWheelFmSemitones = 1.0f;

// Values of the shared modules for the current sample.
struct GlobalSignals {
    float lfo = 0.0f, lfo2 = 0.0f, noise = 0.0f, modWheel = 0.0f, rand = 0.0f;
};

// Spec S5.25: equal-power pan, scaled so the centre is exactly 1 on both sides (today's level); a voice fully
// to one side is sqrt(2) there and 0 on the other side (the same power).
inline void panGains(float position, float& left, float& right) noexcept {
    if (position == 0.0f) { left = right = 1.0f; return; }
    if (position <= -1.0f) { left = std::numbers::sqrt2_v<float>; right = 0.0f; return; }   // exactly one side
    if (position >= 1.0f)  { left = 0.0f; right = std::numbers::sqrt2_v<float>; return; }
    const double p = static_cast<double>(position);
    const double theta = (p + 1.0) * std::numbers::pi / 4.0;
    left = static_cast<float>(std::cos(theta) * std::numbers::sqrt2);
    right = static_cast<float>(std::sin(theta) * std::numbers::sqrt2);
}

class Voice {
public:
    void prepare(double sr) noexcept {
        sampleRate = sr; sinceOn = 0; lfoGain = {1.0f, 1.0f};
        for (auto& o : osc) o.setSampleRate(sr);
        env1.setSampleRate(sr); env2.setSampleRate(sr); vcf.setSampleRate(sr); vcf2.setSampleRate(sr);
        formant.setSampleRate(sr);
        env1.reset(); env2.reset(); vcf.reset(); vcf2.reset(); formant.reset(); out.fill(0.0f);
        vcf2Wet = 0.0f; vcf2WetStep = static_cast<float>(1.0 / (0.02 * sr));
        formantWet = 0.0f; formantActive = false;
        gate = keyDown = false; played = false; lastPitch.fill(1e9);
        env1GateWasOn = env2GateWasOn = false; env1KeyGated = env2KeyGated = true;
    }
    void setParams(const VoiceParams& p) noexcept {
        for (std::size_t n = 0; n < osc.size(); ++n) osc[n].setWaveform(p.waves[n]);
        env1.setParameters(p.attack, p.decay, p.sustain, p.release);
        env2.setParameters(p.env2Attack, p.env2Decay, p.env2Sustain, p.env2Release);
    }
    void setResonance(float r, float r2) noexcept { vcf.setResonance(r); vcf2.setResonance(r2); }
    void setFilterMode(FilterSlope slope, FilterType type, FilterSlope slope2, FilterType type2) noexcept {
        vcf.setMode(slope, type); vcf2.setMode(slope2, type2);
    }

    // Retriggering an active voice keeps oscillator phase, filter state and envelope level
    // continuous, so stealing a voice does not click. An envelope whose gate input is keys.gate
    // (normalled or by cable) follows the key events directly, so a repeated note retriggers it;
    // an envelope gated by another signal follows that signal's edges in process() (Spec S5.8).
    // Spec S5.27: glideFrom = the pitch (semitones from C4) to slide from over glideSeconds; none: start on the note.
    void noteOn(int n, float vel, const Patch& patch, std::optional<double> glideFrom = std::nullopt, double glideSeconds = 0.0) noexcept {
        const double target = static_cast<double>(n - keysPitchZeroNote);
        if (glideFrom && glideSeconds > 0.0 && *glideFrom != target) {
            glidePitch = *glideFrom;
            glideStep = std::fabs(target - *glideFrom) / (glideSeconds * sampleRate);   // even in octaves (S5.27)
        } else {
            glidePitch = target;
            glideStep = 0.0;
        }
        if (!env1.isActive()) { for (auto& o : osc) o.reset(); vcf.reset(); vcf2.reset(); formant.reset(); }
        note = n; velocity = vel; gate = true; keyDown = true; played = true;
        sinceOn = 0;                              // Spec S5.24: the LFO delay starts again at every note-on
        lastPitch.fill(1e9);                      // force frequency update
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

    // One sample through the voice's modules, in order KEYS, ENV1, ENV2, VCO 1-3, MIX, VCF 1, VCF 2, FORMANT, VCA.
    // `vcoUsed`: VCOs whose output is heard or patched somewhere (others are not computed).
    // Returns this voice's contribution to out.in (only meaningful for per-voice sources).
    float process(const GlobalSignals& g, const Patch& patch, const ModParams& mp,
                  double cutoffLog2, double cutoff2Log2, const std::array<bool, numVcos>& vcoUsed) noexcept {
        // Spec S5.24: how much of LFO 1 / 2 this voice gets: nothing for the first half of the Delay, then a
        // linear fade-in to full at the Delay; Delay 0 = 1 exactly. Counted while the gate is open, so in
        // release the depth stays where it was.
        const double t = static_cast<double>(sinceOn) / sampleRate;
        for (std::size_t k = 0; k < 2; ++k) {
            const double d = static_cast<double>(mp.lfoDelay[k]);
            lfoGain[k] = !(d > 0.0) ? 1.0f : static_cast<float>(std::clamp((t - 0.5 * d) / (0.5 * d), 0.0, 1.0));
        }
        if (gate && sinceOn < (std::int64_t{1} << 40)) ++sinceOn;
        // Spec S5.27: the key pitch glides towards the note (exactly the note when there is no glide, as before).
        const double noteSemitones = static_cast<double>(note - keysPitchZeroNote);
        if (glidePitch != noteSemitones) {
            if (glideStep <= 0.0) glidePitch = noteSemitones;
            else if (glidePitch < noteSemitones) glidePitch = std::min(noteSemitones, glidePitch + glideStep);
            else glidePitch = std::max(noteSemitones, glidePitch - glideStep);
        }
        const double keySemitones = glidePitch + bend;                                    // Spec S5.8
        out[o(Out::KeysPitch)] = static_cast<float>(keySemitones / 12.0);
        out[o(Out::KeysGate)] = gate ? 1.0f : 0.0f;
        out[o(Out::KeysVel)] = velocity;

        followGate(env1, In::Env1Gate, env1GateWasOn, env1KeyGated, g, patch);
        followGate(env2, In::Env2Gate, env2GateWasOn, env2KeyGated, g, patch);
        out[o(Out::Env1)] = env1.next();
        out[o(Out::Env2)] = env2.next();

        for (int n = 0; n < numVcos; ++n) {
            if (!vcoUsed[static_cast<std::size_t>(n)]) continue;
            const auto i = static_cast<std::size_t>(n);
            // vcoN.pitch: from the keys it is used in full precision; any other source is 1.0 per octave.
            const In pitchIn = vcoPitchIn(n);
            const double semitones = patch.onlySource(pitchIn, Out::KeysPitch)
                ? keySemitones : 12.0 * static_cast<double>(read(pitchIn, g, patch));
            const float fmDepth = mp.fmSemitones[i] + g.modWheel * modWheelFmSemitones;   // Spec S3.5
            const double pitch = semitones + static_cast<double>(read(vcoFmIn(n), g, patch) * fmDepth)
                               + (12.0 * mp.octave[i] + static_cast<double>(mp.tune[i]));   // Spec S5.10
            if (pitch < lastPitch[i] || pitch > lastPitch[i]) { lastPitch[i] = pitch;   // changed (exact check)
                osc[i].setFrequency(midiNoteToHz(keysPitchZeroNote) * std::exp2(pitch / 12.0)); }
            // Spec S5.20: phase modulation, index x input radians; off (exactly) while the PM fader is 0.
            double pm = 0.0;
            if (mp.pmIndex[i] > 0.0f) {
                const float x = read(vcoPmIn(n), g, patch);
                if (std::isfinite(x)) pm = static_cast<double>(mp.pmIndex[i] * x) / (2.0 * std::numbers::pi);
            }
            // Spec S5.28: Shape = fader + vcoN.shape input (only while patched, so the default stays exact), 0..1.
            const In shapeIn = vcoShapeIn(n);
            osc[i].setShape(patch.isPatched(shapeIn) ? std::clamp(mp.vcoShape[i] + read(shapeIn, g, patch), 0.0f, 1.0f)
                                                     : mp.vcoShape[i]);
            out[o(vcoOut(n))] = osc[i].next(pm);
        }

        out[o(Out::Mix)] = read(In::MixIn1, g, patch) * mp.mixVco[0] + read(In::MixIn2, g, patch) * mp.mixVco[1]
                         + read(In::MixIn3, g, patch) * mp.mixVco[2] + read(In::MixIn4, g, patch) * mp.mixNoise;

        // Spec S5.13: key tracking (octaves = Key x keys.pitch, 0 at C4) and the Res input (added to the
        // Reso fader). Both only when used, so the default stays exactly as it was.
        const double keyOctaves = keySemitones / 12.0;
        double c1 = cutoffLog2 + static_cast<double>(read(In::Vcf1Cutoff, g, patch) * mp.envOctaves);
        if (mp.keyTrack > 0.0f) c1 += static_cast<double>(mp.keyTrack) * keyOctaves;
        if (patch.isPatched(In::Vcf1Res)) vcf.setResonance(mp.resonance + read(In::Vcf1Res, g, patch));
        vcf.setCutoff(std::exp2(c1));
        out[o(Out::Vcf1)] = vcf.process(read(In::Vcf1In, g, patch));

        // VCF 2 (Spec S5.11). Always computed (its state stays current); when open — 24 dB lowpass,
        // no resonance, cutoff at the maximum — its output is exactly its input. Entering or leaving
        // that state crossfades over 20 ms.
        double c2 = cutoff2Log2 + static_cast<double>(read(In::Vcf2Cutoff, g, patch) * mp.vcf2EnvOctaves);
        if (mp.vcf2KeyTrack > 0.0f) c2 += static_cast<double>(mp.vcf2KeyTrack) * keyOctaves;
        if (patch.isPatched(In::Vcf2Res)) vcf2.setResonance(mp.vcf2Resonance + read(In::Vcf2Res, g, patch));
        const float in2 = read(In::Vcf2In, g, patch);
        vcf2.setCutoff(std::exp2(c2));
        const float filtered2 = vcf2.process(in2);
        const bool open2 = c2 >= maxCutoffLog2 && vcf2.isPlainLowpassNoResonance();
        if (open2) vcf2Wet = std::fmax(0.0f, vcf2Wet - vcf2WetStep);
        else       vcf2Wet = std::fmin(1.0f, vcf2Wet + vcf2WetStep);
        out[o(Out::Vcf2)] = vcf2Wet <= 0.0f ? in2 : (vcf2Wet >= 1.0f ? filtered2 : in2 + (filtered2 - in2) * vcf2Wet);

        // FORMANT (Spec S5.16). Mix glides over 20 ms; while it is 0 the input passes exactly and the
        // filters are not run. Vowel = fader + formant.vowel x 4 (1.0 = the whole A..U span), clamped.
        const float inF = read(In::FormantIn, g, patch);
        if (formantWet < mp.formantMix)      formantWet = std::fmin(mp.formantMix, formantWet + vcf2WetStep);
        else if (formantWet > mp.formantMix) formantWet = std::fmax(mp.formantMix, formantWet - vcf2WetStep);
        if (formantWet <= 0.0f) {
            formantActive = false;
            out[o(Out::Formant)] = inF;
        } else {
            if (!formantActive) { formant.reset(); formantActive = true; }
            const float cv = read(In::FormantVowel, g, patch);
            const float position = std::isfinite(cv) ? std::clamp(mp.formantVowel + maxVowelPosition * cv, 0.0f, maxVowelPosition)
                                                     : mp.formantVowel;   // a NaN or inf signal: the fader alone
            const float resCv = read(In::FormantRes, g, patch);   // Spec S5.29: added to the Reso fader, 0..1
            const float res = std::isfinite(resCv) ? std::clamp(mp.formantRes + resCv, 0.0f, 1.0f) : mp.formantRes;
            formant.setTarget(mp.formantVoice, position, res);
            const float wet = formant.process(inF);
            out[o(Out::Formant)] = formantWet >= 1.0f ? wet : inF + (wet - inF) * formantWet;
        }

        // MIX 2 (Spec S5.26): three inputs with a level each; while one with a level is panned off the centre the
        // voice is stereo from here (a left and a right sum, equal power, centre = 1 on both sides); mix2.out is
        // their average. With the defaults (In 1 100 %, the others 0 %) it is exactly formant.out, as before.
        float mono = 0.0f, mixL = 0.0f, mixR = 0.0f;
        const bool panned = mp.mix2Panned();
        for (int k = 0; k < 3; ++k) {
            const auto i = static_cast<std::size_t>(k);
            if (mp.mix2Level[i] == 0.0f) continue;
            const float x = read(static_cast<In>(static_cast<int>(In::Mix2In1) + k), g, patch) * mp.mix2Level[i];
            mono += x;
            if (panned) { float gl = 1.0f, gr = 1.0f; panGains(mp.mix2Pan[i], gl, gr); mixL += x * gl; mixR += x * gr; }
        }
        out[o(Out::Mix2)] = panned ? 0.5f * (mixL + mixR) : mono;

        float vcaGain = std::clamp(mp.vcaInitial + read(In::VcaCv, g, patch), 0.0f, 1.0f);   // Spec S5.5
        // Spec S5.12: tremolo. vca.mod (+-1) scales the gain between 1 (mod +1) and 1 - depth (mod -1);
        // depth 0 leaves the gain exactly as it is.
        if (mp.tremolo > 0.0f)
            vcaGain *= 1.0f - mp.tremolo * 0.5f * (1.0f - std::clamp(read(In::VcaMod, g, patch), -1.0f, 1.0f));
        // Spec S5.22: gain x (1 - amount + amount x vca.vel); vca.vel is keys.vel unless cabled. At 100 % with the
        // normal connection this is gain x velocity exactly (1 - 1 = 0, 0 + 1 x v = v), as before.
        const float a = mp.vcaVelocity;
        const float velGain = std::clamp(1.0f - a + a * read(In::VcaVel, g, patch), 0.0f, 1.0f);
        out[o(Out::Vca)] = read(In::VcaIn, g, patch) * vcaGain * velGain;
        // S5.26: the stereo voice goes on through the VCA to OUT while vca.in reads only mix2.out and out.in only
        // vca.out (normalled, or one cable from them at its level); otherwise it is mono from that point.
        stereo = false;
        if (panned) {
            const float inLevel = singleSource(In::VcaIn, Out::Mix2, patch), outLevel = singleSource(In::OutIn, Out::Vca, patch);
            if (inLevel > 0.0f && outLevel > 0.0f) {
                const float k = inLevel * vcaGain * velGain * outLevel;
                outL = mixL * k;
                outR = mixR * k;
                stereo = true;
            }
        }

        // Spec S5.25: this voice's out.pan input (per voice), read only while patched.
        panInput = patch.isPatched(In::OutPan) ? read(In::OutPan, g, patch) : 0.0f;
        return readPerVoice(In::OutIn, g, patch);           // global pins are added once by the engine
    }
    float panIn() const noexcept { return panInput; }
    bool isStereo() const noexcept { return stereo; }           // S5.26: this sample's left / right below
    float left() const noexcept { return outL; }
    float right() const noexcept { return outR; }

    bool isActive() const noexcept { return env1.isActive(); }
    bool isHeld() const noexcept { return gate && env1.isActive(); }   // key down or sustained by pedal
    bool isKeyDown() const noexcept { return keyDown && env1.isActive(); }
    bool hasPlayed() const noexcept { return played; }
    int getNote() const noexcept { return note; }
    const Adsr& envelope() const noexcept { return env1; }
    float output(Out which) const noexcept { return out[o(which)]; }

private:
    static constexpr std::size_t o(Out x) noexcept { return static_cast<std::size_t>(x); }
    static bool keyGated(In gateIn, const Patch& patch) noexcept { return patch.onlySource(gateIn, Out::KeysGate); }

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

    float value(Out src, const GlobalSignals& g) const noexcept {
        if (src == noSource)   return 0.0f;           // input without a normalled source
        if (src == Out::Lfo1)  return g.lfo * lfoGain[0];     // Spec S5.24: delayed per voice
        if (src == Out::Lfo2)  return g.lfo2 * lfoGain[1];
        if (src == Out::Noise) return g.noise;
        if (src == Out::Rand)  return g.rand;
        if (src == Out::KeysMod) return g.modWheel;           // Spec S6.24: the mod wheel, shared
        return out[o(src)];                    // computed earlier this sample, or previous sample (feedback)
    }
    // Spec S5.14: the normalled source, or the sum of level x output over the input's pins.
    float read(In in, const GlobalSignals& g, const Patch& patch) const noexcept {
        const int n = patch.pinCount(in);
        if (n == 0) {
            if (patch.routing() == VcfRouting::Serial) return value(normalled[static_cast<std::size_t>(in)], g);
            const auto ns = patch.normals(in);                // Spec S5.17: parallel VCFs
            float s = 0.0f;
            for (int k = 0; k < ns.n; ++k) s += value(ns.s[static_cast<std::size_t>(k)].source, g) * ns.s[static_cast<std::size_t>(k)].level;
            return s;
        }
        float sum = 0.0f;
        for (int k = 0; k < n; ++k) {
            const auto& p = patch.pin(in, k);
            sum += value(static_cast<Out>(p.source), g) * p.level;
        }
        return sum;
    }
    // As read(), without global sources (out.in: those are output once, not once per voice).
    float readPerVoice(In in, const GlobalSignals& g, const Patch& patch) const noexcept {
        const int n = patch.pinCount(in);
        if (n == 0) { const Out src = normalled[static_cast<std::size_t>(in)]; return isGlobal(src) ? 0.0f : value(src, g); }
        float sum = 0.0f;
        for (int k = 0; k < n; ++k) {
            const auto& p = patch.pin(in, k);
            if (!isGlobal(static_cast<Out>(p.source))) sum += value(static_cast<Out>(p.source), g) * p.level;
        }
        return sum;
    }

    std::array<Oscillator, numVcos> osc;
    Adsr env1, env2;
    Ladder vcf, vcf2;
    float vcf2Wet = 0.0f, vcf2WetStep = 1.0f / 960.0f;   // 0 = VCF 2 bypassed (open)
    FormantFilter formant;                     // Spec S5.16
    float formantWet = 0.0f;                   // smoothed formant.mix; 0 = bypassed
    bool formantActive = false;
    static inline const double maxCutoffLog2 = std::log2(Ladder::maxCutoff);
    std::array<float, numOutputs> out{};
    int note = 60;
    float velocity = 0.0f;
    double bend = 0.0;                         // semitones
    std::array<double, numVcos> lastPitch{1e9, 1e9, 1e9};
    bool gate = false;                         // ENV1 open (key down or sustained)
    bool keyDown = false;                      // physical key still down
    bool played = false;                       // has had a note since prepare()
    bool env1GateWasOn = false, env2GateWasOn = false;     // gate level last sample (edge detection)
    bool env1KeyGated = true, env2KeyGated = true;         // gate source was keys.gate last sample
    double sampleRate = 48000.0;
    std::int64_t sinceOn = 0;                  // samples with the gate open since the last note-on (S5.24)
    std::array<float, 2> lfoGain{1.0f, 1.0f}; // this voice's share of LFO 1 / LFO 2 now (S5.24)
    float panInput = 0.0f;                     // out.pan read this sample (S5.25)
    double glidePitch = 0.0, glideStep = 0.0;  // S5.27: the sliding key pitch (semitones from C4) and its step
    bool stereo = false;                       // S5.26: a stereo voice this sample (outL / outR)
    float outL = 0.0f, outR = 0.0f;

    // S5.26: the level at which `in` reads only `src` — 1 when normalled from it and unpatched, the cable's level
    // when its one cable comes from it; 0 otherwise.
    static float singleSource(In in, Out src, const Patch& patch) noexcept {
        const int n = patch.pinCount(in);
        if (n == 0) return patch.normalLevel(in, src) > 0.0f && patch.normals(in).n == 1 ? 1.0f : 0.0f;
        if (n == 1 && static_cast<Out>(patch.pin(in, 0).source) == src) return patch.pin(in, 0).level;
        return 0.0f;
    }
};

// Spec S5.25: the Voices spread term of a voice slot — alternating left / right, wider first; 0 with one voice.
inline float voiceSlotPan(int slot, int voiceCount) noexcept {
    if (voiceCount <= 1) return 0.0f;
    static constexpr float pattern[8] = {-1.0f, 1.0f, -0.5f, 0.5f, -0.75f, 0.75f, -0.25f, 0.25f};
    return pattern[slot % 8];
}

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
        lfo2.setSampleRate(sr); lfo2.reset();
        lastGlobal = {}; newestVoice = 0;
        noise.reset();
        random.setSampleRate(sr); random.reset();
        gain = 0.0f;                         // fade in like a fresh instance (hosts re-prepare often)
        order.fill(0);
        counter = 0;
        sustainPedal = false;
        gainStep = static_cast<float>(1.0 / (0.02 * sr));                     // 20 ms gain smoothing
        panStep = static_cast<float>(2.0 / (0.02 * sr));                      // S5.25: 20 ms across the full width
        cutoffStep = (std::log2(Ladder::maxCutoff) - std::log2(Ladder::minCutoff)) / (0.02 * sr);
        cutoffLog2 = cutoffTarget;
        cutoff2Log2 = cutoff2Target;
    }
    void setParams(const VoiceParams& p) noexcept { for (auto& v : voices) v.setParams(p); }
    void setModParams(const ModParams& p) noexcept {
        mod = p;
        patch.setRouting(p.vcfRouting);          // Spec S5.17
        patch.setEffectOrder(p.effectOrder);     // Spec S7.14
        updateRouting();
        cutoffTarget = std::log2(std::clamp(static_cast<double>(p.cutoff), Ladder::minCutoff, Ladder::maxCutoff));
        cutoff2Target = std::log2(std::clamp(static_cast<double>(p.vcf2Cutoff), Ladder::minCutoff, Ladder::maxCutoff));
        lfo.setRate(p.lfoRate); lfo.setWave(p.lfoWave);
        lfo2.setRate(p.lfo2Rate); lfo2.setWave(p.lfo2Wave);
        if (!patch.isPatched(In::Lfo1Shape)) lfo.setShape(p.lfoShape);     // S5.28 (patched: per sample in render)
        if (!patch.isPatched(In::Lfo2Shape)) lfo2.setShape(p.lfo2Shape);
        noise.setColour(p.noiseColour);
        random.setRate(p.randRate); random.setSmooth(p.randSmooth);
        for (auto& v : voices) {
            v.setResonance(p.resonance, p.vcf2Resonance);
            v.setFilterMode(p.vcfSlope, p.vcfType, p.vcf2Slope, p.vcf2Type);
        }
    }
    void setGain(float g) noexcept { targetGain = g; }
    void setModWheel(float v) noexcept { modWheel = std::clamp(v, 0.0f, 1.0f); }   // Spec S3.5

    // Patch (Spec S5.4, S5.14). Call on the audio thread (the processor feeds it from a lock-free queue).
    // connect(): one cable at full level, replacing the input's pins. addPin(): add a pin or set its level.
    void connect(In in, Out from) noexcept { patch.disconnect(in); patch.connect(in, from); updateRouting(); }
    void addPin(In in, Out from, float level) noexcept { patch.connect(in, from, level); updateRouting(); }
    void disconnect(In in) noexcept { patch.disconnect(in); updateRouting(); }
    void apply(const PatchCommand& c) noexcept {
        if (c.output == Patch::none) disconnect(c.input);
        else addPin(c.input, static_cast<Out>(c.output), c.level);
    }
    const Patch& getPatch() const noexcept { return patch; }

    void setVoiceCount(int n) noexcept {
        n = n < 1 ? 1 : (n > maxVoices ? maxVoices : n);
        for (int i = n; i < voiceCount; ++i) voices[static_cast<std::size_t>(i)].noteOff(patch); // let them ring out
        voiceCount = n;
    }
    int getVoiceCount() const noexcept { return voiceCount; }

    void noteOn(int note, float velocity) noexcept {
        // Spec S5.27: glide from the pitch of the newest note before this one — always, or (Legato) only while
        // another key is still held.
        std::optional<double> from;
        if (mod.glide > 0.0f && lastNote) {
            bool held = false;
            for (const auto& v : voices) held = held || v.isKeyDown();
            if (mod.glideMode == GlideMode::Always || held) from = *lastNote;
        }
        const int i = allocate(note);
        voices[static_cast<std::size_t>(i)].noteOn(note, velocity, patch, from, static_cast<double>(mod.glide));
        lastNote = static_cast<double>(note - keysPitchZeroNote);   // the next note glides from here
        order[static_cast<std::size_t>(i)] = ++counter;
        newestVoice = i;                                // Spec S5.19: per-voice sources of the LFO rate inputs
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
        for (int i = 0; i < n; ++i) {
            if (gain < targetGain)      gain = std::fmin(gain + gainStep, targetGain);
            else if (gain > targetGain) gain = std::fmax(gain - gainStep, targetGain);
            if (cutoffLog2 < cutoffTarget)      cutoffLog2 = std::fmin(cutoffLog2 + cutoffStep, cutoffTarget);
            else if (cutoffLog2 > cutoffTarget) cutoffLog2 = std::fmax(cutoffLog2 - cutoffStep, cutoffTarget);
            if (cutoff2Log2 < cutoff2Target)      cutoff2Log2 = std::fmin(cutoff2Log2 + cutoffStep, cutoff2Target);
            else if (cutoff2Log2 > cutoff2Target) cutoff2Log2 = std::fmax(cutoff2Log2 - cutoffStep, cutoff2Target);

            // Spec S5.19: a patched LFO rate input moves the rate by its signal in octaves (1.0 = double),
            // read from the previous sample; unpatched, the rate is exactly the fader's.
            if (lfo1RatePatched) lfo.setRate(modulatedRate(mod.lfoRate, In::Lfo1Rate));
            if (lfo2RatePatched) lfo2.setRate(modulatedRate(mod.lfo2Rate, In::Lfo2Rate));
            // Spec S5.28: a patched LFO Shape input adds to the fader (from the previous sample), 0..1.
            if (lfo1ShapePatched) lfo.setShape(std::clamp(mod.lfoShape + readShared(In::Lfo1Shape), 0.0f, 1.0f));
            if (lfo2ShapePatched) lfo2.setShape(std::clamp(mod.lfo2Shape + readShared(In::Lfo2Shape), 0.0f, 1.0f));
            const GlobalSignals g{polarize(lfo.next(), mod.lfoPolarity[0]), polarize(lfo2.next(), mod.lfoPolarity[1]),   // S5.30
                                  noise.next(), modWheel, random.next()};
            lastGlobal = g;
            // Spec S5.25: Pan and Spread glide (20 ms); while both are 0 and out.pan has no cable the output is
            // the mono sum on both sides, exactly as before.
            if (panNow < mod.pan)         panNow = std::fmin(panNow + panStep, mod.pan);
            else if (panNow > mod.pan)    panNow = std::fmax(panNow - panStep, mod.pan);
            if (spreadNow < mod.spread)      spreadNow = std::fmin(spreadNow + panStep, mod.spread);
            else if (spreadNow > mod.spread) spreadNow = std::fmax(spreadNow - panStep, mod.spread);
            const bool panning = right != nullptr && (panNow != 0.0f || spreadNow != 0.0f || panPatched || mod.mix2Panned());
            float sum = 0.0f, sumL = 0.0f, sumR = 0.0f;
            for (std::size_t k = 0; k < voices.size(); ++k) {   // includes voices ringing out after a count reduction
                auto& v = voices[k];
                if (!(v.isActive() || (drone && v.hasPlayed()))) continue;
                const float x = v.process(g, patch, mod, cutoffLog2, cutoff2Log2, vcoUsed);
                if (!panning) { sum += x; continue; }
                const float xl = v.isStereo() ? v.left() : x, xr = v.isStereo() ? v.right() : x;   // S5.26
                float term = 0.0f;
                if (spreadNow != 0.0f)
                    term = mod.spreadMode == SpreadMode::Voices
                        ? voiceSlotPan(static_cast<int>(k), voiceCount)
                        : std::clamp(v.output(Out::KeysPitch) / 3.0f, -1.0f, 1.0f);   // +-3 octaves from C4
                const float pos = std::clamp(panNow + spreadNow * term + v.panIn(), -1.0f, 1.0f);
                float gl = 1.0f, gr = 1.0f;
                panGains(pos, gl, gr);
                sumL += xl * gl;
                sumR += xr * gr;
            }
            const float globals = globalOnOut ? globalOut(g) : 0.0f;   // a global source is output once (S5.5), centred

            if (!panning) {
                const float s = SoftLimiter::process((sum + globals) * gain * headroom);   // Spec S4.6
                left[i] = s;
                if (right) right[i] = s;
            } else {                                                                     // the limiter per side
                left[i] = SoftLimiter::process((sumL + globals) * gain * headroom);
                right[i] = SoftLimiter::process((sumR + globals) * gain * headroom);
            }
        }
    }

    // Spec S7.6: keys.pitch (octaves from C4, with bend) of the newest note, for the COMB's key tracking.
    float newestKeyOctaves() const noexcept { return voices[static_cast<std::size_t>(newestVoice)].output(Out::KeysPitch); }

    int activeVoices() const noexcept {
        int n = 0; for (const auto& v : voices) n += v.isActive() ? 1 : 0; return n;
    }
    const Voice& voice(int i) const noexcept { return voices[static_cast<std::size_t>(i)]; }

private:
    // Spec S5.6, S5.8: with out.in or vca.cv patched, a VCA initial gain, or ENV1 gated by something
    // other than the keys, the sound no longer ends with the key, so voices that have played keep running.
    void updateRouting() noexcept {
        drone = patch.isPatched(In::OutIn) || patch.isPatched(In::VcaCv) || mod.vcaInitial > 0.0f
             || !patch.onlySource(In::Env1Gate, Out::KeysGate);
        lfo1RatePatched = patch.isPatched(In::Lfo1Rate);
        lfo2RatePatched = patch.isPatched(In::Lfo2Rate);
        panPatched = patch.isPatched(In::OutPan);                // S5.25
        if (!lfo1RatePatched) lfo.setRate(mod.lfoRate);     // back to the fader's rate exactly
        if (!lfo2RatePatched) lfo2.setRate(mod.lfo2Rate);
        lfo1ShapePatched = patch.isPatched(In::Lfo1Shape);       // S5.28
        lfo2ShapePatched = patch.isPatched(In::Lfo2Shape);
        if (!lfo1ShapePatched) lfo.setShape(mod.lfoShape);
        if (!lfo2ShapePatched) lfo2.setShape(mod.lfo2Shape);
        globalOnOut = false;
        for (int k = 0; k < patch.pinCount(In::OutIn); ++k)
            globalOnOut = globalOnOut || isGlobal(static_cast<Out>(patch.pin(In::OutIn, k).source));
        // Spec S5.10: a VCO is computed only when something uses it: a mixer input with its level
        // above 0, or any other input. VCO 2 and 3 at level 0 cost nothing and leave the sound as is.
        const std::array<float, 4> mixLevel{mod.mixVco[0], mod.mixVco[1], mod.mixVco[2], mod.mixNoise};
        vcoUsed.fill(false);
        for (int i = 0; i < numInputs; ++i) {
            const auto in = static_cast<In>(i);
            const int mixIndex = i - static_cast<int>(In::MixIn1);
            if (mixIndex >= 0 && mixIndex < 4 && !(mixLevel[static_cast<std::size_t>(mixIndex)] > 0.0f)) continue;
            const int n = patch.pinCount(in);
            for (int k = 0; k < (n == 0 ? 1 : n); ++k) {        // every source the input reads
                const Out src = n == 0 ? normalled[static_cast<std::size_t>(i)] : static_cast<Out>(patch.pin(in, k).source);
                const int vco = static_cast<int>(src) - static_cast<int>(Out::Vco1);
                if (vco >= 0 && vco < numVcos) vcoUsed[static_cast<std::size_t>(vco)] = true;
            }
        }
    }

    // Spec S5.19: the value of an input of a shared module (the LFO rate inputs): shared sources as they
    // were last sample, per-voice sources from the voice of the newest note ("last note").
    float readShared(In in) const noexcept {
        const Voice& v = voices[static_cast<std::size_t>(newestVoice)];
        const auto valueOf = [&](Out src) {
            if (src == noSource)   return 0.0f;
            if (src == Out::Lfo1)  return lastGlobal.lfo;
            if (src == Out::Lfo2)  return lastGlobal.lfo2;
            if (src == Out::Noise) return lastGlobal.noise;
            if (src == Out::Rand)  return lastGlobal.rand;
            if (src == Out::KeysMod) return lastGlobal.modWheel;   // S6.24
            return v.output(src);
        };
        const int n = patch.pinCount(in);
        if (n == 0) return valueOf(normalled[static_cast<std::size_t>(in)]);
        float s = 0.0f;
        for (int k = 0; k < n; ++k) s += valueOf(static_cast<Out>(patch.pin(in, k).source)) * patch.pin(in, k).level;
        return s;
    }
    float modulatedRate(float base, In in) const noexcept {
        const float octaves = readShared(in);
        if (!std::isfinite(octaves)) return base;
        return std::clamp(base * std::exp2(std::clamp(octaves, -12.0f, 12.0f)), minModulatedLfoRate, maxModulatedLfoRate);
    }

    float globalOut(const GlobalSignals& g) const noexcept {   // global pins on out.in, summed once
        float s = 0.0f;
        for (int k = 0; k < patch.pinCount(In::OutIn); ++k) {
            const auto& p = patch.pin(In::OutIn, k);
            const Out src = static_cast<Out>(p.source);
            if (isGlobal(src))
                s += (src == Out::Lfo1 ? g.lfo : src == Out::Lfo2 ? g.lfo2 : src == Out::Rand ? g.rand
                      : src == Out::KeysMod ? g.modWheel : g.noise) * p.level;
        }
        return s;
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
    Lfo lfo, lfo2;                         // LFO 1, LFO 2 (Spec S5.12)
    Noise noise;
    Random random;                         // sample & hold (Spec S5.15)
    std::uint64_t counter = 0;
    int voiceCount = 8;
    bool sustainPedal = false, drone = false, globalOnOut = false;
    bool lfo1RatePatched = false, lfo2RatePatched = false;   // Spec S5.19
    bool lfo1ShapePatched = false, lfo2ShapePatched = false; // Spec S5.28
    int newestVoice = 0;                     // voice of the newest note (S5.19)
    GlobalSignals lastGlobal{};              // shared signals of the previous sample (S5.19)
    static constexpr float minModulatedLfoRate = 0.01f, maxModulatedLfoRate = 50.0f;
    std::array<bool, numVcos> vcoUsed{true, false, false};
    float gain = 0.0f, targetGain = 1.0f, gainStep = 0.001f, modWheel = 0.0f;
    std::optional<double> lastNote;                // S5.27: the newest note played (semitones from C4)
    float panNow = 0.0f, spreadNow = 0.0f, panStep = 0.001f;   // Spec S5.25: glided Pan and Spread
    bool panPatched = false;                                    // out.pan has a cable
    double cutoffLog2 = std::log2(18000.0), cutoffTarget = std::log2(18000.0), cutoffStep = 0.01;
    double cutoff2Log2 = std::log2(18000.0), cutoff2Target = std::log2(18000.0);
};

} // namespace lmw::dsp
