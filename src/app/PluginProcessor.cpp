// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
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
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{P::voices, 1}, "Voices", 1, lmw::dsp::Engine::maxVoices, 8),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::wave, 1}, "VCO Wave",
              juce::StringArray{"Sine", "Saw", "Square", "Triangle"}, 1),
          f(P::fm,        "VCO FM",        Range(0.0f, 12.0f), 0.0f, semis),
          f(P::mixVco,    "Mixer VCO",     Range(0.0f, 1.0f), 1.0f, percent),
          f(P::mixNoise,  "Mixer Noise",   Range(0.0f, 1.0f), 0.0f, percent),
          f(P::cutoff,    "VCF Cutoff",    logRange(20.0f, 18000.0f, 600.0f), 18000.0f, hertz),
          f(P::resonance, "VCF Resonance", Range(0.0f, 1.0f), 0.0f, percent),
          f(P::vcfEnv,    "VCF Env",       Range(-5.0f, 5.0f), 0.0f, octaves),
          f(P::vcaInitial, "VCA Initial",  Range(0.0f, 1.0f), 0.0f, percent));
    envelope(l, P::env1Attack, P::env1Decay, P::env1Sustain, P::env1Release, "Env1", 0.01f, 0.2f, 0.7f, 0.3f);
    envelope(l, P::env2Attack, P::env2Decay, P::env2Sustain, P::env2Release, "Env2", 0.01f, 0.3f, 0.0f, 0.3f);
    l.add(f(P::lfoRate, "LFO Rate", logRange(0.05f, 20.0f, 2.0f), 5.0f, hertz),
          std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{P::lfoWave, 1}, "LFO Wave",
              juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0),
          f(P::gain, "Volume", Range(0.0f, 1.0f), 0.8f, decibels));
    return l;
}

LMWSynthProcessor::LMWSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LMWSynth", createLayout()) {
    namespace P = lmw::param;
    auto get = [this](const char* id) { auto* p = apvts.getRawParameterValue(id); jassert(p != nullptr); return p; };
    raw = {get(P::voices), get(P::wave), get(P::fm), get(P::mixVco), get(P::mixNoise), get(P::cutoff),
           get(P::resonance), get(P::vcfEnv), get(P::vcaInitial), get(P::env1Attack), get(P::env1Decay), get(P::env1Sustain),
           get(P::env1Release), get(P::env2Attack), get(P::env2Decay), get(P::env2Sustain), get(P::env2Release),
           get(P::lfoRate), get(P::lfoWave), get(P::gain)};
    keyboard.addListener(this);
}

LMWSynthProcessor::~LMWSynthProcessor() { keyboard.removeListener(this); }

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

void LMWSynthProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);
    pullParameters();
}

bool LMWSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void LMWSynthProcessor::pullParameters() noexcept {
    lmw::dsp::VoiceParams p;
    p.wave        = static_cast<lmw::dsp::Waveform>(static_cast<int>(raw.wave->load()));
    p.attack      = raw.e1a->load(); p.decay      = raw.e1d->load();
    p.sustain     = raw.e1s->load(); p.release    = raw.e1r->load();
    p.env2Attack  = raw.e2a->load(); p.env2Decay  = raw.e2d->load();
    p.env2Sustain = raw.e2s->load(); p.env2Release = raw.e2r->load();
    engine.setParams(p);

    lmw::dsp::ModParams m;
    m.fmSemitones = raw.fm->load();
    m.mixVco      = raw.mixVco->load();   m.mixNoise   = raw.mixNoise->load();
    m.cutoff      = raw.cutoff->load();   m.resonance  = raw.resonance->load();
    m.envOctaves  = raw.vcfEnv->load();
    m.vcaInitial  = raw.vcaInitial->load();
    m.lfoRate     = raw.lfoRate->load();
    m.lfoWave     = static_cast<lmw::dsp::LfoWave>(static_cast<int>(raw.lfoWave->load()));
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
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
}

void LMWSynthProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void LMWSynthProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* LMWSynthProcessor::createEditor() { return new LMWSynthEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LMWSynthProcessor(); }
