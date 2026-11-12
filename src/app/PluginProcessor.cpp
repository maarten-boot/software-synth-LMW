// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "dsp/PatchText.h"
#include "app/StateMigration.h"
#include "app/PatchFile.h"
#include "app/NinePService.h"
#include "ui/Config.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include "PluginEditor.h"
#include "ui/ParamText.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout LMWSynthProcessor::createLayout() {
    namespace P = lmw::param;
    namespace tx = lmw::ui::text;
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;
    auto time = [](float maxSec) { Range r(0.0f, maxSec); r.setSkewForCentre(maxSec / 10.0f); return r; };
    auto logRange = [](float lo, float hi, float centre) { Range r(lo, hi); r.setSkewForCentre(centre); return r; };
    auto attr = [](auto toText, auto fromText) {   // value text with units (Spec S6.1); no separate host label
        return Attr()
            .withStringFromValueFunction([toText](float v, int) { return juce::String(toText(v)); })
            .withValueFromStringFunction([fromText](const juce::String& s) { return fromText(s.toStdString()); });
    };
    const auto seconds  = attr(tx::time, tx::parseTime);
    const auto percent  = attr(tx::percent, tx::parsePercent);
    const auto decibels = attr(tx::decibels, tx::parseDecibels);
    const auto hertz    = attr(tx::hertz, tx::parseHertz);
    const auto octaves  = attr(tx::octaves, tx::parseNumber);
    const auto semis    = attr(tx::semitones, tx::parseNumber);
    auto f = [](const char* id, const char* name, Range r, float def, const Attr& a) {
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, r, def, a);
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
                  juce::StringArray{"Sine", "Saw", "Square", "Triangle"}, 1),
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcoOctave[n], 1}, name + " Octave",
                  juce::StringArray{"-2", "-1", "0", "+1", "+2"}, 2),
              f(P::vcoTune[n], (name + " Tune").toRawUTF8(), tune, 0.0f, semis),
              f(P::vcoFm[n],   (name + " FM").toRawUTF8(), Range(0.0f, 12.0f), 0.0f, semis),
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
                  juce::StringArray{"24 dB", "12 dB", "6 dB"}, 0),
              std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::vcfType[n], 1}, name + " Type",
                  juce::StringArray{"Lowpass", "Highpass", "Bandpass", "Notch"}, 0));
    }
    l.add(f(P::vcaInitial, "VCA Initial",  Range(0.0f, 1.0f), 0.0f, percent),
          f(P::vcaTremolo, "VCA Tremolo",  Range(0.0f, 1.0f), 0.0f, percent));   // Spec S5.12
    envelope(l, P::env1Attack, P::env1Decay, P::env1Sustain, P::env1Release, "Env1", 0.01f, 0.2f, 0.7f, 0.3f);
    envelope(l, P::env2Attack, P::env2Decay, P::env2Sustain, P::env2Release, "Env2", 0.01f, 0.3f, 0.0f, 0.3f);
    // Spec S5.12: LFO 1 (vibrato) and LFO 2 (tremolo).
    l.add(f(P::lfoRate, "LFO 1 Rate", logRange(0.05f, 20.0f, 2.0f), 5.0f, hertz),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::lfoWave, 1}, "LFO 1 Wave",
              juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0),
          f(P::lfo2Rate, "LFO 2 Rate", logRange(0.05f, 20.0f, 2.0f), 4.0f, hertz),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::lfo2Wave, 1}, "LFO 2 Wave",
              juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0),
          f(P::gain, "Volume", Range(0.0f, 1.0f), 0.8f, decibels));
    // Spec S7.4: reverb on the Effects tab. Mix 0 = off (bypassed exactly).
    l.add(f(P::reverbMix,     "Reverb Mix",     Range(0.0f, 1.0f), 0.0f, percent),
          f(P::reverbSize,    "Reverb Size",    Range(0.0f, 1.0f), 0.5f, percent),
          f(P::reverbDamping, "Reverb Damping", Range(0.0f, 1.0f), 0.5f, percent),
          f(P::reverbWidth,   "Reverb Width",   Range(0.0f, 1.0f), 1.0f, percent));
    return l;
}

LMWSynthProcessor::LMWSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LMWSynth", createLayout()) {
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
    raw.lfo2Rate = get(P::lfo2Rate); raw.lfo2Wave = get(P::lfo2Wave); raw.tremolo = get(P::vcaTremolo);
    raw.revMix = get(P::reverbMix); raw.revSize = get(P::reverbSize);
    raw.revDamping = get(P::reverbDamping); raw.revWidth = get(P::reverbWidth);
    raw.noiseColour = get(P::noiseColour); raw.noiseRate = get(P::noiseRate); raw.noiseSmooth = get(P::noiseSmooth);
    for (std::size_t n = 0; n < 3; ++n) {
        raw.wave[n] = get(P::vcoWave[n]); raw.fm[n] = get(P::vcoFm[n]); raw.octave[n] = get(P::vcoOctave[n]);
        raw.tune[n] = get(P::vcoTune[n]); raw.mixVco[n] = get(P::mixVco[n]);
    }
    keyboard.addListener(this);

    // Spec S10: 9P server — a Unix socket per instance, TCP when ninep_tcp is set in the config file.
    ninep = std::make_unique<NinePService>(*this);
    const auto settings = lmw::ui::parseConfig(LMWSynthEditor::configFile().loadFileAsString().toStdString());
    ninepAddress = ninep->start(settings.tcpEndpoint());
    std::fprintf(stderr, "LMW-Synth 9P: %s\n", ninepAddress.empty() ? "(not available)" : ninepAddress.c_str());
}

