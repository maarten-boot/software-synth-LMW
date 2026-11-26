// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "ui/Schematic.h"
#include "dsp/PatchText.h"
#include "app/StateMigration.h"
#include "app/PatchFile.h"
#include "app/NinePService.h"
#include "ui/Config.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include "PluginEditor.h"
#include "ui/ParamText.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout LMWSynthProcessor::createLayout(std::map<std::string, FloatInfo>& info) {
    namespace P = lmw::param;
    namespace tx = lmw::ui::text;
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;
    struct Skewed {                                  // a range and, when skewed, its centre (for /info)
        Range range;
        std::optional<float> centre;
        Skewed(Range r, std::optional<float> c = std::nullopt) : range(r), centre(c) {}
    };
    auto skewed = [](float lo, float hi, float centre) { Range r(lo, hi); r.setSkewForCentre(centre); return Skewed(r, centre); };
    auto time = [&](float maxSec) { return skewed(0.0f, maxSec, maxSec / 10.0f); };
    auto logRange = skewed;
    struct Text {                                    // value text with units (Spec S6.1); no separate host label
        Attr attr;
        const char* unit;                            // as /info reports it
    };
    auto attr = [](auto toText, auto fromText, const char* unit) {
        return Text{Attr()
            .withStringFromValueFunction([toText](float v, int) { return juce::String(toText(v)); })
            .withValueFromStringFunction([fromText](const juce::String& s) { return fromText(s.toStdString()); }), unit};
    };
    const auto seconds  = attr(tx::time, tx::parseTime, "s");
    const auto percent  = attr(tx::percent, tx::parsePercent, "%");
    const auto decibels = attr(tx::decibels, tx::parseDecibels, "dB");
    const auto hertz    = attr(tx::hertz, tx::parseHertz, "Hz");
    const auto octaves  = attr(tx::octaves, tx::parseNumber, "oct");
    const auto semis    = attr(tx::semitones, tx::parseNumber, "st");
    const auto vowels   = attr(tx::vowel, tx::parseVowel, "vowel");   // Spec S5.16: 0 = A .. 4 = U
    const auto index    = attr(tx::index, tx::parseNumber, "rad");     // Spec S5.20: PM index
    auto f = [&info](const char* id, const char* name, Skewed r, float def, const Text& t) {
        info[id] = FloatInfo{t.unit, def, r.centre};
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, r.range, def, t.attr);
    };
    auto envelope = [&](APVTS::ParameterLayout& l, const char* a, const char* d, const char* s, const char* r,
                        const char* name, float defA, float defD, float defS, float defR) {
        const juce::String n(name);
        l.add(f(a, (n + " Attack").toRawUTF8(),  time(5.0f),  defA, seconds),
              f(d, (n + " Decay").toRawUTF8(),   time(5.0f),  defD, seconds),
              f(s, (n + " Sustain").toRawUTF8(), Range(0.0f, 1.0f), defS, percent),
              f(r, (n + " Release").toRawUTF8(), time(10.0f), defR, seconds));
    };

    APVTS::ParameterLayout l;   // Spec S5a.1 — defaults reproduce the Phase-2 sound
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{P::voices, 1}, "Voices", 1, lmw::dsp::Engine::maxVoices, 8));
    for (std::size_t n = 0; n < 3; ++n) {          // Spec S5.10: VCO 1-3
        const juce::String name = "VCO " + juce::String(static_cast<int>(n) + 1);
        Range tune(-7.0f, 7.0f, 0.01f);
        l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcoWave[n], 1}, name + " Wave",
                  juce::StringArray{"Sine", "Saw", "Square", "Triangle", "Pulse"}, 1),   // Pulse: S5.18
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcoOctave[n], 1}, name + " Octave",
                  juce::StringArray{"-2", "-1", "0", "+1", "+2"}, 2),
              f(P::vcoTune[n], (name + " Tune").toRawUTF8(), tune, 0.0f, semis),
              // Skewed for more travel where small changes matter (user 2026-11-21): half way = 1 st, PM index 2.
              f(P::vcoFm[n],   (name + " FM").toRawUTF8(), skewed(0.0f, 12.0f, 1.0f), 0.0f, semis),
              f(P::vcoPm[n],   (name + " PM").toRawUTF8(), skewed(0.0f, 10.0f, 2.0f), 0.0f, index),   // Spec S5.20
              f(P::mixVco[n],  ("Mixer " + name).toRawUTF8(), Range(0.0f, 1.0f), n == 0 ? 1.0f : 0.0f, percent));
    }
    l.add(f(P::mixNoise,  "Mixer Noise",   Range(0.0f, 1.0f), 0.0f, percent));
    // Spec S5.15: NOISE module.
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::noiseColour, 1}, "Noise Colour",
              juce::StringArray{"White", "Pink", "Brown"}, 0),
          f(P::noiseRate,   "Noise Random Rate",   logRange(0.05f, 50.0f, 4.0f), 4.0f, hertz),
          f(P::noiseSmooth, "Noise Random Smooth", Range(0.0f, 1.0f), 0.0f, percent));
    for (std::size_t n = 0; n < 2; ++n) {          // Spec S5.9, S5.11: VCF 1-2 (defaults: VCF 2 open)
        const juce::String name = "VCF " + juce::String(static_cast<int>(n) + 1);
        l.add(f(P::vcfCutoff[n], (name + " Cutoff").toRawUTF8(),    logRange(20.0f, 18000.0f, 600.0f), 18000.0f, hertz),
              f(P::vcfRes[n],    (name + " Resonance").toRawUTF8(), Range(0.0f, 1.0f), 0.0f, percent),
              f(P::vcfEnv[n],    (name + " Env").toRawUTF8(),       Range(-5.0f, 5.0f), 0.0f, octaves),
              f(P::vcfKey[n],    (name + " Key").toRawUTF8(),       Range(0.0f, 1.0f), 0.0f, percent),   // Spec S5.13
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcfSlope[n], 1}, name + " Slope",
                  juce::StringArray{"36 dB", "24 dB", "12 dB", "6 dB"}, 1),   // S5.23: 36 first, default 24
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcfType[n], 1}, name + " Type",
                  juce::StringArray{"Lowpass", "Highpass", "Bandpass", "Notch"}, 0));
    }
    // Spec S5.17: VCF routing, on the VCF 2 panel.
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcfRouting, 1}, "VCF Routing",
              juce::StringArray{"Serial", "Parallel"}, 0));
    // Spec S5.16: FORMANT. Mix 0 = off (bypassed exactly).
    l.add(f(P::formantVowel, "Formant Vowel", Range(0.0f, 4.0f), 0.0f, vowels),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::formantVoice, 1}, "Formant Voice",
              juce::StringArray{"Bass", "Baritone", "Tenor", "Alto", "Soprano", "Child"}, 2),
          f(P::formantRes, "Formant Resonance", Range(0.0f, 1.0f), 0.5f, percent),
          f(P::formantMix, "Formant Mix", Range(0.0f, 1.0f), 0.0f, percent));
    l.add(f(P::vcaInitial, "VCA Initial",  Range(0.0f, 1.0f), 0.0f, percent),
          f(P::vcaTremolo, "VCA Tremolo",  Range(0.0f, 1.0f), 0.0f, percent),    // Spec S5.12
          f(P::vcaVelocity, "VCA Velocity", Range(0.0f, 1.0f), 1.0f, percent));   // Spec S5.22
    envelope(l, P::env1Attack, P::env1Decay, P::env1Sustain, P::env1Release, "Env1", 0.01f, 0.2f, 0.7f, 0.3f);
    envelope(l, P::env2Attack, P::env2Decay, P::env2Sustain, P::env2Release, "Env2", 0.01f, 0.3f, 0.0f, 0.3f);
    // Spec S5.12: LFO 1 (vibrato) and LFO 2 (tremolo).
    l.add(f(P::lfoRate, "LFO 1 Rate", logRange(0.05f, 20.0f, 2.0f), 5.0f, hertz),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::lfoWave, 1}, "LFO 1 Wave",
              juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0),
          f(P::lfo2Rate, "LFO 2 Rate", logRange(0.05f, 20.0f, 2.0f), 4.0f, hertz),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::lfo2Wave, 1}, "LFO 2 Wave",
              juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0),
          f(P::lfoDelay, "LFO 1 Delay", time(5.0f), 0.0f, seconds),      // Spec S5.24
          f(P::lfo2Delay, "LFO 2 Delay", time(5.0f), 0.0f, seconds),
          f(P::gain, "Volume", Range(0.0f, 1.0f), 0.8f, decibels),
          f(P::outPan, "Pan", Range(-1.0f, 1.0f), 0.0f, percent),          // Spec S5.25
          f(P::outSpread, "Spread", Range(0.0f, 1.0f), 0.0f, percent),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::spreadMode, 1}, "Spread mode",
              juce::StringArray{"Keys", "Voices"}, 0));
    for (std::size_t k = 0; k < 3; ++k) {                                   // Spec S5.26: MIX 2
        const juce::String n = "MIX 2 Level " + juce::String(static_cast<int>(k) + 1), pn = "MIX 2 Pan " + juce::String(static_cast<int>(k) + 1);
        l.add(f(P::mix2Level[k], n.toRawUTF8(), Range(0.0f, 1.0f), k == 0 ? 1.0f : 0.0f, percent),
              f(P::mix2Pan[k], pn.toRawUTF8(), Range(-1.0f, 1.0f), 0.0f, percent));
    }
    // Spec S7.4: reverb on the Effects tab. Mix 0 = off (bypassed exactly).
    l.add(f(P::reverbMix,     "Reverb Mix",     Range(0.0f, 1.0f), 0.0f, percent),
          f(P::reverbSize,    "Reverb Size",    Range(0.0f, 1.0f), 0.5f, percent),
          f(P::reverbDamping, "Reverb Damping", Range(0.0f, 1.0f), 0.5f, percent),
          f(P::reverbWidth,   "Reverb Width",   Range(0.0f, 1.0f), 1.0f, percent));
    // Spec S7.6: COMB on the Effects tab. Mix 0 = off (bypassed exactly).
    l.add(f(P::combFreq,     "Comb Frequency", logRange(20.0f, 5000.0f, 440.0f), 440.0f, hertz),
          f(P::combKey,      "Comb Key",       Range(0.0f, 1.0f), 0.0f, percent),
          f(P::combFeedback, "Comb Feedback",  Range(-0.95f, 0.95f), 0.5f, percent),
          f(P::combDamping,  "Comb Damping",   Range(0.0f, 1.0f), 0.2f, percent),
          f(P::combMix,      "Comb Mix",       Range(0.0f, 1.0f), 0.0f, percent));
    // Spec S7.8: DELAY on the Effects tab. Mix 0 = off (bypassed exactly).
    l.add(f(P::delayTime,     "Delay Time",     time(2.0f), 0.35f, seconds),
          f(P::delayFeedback, "Delay Feedback", Range(0.0f, 0.95f), 0.35f, percent),
          f(P::delayDamping,  "Delay Damping",  Range(0.0f, 1.0f), 0.3f, percent),
          f(P::delaySpread,   "Delay Spread",   Range(0.0f, 1.0f), 0.5f, percent),
          f(P::delayMix,      "Delay Mix",      Range(0.0f, 1.0f), 0.0f, percent));
    // Spec S7.11: ENSEMBLE between COMB and DELAY. Mix 0 = off (bypassed exactly). Rate: the slow LFO (fast = 10 x).
    l.add(f(P::ensembleDepth,   "Ensemble Depth",   Range(0.0f, 1.0f), 0.6f, percent),
          f(P::ensembleRate,    "Ensemble Rate",    logRange(0.15f, 2.4f, 0.6f), 0.6f, hertz),
          f(P::ensembleShimmer, "Ensemble Shimmer", Range(0.0f, 1.0f), 0.4f, percent),
          f(P::ensembleMix,     "Ensemble Mix",     Range(0.0f, 1.0f), 0.0f, percent));
    return l;
}

