// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/ParamText.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout LMWSynthProcessor::createLayout() {
    auto time = [](float maxSec) {
        juce::NormalisableRange<float> r(0.0f, maxSec, 0.0f);
        r.setSkewForCentre(maxSec / 10.0f);
        return r;
    };
    // Value text with units (Spec S6.1); units are in the text, so no separate host label.
    using Attr = juce::AudioParameterFloatAttributes;
    namespace tx = lmw::ui::text;
    const auto seconds = Attr()
        .withStringFromValueFunction([](float v, int) { return juce::String(tx::time(v)); })
        .withValueFromStringFunction([](const juce::String& s) { return tx::parseTime(s.toStdString()); });
    const auto percent = Attr()
        .withStringFromValueFunction([](float v, int) { return juce::String(tx::percent(v)); })
        .withValueFromStringFunction([](const juce::String& s) { return tx::parsePercent(s.toStdString()); });
    const auto decibels = Attr()
        .withStringFromValueFunction([](float v, int) { return juce::String(tx::decibels(v)); })
        .withValueFromStringFunction([](const juce::String& s) { return tx::parseDecibels(s.toStdString()); });
    return {
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{lmw::param::wave, 1}, "VCO Wave",
            juce::StringArray{"Sine", "Saw", "Square", "Triangle"}, 1),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{lmw::param::attack, 1}, "Attack",  time(5.0f),  0.01f, seconds),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{lmw::param::decay, 1},  "Decay",   time(5.0f),  0.2f,  seconds),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{lmw::param::sustain, 1}, "Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f, percent),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{lmw::param::release, 1}, "Release", time(10.0f), 0.3f, seconds),
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{lmw::param::gain, 1}, "Volume",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f, decibels),
        std::make_unique<juce::AudioParameterInt>(juce::ParameterID{lmw::param::voices, 1}, "Voices",
            1, lmw::dsp::Engine::maxVoices, 8),
    };
}

LMWSynthProcessor::LMWSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LMWSynth", createLayout()) {
    pWave    = apvts.getRawParameterValue(lmw::param::wave);
    pAttack  = apvts.getRawParameterValue(lmw::param::attack);
    pDecay   = apvts.getRawParameterValue(lmw::param::decay);
    pSustain = apvts.getRawParameterValue(lmw::param::sustain);
    pRelease = apvts.getRawParameterValue(lmw::param::release);
    pGain    = apvts.getRawParameterValue(lmw::param::gain);
    pVoices  = apvts.getRawParameterValue(lmw::param::voices);
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
    p.wave    = static_cast<lmw::dsp::Waveform>(static_cast<int>(pWave->load()));
    p.attack  = pAttack->load();
    p.decay   = pDecay->load();
    p.sustain = pSustain->load();
    p.release = pRelease->load();
    engine.setParams(p);
    engine.setGain(pGain->load());
    engine.setVoiceCount(static_cast<int>(pVoices->load()));
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
        case M::None:        return;
    }
    applyNoteEvent(e);
    audioToGui.push(e);                              // dropped if the GUI is not draining (editor closed)
}

void LMWSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    pullParameters();

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