LMWSynthProcessor::~LMWSynthProcessor() {
    ninep.reset();                                   // stops the 9P threads before anything else goes
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
    const auto chunk = static_cast<std::size_t>(std::max(samplesPerBlock, 512));   // larger blocks: in chunks
    wetLeft.assign(chunk, 0.0f);
    wetRight.assign(chunk, 0.0f);
    reverbMix.reset(sampleRate, 0.02);
    reverbMix.setCurrentAndTargetValue(raw.revMix->load());
    reverbRunning = false;
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
void LMWSynthProcessor::applyReverb(float* left, float* right, int n) noexcept {
    reverbMix.setTargetValue(raw.revMix->load());
    if (!reverbRunning && !reverbMix.isSmoothing() && reverbMix.getCurrentValue() <= 0.0f) return;
    reverbRunning = true;
    reverb.setParameters(reverbParameters());
    const int chunk = static_cast<int>(wetLeft.size());
    for (int pos = 0; pos < n; pos += chunk) {
        const int k = std::min(chunk, n - pos);
        float* l = left + pos;
        float* r = right != nullptr ? right + pos : nullptr;
        std::copy(l, l + k, wetLeft.data());
        if (r != nullptr) {
            std::copy(r, r + k, wetRight.data());
            reverb.processStereo(wetLeft.data(), wetRight.data(), k);
        } else {
            reverb.processMono(wetLeft.data(), k);
        }
        for (int i = 0; i < k; ++i) {
            const float m = reverbMix.getNextValue();
            const auto ui = static_cast<std::size_t>(i);
            l[i] = l[i] * (1.0f - m) + wetLeft[ui] * m;
            if (r != nullptr) r[i] = r[i] * (1.0f - m) + wetRight[ui] * m;
        }
    }
    if (!reverbMix.isSmoothing() && reverbMix.getCurrentValue() <= 0.0f) {
        reverb.reset();                            // RT-safe: clears the delay lines
        reverbRunning = false;
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
    m.vcaInitial  = raw.vcaInitial->load();
    m.lfoRate     = raw.lfoRate->load();
    m.lfoWave     = static_cast<lmw::dsp::LfoWave>(static_cast<int>(raw.lfoWave->load()));
    m.lfo2Rate    = raw.lfo2Rate->load();
    m.lfo2Wave    = static_cast<lmw::dsp::LfoWave>(static_cast<int>(raw.lfo2Wave->load()));
    m.tremolo     = raw.tremolo->load();
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
    applyReverb(left, right, n);                         // Spec S7.4
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

std::string LMWSynthProcessor::valueText(const juce::RangedAudioParameter& r) {
    if (auto* c = dynamic_cast<const juce::AudioParameterChoice*>(&r)) return c->getCurrentChoiceName().toStdString();
    if (auto* i = dynamic_cast<const juce::AudioParameterInt*>(&r)) return std::to_string(i->get());
    if (auto* f = dynamic_cast<const juce::AudioParameterFloat*>(&r)) {
        char buf[40];
        std::snprintf(buf, sizeof buf, "%.7g", static_cast<double>(f->get()));   // ~float precision: 1000 stays "1000"
        return buf;
    }
    return {};
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
        lmw::patchfile::Parameter out{r->paramID.toStdString(), valueText(*r), {}};
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
    p.notes = notes().toStdString();
    return juce::String::fromUTF8(lmw::patchfile::write(p).c_str());
}

bool LMWSynthProcessor::loadPatchText(const juce::String& text, juce::String& error) {
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
    const auto cables = lmw::dsp::patchFromText(lmw::migration::cables(version, cableText));
    if (!cables) {
        error = "The [cables] section names an unknown socket or one input twice.";
        return false;
    }
    std::map<std::string, std::string> values;
    for (const auto& prm : p->parameters) values[lmw::migration::parameterId(version, prm.id)] = prm.value;

    for (auto* param : getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        float normalised = r->getDefaultValue();                 // missing or unreadable: default
        if (const auto it = values.find(r->paramID.toStdString()); it != values.end())
            normalised = parseValue(*r, juce::String::fromUTF8(it->second.c_str())).value_or(normalised);
        setNormalised(*r, normalised);
    }
    setPatch(*cables);
    setNotes(juce::String::fromUTF8(p->notes.c_str()));
    setPatchName(juce::String::fromUTF8(p->name.c_str()));
    ++loadedPatches;
    return true;
}