LMWSynthProcessor::LMWSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LMWSynth", createLayout(floatInfo)) {
    namespace P = lmw::param;
    auto get = [this](const char* id) { auto* p = apvts.getRawParameterValue(id); jassert(p != nullptr); return p; };
    raw.voices = get(P::voices); raw.mixNoise = get(P::mixNoise); raw.vcaInitial = get(P::vcaInitial);
    for (std::size_t n = 0; n < 2; ++n) {
        raw.cutoff[n] = get(P::vcfCutoff[n]); raw.resonance[n] = get(P::vcfRes[n]); raw.vcfEnv[n] = get(P::vcfEnv[n]); raw.vcfKey[n] = get(P::vcfKey[n]);
        raw.vcfSlope[n] = get(P::vcfSlope[n]); raw.vcfType[n] = get(P::vcfType[n]);
    }
    raw.e1a = get(P::env1Attack); raw.e1d = get(P::env1Decay); raw.e1s = get(P::env1Sustain); raw.e1r = get(P::env1Release);
    raw.e2a = get(P::env2Attack); raw.e2d = get(P::env2Decay); raw.e2s = get(P::env2Sustain); raw.e2r = get(P::env2Release);
    raw.lfoRate = get(P::lfoRate); raw.lfoWave = get(P::lfoWave); raw.gain = get(P::gain);
    raw.lfo2Rate = get(P::lfo2Rate); raw.lfo2Wave = get(P::lfo2Wave);
    raw.lfoDelay = get(P::lfoDelay); raw.lfo2Delay = get(P::lfo2Delay);
    raw.outPan = get(P::outPan); raw.outSpread = get(P::outSpread); raw.spreadMode = get(P::spreadMode);
    for (std::size_t k = 0; k < 3; ++k) { raw.mix2Level[k] = get(P::mix2Level[k]); raw.mix2Pan[k] = get(P::mix2Pan[k]); } raw.tremolo = get(P::vcaTremolo);
    raw.vcaVelocity = get(P::vcaVelocity);
    raw.revMix = get(P::reverbMix); raw.revSize = get(P::reverbSize);
    raw.revDamping = get(P::reverbDamping); raw.revWidth = get(P::reverbWidth);
    raw.combFreq = get(P::combFreq); raw.combKey = get(P::combKey); raw.combFeedback = get(P::combFeedback);
    raw.combDamping = get(P::combDamping); raw.combMix = get(P::combMix);
    raw.delayTime = get(P::delayTime); raw.delayFeedback = get(P::delayFeedback); raw.delayDamping = get(P::delayDamping);
    raw.delaySpread = get(P::delaySpread); raw.delayMix = get(P::delayMix);
    raw.ensembleDepth = get(P::ensembleDepth); raw.ensembleRate = get(P::ensembleRate);
    raw.ensembleShimmer = get(P::ensembleShimmer); raw.ensembleMix = get(P::ensembleMix);
    raw.noiseColour = get(P::noiseColour); raw.noiseRate = get(P::noiseRate); raw.noiseSmooth = get(P::noiseSmooth);
    raw.formantVowel = get(P::formantVowel); raw.formantVoice = get(P::formantVoice);
    raw.formantRes = get(P::formantRes); raw.formantMix = get(P::formantMix);
    raw.vcfRouting = get(P::vcfRouting);
    for (std::size_t n = 0; n < 3; ++n) {
        raw.wave[n] = get(P::vcoWave[n]); raw.fm[n] = get(P::vcoFm[n]); raw.pm[n] = get(P::vcoPm[n]); raw.octave[n] = get(P::vcoOctave[n]);
        raw.tune[n] = get(P::vcoTune[n]); raw.mixVco[n] = get(P::mixVco[n]);
    }
    keyboard.addListener(this);
    for (auto* param : getParameters()) param->addListener(this);   // Spec S10.10 /serial
    apvts.state.addListener(this);

    // Spec S10: 9P server — a Unix socket per instance, TCP when ninep_tcp is set in the config file.
    ninep = std::make_unique<NinePService>(*this);
    const auto settings = lmw::ui::parseConfig(LMWSynthEditor::configFile().loadFileAsString().toStdString());
    ninepAddress = ninep->start(settings.tcpEndpoint());
    std::fprintf(stderr, "LMW-Synth 9P: %s\n", ninepAddress.empty() ? "(not available)" : ninepAddress.c_str());
}

LMWSynthProcessor::~LMWSynthProcessor() {
    ninep.reset();                                   // stops the 9P threads before anything else goes
    apvts.state.removeListener(this);
    for (auto* param : getParameters()) param->removeListener(this);
    keyboard.removeListener(this);
}

void LMWSynthProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (applyingMidiInput) return;
    guiToAudio.push({lmw::dsp::NoteEvent::Kind::On, static_cast<unsigned char>(channel),
                     static_cast<unsigned char>(note), velocity});
}

void LMWSynthProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float) {
    if (applyingMidiInput) return;
    guiToAudio.push({lmw::dsp::NoteEvent::Kind::Off, static_cast<unsigned char>(channel),
                     static_cast<unsigned char>(note), 0.0f});
}

void LMWSynthProcessor::resetKeyboardDisplay() {
    audioToGui.clear();
    keyboard.reset();            // clears the display only; does not notify listeners
}

void LMWSynthProcessor::updateKeyboardFromMidiInput() {
    const juce::ScopedValueSetter<bool> guard(applyingMidiInput, true);
    lmw::dsp::NoteEvent e;
    while (audioToGui.pop(e)) {
        if (e.kind == lmw::dsp::NoteEvent::Kind::On)       keyboard.noteOn(e.channel, e.note, e.velocity);
        else if (e.kind == lmw::dsp::NoteEvent::Kind::Off) keyboard.noteOff(e.channel, e.note, 0.0f);
        else                                                keyboard.allNotesOff(e.channel);
    }
}

void LMWSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate);
    pullParameters();
    reverb.setParameters(reverbParameters());      // before setSampleRate, which makes them current (no glide)
    reverb.setSampleRate(sampleRate);              // allocates: here, not on the audio thread
    reverb.reset();
    reverbMix.reset(sampleRate, 0.02);
    reverbMix.setCurrentAndTargetValue(raw.revMix->load());
    reverbRunning = false;
    combL.prepare(sampleRate); combR.prepare(sampleRate);   // allocates: Spec S7.6
    combMix.reset(sampleRate, 0.02);
    combMix.setCurrentAndTargetValue(raw.combMix->load());
    combRunning = false;
    delayFx.prepare(sampleRate);                   // allocates: Spec S7.8
    delayMix.reset(sampleRate, 0.02);
    delayMix.setCurrentAndTargetValue(raw.delayMix->load());
    delayRunning = false;
    ensembleFx.prepare(sampleRate);                // allocates: Spec S7.11
    ensembleMix.reset(sampleRate, 0.02);
    ensembleMix.setCurrentAndTargetValue(raw.ensembleMix->load());
    ensembleRunning = false;
    prevComb = {}; prevEnsemble = {}; prevDelay = {}; prevReverb = {};
}

juce::Reverb::Parameters LMWSynthProcessor::reverbParameters() const noexcept {
    juce::Reverb::Parameters rp;
    rp.roomSize = raw.revSize->load();
    rp.damping = raw.revDamping->load();
    rp.width = raw.revWidth->load();
    rp.wetLevel = 1.0f / 3.0f;                     // JUCE scales wet by 3: about unity loudness
    rp.dryLevel = 0.0f;                            // wet only; dry is mixed in applyReverb
    return rp;
}

// Spec S7.4: out = dry x (1 - mix) + wet x mix, mix smoothed over 20 ms. The reverb runs while Mix is
// above 0 or still fading; once Mix has reached 0 it stops and its tail is cleared (it would be silent).
LMWSynthProcessor::Stereo LMWSynthProcessor::reverbSample(Stereo in, bool stereo) noexcept {
    if (!reverbRunning && !reverbMix.isSmoothing() && reverbMix.getCurrentValue() <= 0.0f) return in;
    reverbRunning = true;
    Stereo wet = in;
    if (stereo) reverb.processStereo(&wet.l, &wet.r, 1);
    else        { reverb.processMono(&wet.l, 1); wet.r = wet.l; }
    // Spec S7.4 (2026-11-26): the same loudness at every Size, and Damping darkens the tail audibly.
    const float k = reverbLpCoeff;
    reverbLp.l = wet.l * reverbSizeGain * (1.0f - k) + reverbLp.l * k;
    reverbLp.r = wet.r * reverbSizeGain * (1.0f - k) + reverbLp.r * k;
    wet = reverbLp;
    const float m = reverbMix.getNextValue();
    const Stereo out{in.l * (1.0f - m) + wet.l * m, in.r * (1.0f - m) + wet.r * m};
    if (!reverbMix.isSmoothing() && reverbMix.getCurrentValue() <= 0.0f) {
        reverb.reset();                            // RT-safe: clears the delay lines
        reverbLp = {};
        reverbRunning = false;
    }
    return out;
}

// Spec S7.6: COMB, the same Mix rule as the reverb.
LMWSynthProcessor::Stereo LMWSynthProcessor::combSample(Stereo in) noexcept {
    if (!combRunning && !combMix.isSmoothing() && combMix.getCurrentValue() <= 0.0f) return in;
    combRunning = true;
    const float m = combMix.getNextValue();
    const Stereo wet{combL.process(in.l), combR.process(in.r)};
    const Stereo out{in.l * (1.0f - m) + wet.l * m, in.r * (1.0f - m) + wet.r * m};
    if (!combMix.isSmoothing() && combMix.getCurrentValue() <= 0.0f) {
        combL.reset(); combR.reset();
        combRunning = false;
    }
    return out;
}

// Spec S7.11: ENSEMBLE, the same Mix rule (0: not run, the input passes exactly).
LMWSynthProcessor::Stereo LMWSynthProcessor::ensembleSample(Stereo in) noexcept {
    if (!ensembleRunning && !ensembleMix.isSmoothing() && ensembleMix.getCurrentValue() <= 0.0f) return in;
    ensembleRunning = true;
    const float m = ensembleMix.getNextValue();
    Stereo wet;
    ensembleFx.process(in.l, in.r, wet.l, wet.r);
    const Stereo out{in.l * (1.0f - m) + wet.l * m, in.r * (1.0f - m) + wet.r * m};
    if (!ensembleMix.isSmoothing() && ensembleMix.getCurrentValue() <= 0.0f) {
        ensembleFx.reset();
        ensembleRunning = false;
    }
    return out;
}

// Spec S7.8: DELAY, the same Mix rule.
LMWSynthProcessor::Stereo LMWSynthProcessor::delaySample(Stereo in) noexcept {
    if (!delayRunning && !delayMix.isSmoothing() && delayMix.getCurrentValue() <= 0.0f) return in;
    delayRunning = true;
    const float m = delayMix.getNextValue();
    Stereo wet;
    delayFx.process(in.l, in.r, wet.l, wet.r);
    const Stereo out{in.l * (1.0f - m) + wet.l * m, in.r * (1.0f - m) + wet.r * m};
    if (!delayMix.isSmoothing() && delayMix.getCurrentValue() <= 0.0f) {
        delayFx.reset();
        delayRunning = false;
    }
    return out;
}

// Spec S7.4, S7.6, S7.8, S7.11: the effects rack. Effect inputs read out.out (the synth after OUT), comb.out,
// ensemble.out, delay.out and reverb.out: their normalled sources (out.out -> COMB -> ENSEMBLE -> DELAY -> REVERB
// -> main.in) or their cables. Per sample in the order COMB, ENSEMBLE, DELAY, REVERB, main; a source later in that
// order is its previous sample's value.
void LMWSynthProcessor::processEffects(float* left, float* right, int n) noexcept {
    using lmw::dsp::In;
    using lmw::dsp::Out;
    reverbMix.setTargetValue(raw.revMix->load());
    combMix.setTargetValue(raw.combMix->load());
    delayMix.setTargetValue(raw.delayMix->load());
    ensembleMix.setTargetValue(raw.ensembleMix->load());
    ensembleFx.setParameters(raw.ensembleDepth->load(), raw.ensembleRate->load(), raw.ensembleShimmer->load());
    delayFx.setParameters(raw.delayTime->load(), raw.delayFeedback->load(), raw.delayDamping->load(), raw.delaySpread->load());
    if (reverbRunning || reverbMix.isSmoothing() || reverbMix.getTargetValue() > 0.0f) {
        reverb.setParameters(reverbParameters());
        // Spec S7.4 (2026-11-26). Size: Freeverb's combs feed back g = 0.7 + 0.28 x Size, so a held note builds up
        // up to 1 / (1 - g^2) in energy (in practice about half that in dB, measured: +3 dB at 95 %): above Size 50 %
        // the wet level is scaled down by the fourth root of that ratio (never up), so a
        // cathedral is as loud as a hall instead of overloading. Damping: a one-pole lowpass on the wet signal,
        // 20 kHz at 0 down to 1.2 kHz at 100 % (cutoff = 20 kHz x 0.06^(Damping^2)), on top of Freeverb's own.
        const double g = 0.7 + 0.28 * static_cast<double>(raw.revSize->load()), gRef = 0.84;
        reverbSizeGain = static_cast<float>(std::min(1.0, std::pow((1.0 - g * g) / (1.0 - gRef * gRef), 0.25)));   // measured: half the theory in dB
        const double d = static_cast<double>(raw.revDamping->load());
        const double fc = 20000.0 * std::pow(0.06, d * d);
        reverbLpCoeff = static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * std::min(fc, 0.45 * getSampleRate()) / getSampleRate()));
    }
    {   // COMB frequency with key tracking (newest note), feedback, damping
        const double hz = static_cast<double>(raw.combFreq->load())
                        * std::exp2(static_cast<double>(raw.combKey->load() * engine.newestKeyOctaves()));
        for (auto* c : {&combL, &combR}) c->setParameters(hz, raw.combFeedback->load(), raw.combDamping->load());
    }
    const auto& patch = engine.getPatch();
    struct Source { Out out; float level; };
    struct Sources { std::array<Source, lmw::dsp::Patch::maxPins> s{}; int n = 0; bool plain = false; };
    const auto sourcesOf = [&patch](In in) {
        Sources r;
        const int k = patch.pinCount(in);
        if (k == 0) {                                 // normalled: one source at full level
            r.s[0] = {lmw::dsp::normalled[static_cast<std::size_t>(in)], 1.0f}; r.n = 1; r.plain = true;
        } else {
            for (int i = 0; i < k; ++i) r.s[static_cast<std::size_t>(i)] = {static_cast<Out>(patch.pin(in, i).source), patch.pin(in, i).level};
            r.n = k;
        }
        return r;
    };
    const Sources comb = sourcesOf(In::CombIn), ens = sourcesOf(In::EnsembleIn), dly = sourcesOf(In::DelayIn), rev = sourcesOf(In::ReverbIn),
                  mainIn = sourcesOf(In::MainIn);
    const bool stereo = right != nullptr;
    for (int i = 0; i < n; ++i) {
        const Stereo synth{left[i], stereo ? right[i] : left[i]};
        Stereo combOut = prevComb, ensOut = prevEnsemble, delayOut = prevDelay, revOut = prevReverb;   // until computed this sample
        const auto value = [&](Out o) {
            switch (o) {
                case Out::OutOut:    return synth;
                case Out::CombOut:   return combOut;
                case Out::EnsembleOut: return ensOut;
                case Out::DelayOut:  return delayOut;
                case Out::ReverbOut: return revOut;
                default:             return Stereo{};
            }
        };
        const auto sum = [&](const Sources& src) {
            if (src.plain) return value(src.s[0].out);   // exactly the source (default: bit-identical)
            Stereo t;
            for (int k = 0; k < src.n; ++k) {
                const auto v = value(src.s[static_cast<std::size_t>(k)].out);
                t.l += v.l * src.s[static_cast<std::size_t>(k)].level;
                t.r += v.r * src.s[static_cast<std::size_t>(k)].level;
            }
            return t;
        };
        combOut = combSample(sum(comb));
        ensOut = ensembleSample(sum(ens));
        delayOut = delaySample(sum(dly));
        revOut = reverbSample(sum(rev), stereo);
        const Stereo out = sum(mainIn);
        left[i] = lmw::dsp::MainLimiter::process(out.l);   // Spec S7.4 (2026-11-26): no clipping after the effects
        if (stereo) right[i] = lmw::dsp::MainLimiter::process(out.r);
        prevComb = combOut;
        prevEnsemble = ensOut;
        prevDelay = delayOut;
        prevReverb = revOut;
    }
}

bool LMWSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void LMWSynthProcessor::pullParameters() noexcept {
    lmw::dsp::VoiceParams p;
    for (std::size_t n = 0; n < 3; ++n) p.waves[n] = static_cast<lmw::dsp::Waveform>(static_cast<int>(raw.wave[n]->load()));
    p.attack      = raw.e1a->load(); p.decay      = raw.e1d->load();
    p.sustain     = raw.e1s->load(); p.release    = raw.e1r->load();
    p.env2Attack  = raw.e2a->load(); p.env2Decay  = raw.e2d->load();
    p.env2Sustain = raw.e2s->load(); p.env2Release = raw.e2r->load();
    engine.setParams(p);

    lmw::dsp::ModParams m;
    for (std::size_t n = 0; n < 3; ++n) {
        m.fmSemitones[n] = raw.fm[n]->load();
        m.pmIndex[n]     = raw.pm[n]->load();        // Spec S5.20
        m.octave[n]      = static_cast<int>(raw.octave[n]->load()) - 2;    // choice index 0..4 = -2..+2
        m.tune[n]        = raw.tune[n]->load();
        m.mixVco[n]      = raw.mixVco[n]->load();
    }
    m.mixNoise    = raw.mixNoise->load();
    const auto slope = [this](std::size_t n) { return static_cast<lmw::dsp::FilterSlope>(static_cast<int>(raw.vcfSlope[n]->load())); };
    const auto type  = [this](std::size_t n) { return static_cast<lmw::dsp::FilterType>(static_cast<int>(raw.vcfType[n]->load())); };
    m.cutoff      = raw.cutoff[0]->load();   m.resonance     = raw.resonance[0]->load();
    m.envOctaves  = raw.vcfEnv[0]->load();   m.vcfSlope      = slope(0);   m.vcfType = type(0);
    m.vcf2Cutoff  = raw.cutoff[1]->load();   m.vcf2Resonance = raw.resonance[1]->load();
    m.vcf2EnvOctaves = raw.vcfEnv[1]->load(); m.vcf2Slope    = slope(1);   m.vcf2Type = type(1);
    m.keyTrack    = raw.vcfKey[0]->load();   m.vcf2KeyTrack  = raw.vcfKey[1]->load();
    m.formantVowel = raw.formantVowel->load();      // Spec S5.16
    m.formantVoice = static_cast<lmw::dsp::FormantVoice>(static_cast<int>(raw.formantVoice->load()));
    m.formantRes   = raw.formantRes->load();
    m.formantMix   = raw.formantMix->load();
    m.vcfRouting   = vcfRouting();                    // Spec S5.17
    m.vcaInitial  = raw.vcaInitial->load();
    m.lfoRate     = raw.lfoRate->load();
    m.lfoWave     = static_cast<lmw::dsp::LfoWave>(static_cast<int>(raw.lfoWave->load()));
    m.lfo2Rate    = raw.lfo2Rate->load();
    m.lfo2Wave    = static_cast<lmw::dsp::LfoWave>(static_cast<int>(raw.lfo2Wave->load()));
    m.lfoDelay    = {raw.lfoDelay->load(), raw.lfo2Delay->load()};          // Spec S5.24
    m.pan         = raw.outPan->load();                                      // Spec S5.25
    m.spread      = raw.outSpread->load();
    m.spreadMode  = static_cast<lmw::dsp::SpreadMode>(static_cast<int>(raw.spreadMode->load()));
    for (std::size_t k = 0; k < 3; ++k) { m.mix2Level[k] = raw.mix2Level[k]->load(); m.mix2Pan[k] = raw.mix2Pan[k]->load(); }   // S5.26
    m.tremolo     = raw.tremolo->load();
    m.vcaVelocity = raw.vcaVelocity->load();
    m.noiseColour = static_cast<lmw::dsp::NoiseColour>(static_cast<int>(raw.noiseColour->load()));
    m.randRate    = raw.noiseRate->load();
    m.randSmooth  = raw.noiseSmooth->load();
    engine.setModParams(m);

    engine.setGain(raw.gain->load());
    engine.setVoiceCount(static_cast<int>(raw.voices->load()));
}

void LMWSynthProcessor::applyNoteEvent(const lmw::dsp::NoteEvent& e) noexcept {
    using K = lmw::dsp::NoteEvent::Kind;
    if (e.kind == K::On)       engine.noteOn(e.note, e.velocity);
    else if (e.kind == K::Off) engine.noteOff(e.note);
    else                       engine.allNotesOff();
}

// External MIDI (host / MIDI device), audio thread: play it and queue notes for the keyboard display.
// Raw bytes are decoded in place (no MidiMessage copy, which allocates for SysEx) — Spec S10.2.
void LMWSynthProcessor::handleMidi(const juce::uint8* data, int size) noexcept {
    using M = lmw::dsp::MidiEvent::Kind;
    using K = lmw::dsp::NoteEvent::Kind;
    const auto m = lmw::dsp::parseMidi(data, size);
    lmw::dsp::NoteEvent e;
    e.channel = static_cast<unsigned char>(m.channel);
    e.note    = static_cast<unsigned char>(m.note);
    switch (m.kind) {
        case M::NoteOn:      e.kind = K::On; e.velocity = m.value; break;
        case M::NoteOff:     e.kind = K::Off; break;
        case M::AllNotesOff: e.kind = K::AllOff; break;
        case M::Sustain:     engine.setSustain(m.value > 0.5f); return;                       // Spec S3.3
        case M::PitchBend:   engine.setPitchBend(m.value * pitchBendRangeSemitones); return;   // Spec S3.4
        case M::ModWheel:                                                                       // Spec S3.5
            engine.setModWheel(m.value);
            modWheelValue.store(m.value, std::memory_order_relaxed);
            return;
        case M::None:        return;
    }
    applyNoteEvent(e);
    audioToGui.push(e);                              // dropped if the GUI is not draining (editor closed)
}

void LMWSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    pullParameters();

    lmw::dsp::PatchCommand pc;                           // cable changes (Spec S5.4), lock-free
    while (patchToAudio.pop(pc)) engine.apply(pc);

    lmw::dsp::NoteEvent e;                               // on-screen / QWERTY notes, lock-free
    while (guiToAudio.pop(e)) applyNoteEvent(e);

    float* left  = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    int pos = 0;                                         // sample-accurate MIDI
    for (const auto meta : midi) {
        const int at = juce::jlimit(pos, n, meta.samplePosition);
        engine.render(left + pos, right ? right + pos : nullptr, at - pos);
        pos = at;
        handleMidi(meta.data, meta.numBytes);
    }
    engine.render(left + pos, right ? right + pos : nullptr, n - pos);
    processEffects(left, right, n);                      // Spec S7.4, S7.6
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
}

namespace {
const juce::Identifier kPatchProp{"patch"};          // cables as text (Spec S6.6, PatchText.h)
const juce::Identifier kVersionProp{"stateVersion"}; // absent = version 1 (StateMigration.h)
}

void LMWSynthProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto copy = apvts.copyState();
    copy.setProperty(kPatchProp, juce::String(lmw::dsp::patchToText(guiPatch)), nullptr);
    copy.setProperty(kVersionProp, lmw::migration::currentStateVersion, nullptr);
    if (auto xml = copy.createXml()) copyXmlToBinary(*xml, dest);
}

void LMWSynthProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(apvts.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xml);
        auto text = tree.getProperty(kPatchProp).toString().toStdString();
        // Spec S5.10, S5.11: translate parameter IDs and cable names from older states.
        const int version = tree.getProperty(kVersionProp, 1);
        if (version < lmw::migration::currentStateVersion) {
            for (auto child : tree)
                if (child.hasProperty("id")) {
                    const auto id = child.getProperty("id").toString().toStdString();
                    const auto newId = lmw::migration::parameterId(version, id);
                    if (newId != id) child.setProperty("id", juce::String(newId), nullptr);
                    if (child.hasProperty("value")) {                // S5.23: choice indices that moved
                        const auto v = static_cast<float>(child.getProperty("value"));
                        const auto nv = lmw::migration::parameterValue(version, newId, v);
                        if (nv != v) child.setProperty("value", nv, nullptr);
                    }
                }
            text = lmw::migration::cables(version, text);
        }
        tree.removeProperty(kPatchProp, nullptr);
        tree.removeProperty(kVersionProp, nullptr);
        apvts.replaceState(tree);
        // An unreadable cable list is ignored (no cables) rather than failing the whole state.
        // Cable changes go through the message thread (single producer of the cable queue, Spec
        // S10.2); a host restoring state on another thread gets the cables a moment later.
        const auto cables = lmw::dsp::patchFromText(text).value_or(lmw::dsp::Patch{});
        if (juce::MessageManager::existsAndIsCurrentThread()) {
            setPatch(cables);
        } else {
            juce::MessageManager::callAsync([weak = juce::WeakReference<LMWSynthProcessor>(this), cables] {
                if (auto* p = weak.get()) p->setPatch(cables);
            });
        }
    }
}

juce::AudioProcessorEditor* LMWSynthProcessor::createEditor() { return new LMWSynthEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LMWSynthProcessor(); }

// ---- Spec S7: patch files; S10.10: the same value texts are used over 9P ----

namespace {
// The shortest decimal for which accept(that number as a float) holds, written without an exponent
// ("800", "0.01"; very large or small numbers keep it); empty if none is accepted. Tried: the value
// rounded to 1-9 significant digits, and rounded to 0-9 decimals (so a value snapped to -2e-07 by float
// arithmetic, as JUCE's step snapping does on macOS for a written 0, is still "0").
template <typename Accept>
std::string shortestNumber(float v, Accept accept) {
    char buf[48];
    std::string best;
    auto offer = [&](std::string text) {
        if (text.find_first_not_of("-0.") == std::string::npos) text = "0";   // "-0", "-0.00": 0
        if (best.empty() || text.size() < best.size()) best = text;
    };
    for (int decimals = 0; decimals <= 9; ++decimals) {
        std::snprintf(buf, sizeof buf, "%.*f", decimals, static_cast<double>(v));
        if (accept(static_cast<float>(std::strtod(buf, nullptr)))) { offer(buf); break; }
    }
    for (int digits = 1; digits <= 9; ++digits) {
        std::snprintf(buf, sizeof buf, "%.*e", digits - 1, static_cast<double>(v));
        const double candidate = std::strtod(buf, nullptr);
        if (!accept(static_cast<float>(candidate))) continue;
        const char* e = std::strchr(buf, 'e');
        const int exponent = e != nullptr ? std::atoi(e + 1) : 0;
        if (exponent >= -6 && exponent <= 15)
            std::snprintf(buf, sizeof buf, "%.*f", std::max(0, digits - 1 - exponent), candidate);
        offer(buf);
        break;
    }
    return best;
}
} // namespace

std::string LMWSynthProcessor::valueText(const juce::RangedAudioParameter& r, std::string_view written) {
    if (auto* c = dynamic_cast<const juce::AudioParameterChoice*>(&r)) return c->getCurrentChoiceName().toStdString();
    if (auto* i = dynamic_cast<const juce::AudioParameterInt*>(&r)) return std::to_string(i->get());
    if (auto* f = dynamic_cast<const juce::AudioParameterFloat*>(&r)) {
        // A text that, written back, stores this exact value. The value goes through the 0-1 range as a
        // 32-bit float, so many texts store the same value (1234.5678 and 1234.5673; 800 keeps 800.0001):
        // the text last written while it still stores this value, else the shortest one (Kukel 2026-11-17/18).
        const float stored = f->get();
        const auto& range = f->getNormalisableRange().getRange();
        auto accept = [&](float candidate) {   // in range, and stored as parseValue would
            return candidate >= range.getStart() && candidate <= range.getEnd()
                && f->convertFrom0to1(f->convertTo0to1(candidate)) == stored;
        };
        if (!written.empty()) {
            const std::string w(written);
            char* end = nullptr;
            const double number = std::strtod(w.c_str(), &end);
            if (end == w.c_str() + w.size() && w.find_first_not_of("0123456789+-.eE") == std::string::npos
                && accept(static_cast<float>(number)))
                return w;
        }
        auto text = shortestNumber(stored, accept);
        if (text.empty()) {
            char buf[40];
            std::snprintf(buf, sizeof buf, "%.7g", static_cast<double>(stored));
            text = buf;
        }
        return text;
    }
    return {};
}

namespace {
// JSON for /info. Floats as the shortest text that reads back as the same float, always with a decimal
// point (20.0, 0.01); text with " and \ escaped.
std::string jsonFloat(float v) {
    std::string s = shortestNumber(v, [v](float candidate) { return candidate == v; });
    if (s.find_first_of(".eEin") == std::string::npos) s += ".0";
    return s;
}
std::string jsonText(const juce::String& text) {
    std::string out = "\"";
    for (char c : text.toStdString()) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}
} // namespace

std::string LMWSynthProcessor::parameterInfo() const {
    std::string out = "[\n";
    bool first = true;
    for (auto* param : getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        std::string o = "{\"id\": " + jsonText(r->paramID) + ", \"name\": " + jsonText(r->name);
        const float def = r->convertFrom0to1(r->getDefaultValue());
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(r)) {
            o += ", \"kind\": \"choice\", \"choices\": [";
            for (int i = 0; i < c->choices.size(); ++i) o += (i > 0 ? ", " : "") + jsonText(c->choices[i]);
            o += "], \"default\": " + jsonText(c->choices[juce::roundToInt(def)]);
        } else if (auto* n = dynamic_cast<juce::AudioParameterInt*>(r)) {
            o += ", \"kind\": \"int\", \"min\": " + std::to_string(n->getRange().getStart())
               + ", \"max\": " + std::to_string(n->getRange().getEnd()) + ", \"default\": " + std::to_string(juce::roundToInt(def));
        } else {
            const auto& range = r->getNormalisableRange();
            const auto it = floatInfo.find(r->paramID.toStdString());
            const FloatInfo fi = it != floatInfo.end() ? it->second : FloatInfo{"", def, std::nullopt};
            o += ", \"kind\": \"float\", \"min\": " + jsonFloat(range.start) + ", \"max\": " + jsonFloat(range.end)
               + ", \"default\": " + jsonFloat(fi.def) + ", \"unit\": " + jsonText(fi.unit);
            if (fi.centre) o += ", \"centre\": " + jsonFloat(*fi.centre);
            if (range.interval > 0.0f) o += ", \"step\": " + jsonFloat(range.interval);
            o += ", \"scale\": " + jsonText(std::string(lmw::ui::scale::word(            // Spec S6.17
                     lmw::ui::scale::kindOf(fi.unit, range.start, range.end, fi.centre))));
        }
        out += (first ? " " : ",\n ") + o + "}";
        first = false;
    }
    return out + "\n]\n";
}

std::optional<LMWSynthProcessor::Scale> LMWSynthProcessor::scaleOf(const std::string& id) const {
    const auto it = floatInfo.find(id);
    auto* f = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id));
    if (it == floatInfo.end() || f == nullptr) return std::nullopt;
    const auto& r = f->range;
    std::optional<double> c;
    if (it->second.centre) c = *it->second.centre;
    return Scale{lmw::ui::scale::kindOf(it->second.unit, r.start, r.end, c), it->second.unit, r.start, r.end, it->second.centre};
}

std::optional<float> LMWSynthProcessor::parseValue(const juce::RangedAudioParameter& r, const juce::String& text) {
    const auto v = text.trim();
    if (auto* c = dynamic_cast<const juce::AudioParameterChoice*>(&r)) {
        std::vector<std::string> names;
        for (const auto& n : c->choices) names.push_back(n.toStdString());
        const int index = lmw::ui::text::matchChoice(names, v.toStdString());
        if (index >= 0) return r.convertTo0to1(static_cast<float>(index));
        return std::nullopt;
    }
    if (v.isEmpty() || !v.containsOnly("0123456789+-.eE")) return std::nullopt;
    const float real = static_cast<float>(v.getDoubleValue());
    return r.convertTo0to1(r.getNormalisableRange().getRange().clipValue(real));
}

std::string LMWSynthProcessor::currentText(const juce::RangedAudioParameter& r) const {
    std::string written;
    {
        const std::lock_guard lock(writtenMutex);
        if (const auto it = writtenTexts.find(r.paramID.toStdString()); it != writtenTexts.end()) written = it->second;
    }
    return valueText(r, written);
}

void LMWSynthProcessor::rememberText(const juce::RangedAudioParameter& r, std::string_view text) {
    if (dynamic_cast<const juce::AudioParameterFloat*>(&r) == nullptr) return;
    const auto trimmedText = juce::String::fromUTF8(text.data(), static_cast<int>(text.size())).trim().toStdString();
    const std::lock_guard lock(writtenMutex);
    writtenTexts[r.paramID.toStdString()] = trimmedText;
}

void LMWSynthProcessor::setNormalised(juce::RangedAudioParameter& r, float normalised) {
    r.beginChangeGesture();
    r.setValueNotifyingHost(normalised);
    r.endChangeGesture();
}

juce::String LMWSynthProcessor::patchText(const juce::String& name) const {
    lmw::patchfile::Patch p;
    p.stateVersion = lmw::migration::currentStateVersion;
    p.name = name.toStdString();
    for (auto* param : getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        lmw::patchfile::Parameter out{r->paramID.toStdString(), currentText(*r), {}};
        if (dynamic_cast<juce::AudioParameterFloat*>(r) != nullptr)
            out.comment = r->getCurrentValueAsText().toStdString();                   // with its unit
        p.parameters.push_back(std::move(out));
    }
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        if (!guiPatch.isPatched(in)) continue;
        p.cables.emplace_back(std::string(lmw::dsp::inputNames[static_cast<std::size_t>(i)]),
                              lmw::dsp::pinsToText(guiPatch, in));                    // Spec S5.14: pins with levels
    }
    // Spec S6.7: moved schematic blocks, one line each; nothing while the drawing is the default.
    if (const auto l = lmw::ui::schematic::Layout::parse(schematicLayout().toStdString()); l && !l->isDefault())
        for (std::size_t i = 0; i < lmw::ui::schematic::blocks().size(); ++i) {
            const auto at = l->position(i);
            p.schematic.emplace_back(lmw::ui::schematic::blockId(lmw::ui::schematic::blocks()[i].name),
                                     std::to_string(static_cast<int>(at.x)) + ", " + std::to_string(static_cast<int>(at.y)));
        }
    p.notes = notes().toStdString();
    return juce::String::fromUTF8(lmw::patchfile::write(p).c_str());
}

bool LMWSynthProcessor::loadPatchText(const juce::String& text, juce::String& error, bool soundOnly) {
    const auto p = lmw::patchfile::parse(text.toStdString());
    if (!p) {
        error = "Not an LMW-Synth patch file (or made by a newer version).";
        return false;
    }
    // Names from older versions are translated like a saved state (StateMigration.h).
    const int version = p->stateVersion > 0 ? p->stateVersion : lmw::migration::currentStateVersion;
    std::string cableText;
    for (const auto& [in, from] : p->cables) {
        if (!cableText.empty()) cableText += ';';
        cableText += in + "=" + from;
    }
    std::map<std::string, std::string> values;
    for (const auto& prm : p->parameters) values[lmw::migration::parameterId(version, prm.id)] = prm.value;
    if (!applySound(juce::String::fromUTF8(p->name.c_str()), values, lmw::migration::cables(version, cableText), error))
        return false;
    // Spec S6.7: the schematic as saved; without a [schematic] section (or an unreadable one) the default. An own
    // instrument preset (soundOnly) restores it only when its file has one (user 2026-11-24), else it stays.
    if (!soundOnly || !p->schematic.empty()) {
        std::string layout;
        for (const auto& [id, at] : p->schematic) layout += id + ":" + at + ";";
        const auto l = lmw::ui::schematic::Layout::parse(layout);
        setSchematicLayout(l && !l->isDefault() ? juce::String(l->serialize()) : juce::String());
    }
    if (!soundOnly) setNotes(juce::String::fromUTF8(p->notes.c_str()));
    return true;
}

bool LMWSynthProcessor::applySound(const juce::String& name, const std::map<std::string, std::string>& values,
                                   const std::string& cablesText, juce::String& error) {
    const auto cables = lmw::dsp::patchFromText(cablesText);
    if (!cables) {
        error = "The cables name an unknown socket or one input twice.";
        return false;
    }
    for (auto* param : getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        float normalised = r->getDefaultValue();                 // missing or unreadable: default
        if (const auto it = values.find(r->paramID.toStdString()); it != values.end()) {
            normalised = parseValue(*r, juce::String::fromUTF8(it->second.c_str())).value_or(normalised);
            rememberText(*r, it->second);       // read back as written (S7.1)
        }
        setNormalised(*r, normalised);
    }
    setPatch(*cables);
    setPatchName(name);
    clearPresetRecords();                    // Spec S7.5: the panels show what the sound matches
    recordSound();                           // Spec S7.7: "*" once it changes
    ++loadedPatches;
    return true;
}

// Every parameter's value (normalised, exact) and the cables: equal exactly when the sound is the same.
std::string LMWSynthProcessor::soundSignature() const {
    std::string s;
    char buf[24];
    for (auto* param : getParameters()) {
        if (dynamic_cast<const juce::RangedAudioParameter*>(param) == nullptr) continue;
        std::snprintf(buf, sizeof buf, "%.9g;", static_cast<double>(param->getValue()));
        s += buf;
    }
    return s + "|" + lmw::dsp::patchToText(guiPatch);
}

// Spec S6.14: the sound as keys and texts for the undo history.
lmw::ui::History::State LMWSynthProcessor::soundState() const {
    lmw::ui::History::State s;
    char buf[24];
    for (auto* param : getParameters()) {
        auto* r = dynamic_cast<const juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(r->getValue()));
        s["p:" + r->paramID.toStdString()] = buf;
    }
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        if (guiPatch.isPatched(in)) s["c:" + std::string(lmw::dsp::inputNames[static_cast<std::size_t>(i)])] = lmw::dsp::pinsToText(guiPatch, in);
    }
    s["schematic"] = schematicLayout().toStdString();
    s["name"] = patchName().toStdString();
    s["notes"] = notes().toStdString();
    for (int i = 0; i < apvts.state.getNumProperties(); ++i) {
        const auto id = apvts.state.getPropertyName(i).toString();
        if (id.startsWith("preset_") || id.startsWith("presetValues_") || id == soundProp.toString())
            s["s:" + id.toStdString()] = apvts.state.getProperty(id).toString().toStdString();
    }
    return s;
}

namespace {
// Puts back the texts of `changes` (keys from soundState()).
void applyState(LMWSynthProcessor& p, const lmw::ui::History::State& changes) {
    for (const auto& [key, text] : changes) {
        if (key.rfind("p:", 0) == 0) {
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p.state().getParameter(juce::String(key.substr(2)))))
                LMWSynthProcessor::setNormalised(*r, static_cast<float>(std::strtod(text.c_str(), nullptr)));
        } else if (key.rfind("c:", 0) == 0) {
            const auto in = lmw::dsp::inputByName(key.substr(2));
            if (!in) continue;
            p.requestPatch(*in, lmw::dsp::Patch::none);
            if (const auto pins = text.empty() ? std::optional<std::vector<lmw::dsp::Patch::Pin>>() : lmw::dsp::pinsFromText(text))
                for (const auto& pin : *pins) p.requestPatch(*in, pin.source, pin.level);
        } else if (key == "schematic") {
            p.setSchematicLayout(juce::String::fromUTF8(text.c_str()));
        } else if (key == "name") {
            p.setPatchName(juce::String::fromUTF8(text.c_str()));
        } else if (key == "notes") {
            p.setNotes(juce::String::fromUTF8(text.c_str()));
        } else if (key.rfind("s:", 0) == 0) {
            const juce::Identifier id(juce::String(key.substr(2)));
            if (text.empty()) p.state().state.removeProperty(id, nullptr);
            else p.state().state.setProperty(id, juce::String::fromUTF8(text.c_str()), nullptr);
        }
    }
}
} // namespace

bool LMWSynthProcessor::undo() {
    const auto step = history.undo();
    if (!step) return false;
    const juce::ScopedValueSetter<bool> applying(applyingHistory, true);
    applyState(*this, step->before);
    return true;
}

bool LMWSynthProcessor::redo() {
    const auto step = history.redo();
    if (!step) return false;
    const juce::ScopedValueSetter<bool> applying(applyingHistory, true);
    applyState(*this, step->after);
    return true;
}

void LMWSynthProcessor::parameterGestureChanged(int index, bool starting) {
    if (applyingHistory) return;
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm == nullptr || !mm->isThisTheMessageThread()) return;     // the message thread only (S6.14)
    if (!starting) { endStep(); return; }
    const auto& params = getParameters();
    beginStep(index >= 0 && index < params.size() ? params[index]->getName(100).toStdString() : std::string("change"));
}
