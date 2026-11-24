// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE wrapper around lmw::dsp::Engine (Spec S3, S5). Parameters live in an
// AudioProcessorValueTreeState; IDs are dotted paths ("env1.attack") that map
// 1:1 to future 9P files (/params/env1/attack, Spec S5a.1, S10).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ui/FaderScale.h"
#include "ui/History.h"
#include "dsp/Engine.h"
#include "dsp/Comb.h"
#include "dsp/Delay.h"
#include "dsp/SpscQueue.h"
#include "dsp/MidiParse.h"
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <string_view>

namespace lmw::param {   // Spec S5a.1
inline constexpr const char* voices      = "voice.count";
// Spec S5.10: per VCO (index 0..2 = VCO 1..3).
inline constexpr std::array<const char*, 3> vcoWave  {"vco1.wave",   "vco2.wave",   "vco3.wave"};
inline constexpr std::array<const char*, 3> vcoFm    {"vco1.fm",     "vco2.fm",     "vco3.fm"};
inline constexpr std::array<const char*, 3> vcoPm    {"vco1.pm",     "vco2.pm",     "vco3.pm"};   // Spec S5.20
inline constexpr std::array<const char*, 3> vcoOctave{"vco1.octave", "vco2.octave", "vco3.octave"};
inline constexpr std::array<const char*, 3> vcoTune  {"vco1.tune",   "vco2.tune",   "vco3.tune"};
inline constexpr std::array<const char*, 3> mixVco   {"mix.vco1",    "mix.vco2",    "mix.vco3"};
inline constexpr const char* mixNoise    = "mix.noise";
// Spec S5.15: NOISE module — colour of the noise, and the sample & hold random source.
inline constexpr const char* noiseColour = "noise.colour";
inline constexpr const char* noiseRate   = "noise.rate";
inline constexpr const char* noiseSmooth = "noise.smooth";
// Spec S5.9, S5.11: per VCF (index 0..1 = VCF 1..2).
inline constexpr std::array<const char*, 2> vcfCutoff{"vcf1.cutoff", "vcf2.cutoff"};
inline constexpr std::array<const char*, 2> vcfRes   {"vcf1.res",    "vcf2.res"};
inline constexpr std::array<const char*, 2> vcfEnv   {"vcf1.env",    "vcf2.env"};
inline constexpr std::array<const char*, 2> vcfKey   {"vcf1.key",    "vcf2.key"};     // Spec S5.13
inline constexpr std::array<const char*, 2> vcfSlope {"vcf1.slope",  "vcf2.slope"};
inline constexpr std::array<const char*, 2> vcfType  {"vcf1.type",   "vcf2.type"};
inline constexpr const char* vcfRouting  = "vcf2.routing";   // Spec S5.17: Serial / Parallel
// Spec S5.16: FORMANT (vowel filter) after VCF 2.
inline constexpr const char* formantVowel = "formant.vowel";
inline constexpr const char* formantVoice = "formant.voice";
inline constexpr const char* formantRes   = "formant.res";
inline constexpr const char* formantMix   = "formant.mix";
inline constexpr const char* vcaInitial  = "vca.initial";
inline constexpr const char* vcaTremolo  = "vca.tremolo";   // Spec S5.12
inline constexpr const char* vcaVelocity = "vca.velocity";  // Spec S5.22
inline constexpr const char* env1Attack  = "env1.attack";
inline constexpr const char* env1Decay   = "env1.decay";
inline constexpr const char* env1Sustain = "env1.sustain";
inline constexpr const char* env1Release = "env1.release";
inline constexpr const char* env2Attack  = "env2.attack";
inline constexpr const char* env2Decay   = "env2.decay";
inline constexpr const char* env2Sustain = "env2.sustain";
inline constexpr const char* env2Release = "env2.release";
// Spec S5.12: LFO 1 (vibrato, normalled to the VCO FM inputs) and LFO 2 (tremolo, normalled to vca.mod).
inline constexpr const char* lfoRate     = "lfo1.rate";
inline constexpr const char* lfoWave     = "lfo1.wave";
inline constexpr const char* lfo2Rate    = "lfo2.rate";
inline constexpr const char* lfo2Wave    = "lfo2.wave";
inline constexpr const char* gain        = "out.gain";
// Spec S7.4: Effects tab, reverb after the voices (and the soft limiter), before the output.
inline constexpr const char* reverbMix     = "reverb.mix";
inline constexpr const char* reverbSize    = "reverb.size";
inline constexpr const char* reverbDamping = "reverb.damping";
inline constexpr const char* reverbWidth   = "reverb.width";
// Spec S7.6: COMB on the Effects tab.
inline constexpr const char* combFreq     = "comb.freq";
inline constexpr const char* combKey      = "comb.key";
inline constexpr const char* combFeedback = "comb.feedback";
inline constexpr const char* combDamping  = "comb.damping";
inline constexpr const char* combMix      = "comb.mix";
// Spec S7.8: DELAY on the Effects tab.
inline constexpr const char* delayTime     = "delay.time";
inline constexpr const char* delayFeedback = "delay.feedback";
inline constexpr const char* delayDamping  = "delay.damping";
inline constexpr const char* delaySpread   = "delay.spread";
inline constexpr const char* delayMix      = "delay.mix";
}

class LMWSynthProcessor final : public juce::AudioProcessor,
                                private juce::MidiKeyboardState::Listener,
                                private juce::AudioProcessorParameter::Listener,
                                private juce::ValueTree::Listener {
public:
    LMWSynthProcessor();
    ~LMWSynthProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // The reverb rings on after the last note (Spec S7.4); without it the sound stops with the notes.
    // Spec S7.6: the comb rings on too (high feedback).
    double getTailLengthSeconds() const override {
        return raw.revMix->load() > 0.0f || raw.delayMix->load() > 0.0f ? 10.0 : (raw.combMix->load() > 0.0f ? 2.0 : 0.0);
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    // One fixed program; VST3 hosts and the Steinberg validator need it to have a name (S9.2).
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    juce::MidiKeyboardState& keyboardState() noexcept { return keyboard; }
    // Message thread: show notes that arrived from external MIDI on the on-screen keyboard.
    void updateKeyboardFromMidiInput();
    // Message thread: drop queued display events and clear the keyboard display (editor opened).
    void resetKeyboardDisplay();
    // Message thread (GUI, 9P): change one pin (Spec S5.4, S5.14). Lock-free. `from` = Patch::none
    // removes all pins of the input; lmw::dsp::removePin removes the pin from `from`; else it is added (a
    // cable at `level`, -1..1 on modulation inputs, S5.21) or its level set. False if the input has no room (maxPins) or the queue is full.
    bool requestPatch(lmw::dsp::In input, std::int8_t from, float level = 1.0f) noexcept {
        auto next = guiPatch;
        if (from == lmw::dsp::Patch::none) next.disconnect(input);
        else if (!next.connect(input, static_cast<lmw::dsp::Out>(from), level)) return false;
        if (!patchToAudio.push({input, from, level})) return false;
        guiPatch = next;
        ++patchSerial;
        noteChange();
        return true;
    }
    // Message thread: replace all pins (restored state, patch files, 9P). False if the queue could
    // not take every change (the GUI mirror then shows what was sent).
    bool setPatch(const lmw::dsp::Patch& p) noexcept {
        bool ok = true;
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto in = static_cast<lmw::dsp::In>(i);
            if (p.samePins(guiPatch, in)) continue;
            ok = requestPatch(in, lmw::dsp::Patch::none) && ok;
            for (int k = 0; k < p.pinCount(in); ++k) ok = requestPatch(in, p.pin(in, k).source, p.pin(in, k).level) && ok;
        }
        return ok;
    }
    // Spec S7: patch files (message thread). The text holds every parameter, the cables and the notes;
    // loading sets parameters missing from the file to their default. False + error text on failure.
    juce::String patchText(const juce::String& name) const;
    // A parameter's value as text (float exact, choice by name, int) and back (choice also by index;
    // numbers clipped to the range); shared by patch files and 9P (Spec S7.1, S10.10).
    // A float reads back as `written` (the text last written to it, over 9P or in a loaded patch) while
    // that text is a number within the range that stores the current value; else as the shortest such text.
    static std::string valueText(const juce::RangedAudioParameter&, std::string_view written = {});
    std::string currentText(const juce::RangedAudioParameter&) const;           // any thread
    void rememberText(const juce::RangedAudioParameter&, std::string_view text);  // any thread
    static std::optional<float> parseValue(const juce::RangedAudioParameter&, const juce::String& text);
    static void setNormalised(juce::RangedAudioParameter&, float normalised);   // message thread
    // Spec S10.10 /info: every parameter as one JSON array, in the synth's order (id, name, kind, range,
    // default; floats with unit, centre and step where they have one). Built from createLayout().
    std::string parameterInfo() const;
    // Spec S6.17: a float parameter's fader scale (nothing for choices and whole numbers).
    struct Scale { lmw::ui::scale::Kind kind; std::string unit; float lo, hi; std::optional<float> centre; };
    std::optional<Scale> scaleOf(const std::string& id) const;
    const std::string& ninepEndpoints() const noexcept { return ninepAddress; }  // "unix!<path> tcp!..."
    const class NinePService* ninepService() const noexcept { return ninep.get(); }   // Spec S12.2: in-process files
    // soundOnly (Spec S7.7, instrument presets): parameters, cables and name only; notes and the schematic stay.
    bool loadPatchText(const juce::String& text, juce::String& error, bool soundOnly = false);
    // Spec S7.7: sets the whole sound — every parameter (`values` by id; missing: default) and the cables —
    // and the patch name; keeps notes and the schematic. Message thread.
    bool applySound(const juce::String& name, const std::map<std::string, std::string>& values,
                    const std::string& cablesText, juce::String& error);
    // Spec S7.7: "changed since": the sound (all parameter values, the cables) when the last preset or patch
    // was applied, loaded or saved, kept in the saved state; soundChanged() compares with it. Message thread.
    void recordSound() { apvts.state.setProperty(soundProp, juce::String(soundSignature()), nullptr); }
    bool soundChanged() const {
        const auto recorded = apvts.state.getProperty(soundProp).toString();
        return recorded.isNotEmpty() && recorded != juce::String(soundSignature());
    }
    std::string soundSignature() const;
    // Spec S6.14 undo / redo (message thread). The sound as keys and texts: "p:<id>" a parameter (its exact
    // normalised value), "c:<input>" its cables, "schematic", "name", "notes", "s:<property>" the preset
    // labels and the recorded sound (S7.5, S7.7). beginStep/endStep around an action make one step (they
    // nest); undo/redo put back what one step changed and nothing else.
    lmw::ui::History::State soundState() const;
    void beginStep(const std::string& name) { history.begin(name, soundState()); }
    bool endStep() { return history.end(soundState()); }
    void renameStep(const std::string& name) { history.rename(name); }
    struct ScopedStep {                               // one step for everything in a block
        ScopedStep(LMWSynthProcessor& p, const juce::String& name) : proc(p) { proc.beginStep(name.toStdString()); }
        ~ScopedStep() { proc.endStep(); }
        ScopedStep(const ScopedStep&) = delete;
        ScopedStep& operator=(const ScopedStep&) = delete;
        LMWSynthProcessor& proc;
    };
    bool undo();
    bool redo();
    const lmw::ui::History& steps() const noexcept { return history; }
    // Free-text notes and the patch name, kept in the saved state and in patch files.
    juce::String notes() const { return apvts.state.getProperty(notesProp).toString(); }
    void setNotes(const juce::String& text) { apvts.state.setProperty(notesProp, text, nullptr); }
    juce::String patchName() const { return apvts.state.getProperty(nameProp).toString(); }
    void setPatchName(const juce::String& name) { apvts.state.setProperty(nameProp, name, nullptr); }
    // Spec S6.7: positions of the Schematic tab's blocks (Layout::serialize(); empty = default), kept in
    // the saved state and in patch files.
    juce::String schematicLayout() const { return apvts.state.getProperty(schematicProp).toString(); }
    void setSchematicLayout(const juce::String& text) { apvts.state.setProperty(schematicProp, text, nullptr); }
    std::uint32_t patchFileVersion() const noexcept { return loadedPatches; }   // changes on every load
    // Spec S7.5: the preset last chosen per module ("env1", "vcf2", "vco3", "comb", "delay", "reverb"): its name and the
    // values it set ("key=value;key=value", empty for Default), kept in the saved state (not in patch
    // files; a loaded patch clears them). Message thread.
    juce::String presetName(const juce::String& module) const { return apvts.state.getProperty("preset_" + module).toString(); }
    juce::String presetValues(const juce::String& module) const { return apvts.state.getProperty("presetValues_" + module).toString(); }
    void setPresetRecord(const juce::String& module, const juce::String& name, const juce::String& values) {
        apvts.state.setProperty("preset_" + module, name, nullptr);
        apvts.state.setProperty("presetValues_" + module, values, nullptr);
    }
    void clearPresetRecords() {
        for (int i = apvts.state.getNumProperties(); --i >= 0;)
            if (const auto id = apvts.state.getPropertyName(i).toString(); id.startsWith("preset_") || id.startsWith("presetValues_"))
                apvts.state.removeProperty(id, nullptr);
    }

    // Spec S10.10 /serial: goes up with every change to parameters, cables, patch name, notes, schematic
    // (and the rest of the saved state). Any thread; lock-free.
    std::uint64_t changeCount() const noexcept { return changeSerial.load(std::memory_order_relaxed); }
    // Spec S10.10 /keys (message thread): plays or releases a note as the on-screen keyboard does
    // (velocity 1-127; it shows on the keyboard too).
    void playKey(bool on, int note, int velocity) {
        if (on) keyboard.noteOn(1, note, static_cast<float>(velocity) / 127.0f);
        else keyboard.noteOff(1, note, 0.0f);
    }

    // Increases with every cable change, so the editor can redraw cables changed elsewhere.
    std::uint32_t patchVersion() const noexcept { return patchSerial; }
    // Message thread: the patch as last requested (mirror of the engine's patch) and the latest
    // mod wheel value — for hover texts in the GUI.
    // Its VCF routing (Spec S5.17) is taken from the parameter at each call.
    const lmw::dsp::Patch& currentPatch() const noexcept { guiPatch.setRouting(vcfRouting()); return guiPatch; }
    lmw::dsp::VcfRouting vcfRouting() const noexcept {   // any thread
        return raw.vcfRouting->load() >= 0.5f ? lmw::dsp::VcfRouting::Parallel : lmw::dsp::VcfRouting::Serial;
    }
    float modWheel() const noexcept { return modWheelValue.load(std::memory_order_relaxed); }

private:
    // What a float's NormalisableRange does not keep (for /info): the unit its text shows ("%" = value x 100,
    // "dB" = 20 log10 value, otherwise the value itself), the default as given, and the skew centre.
    struct FloatInfo {
        std::string unit;
        float def = 0.0f;
        std::optional<float> centre;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout(std::map<std::string, FloatInfo>& info);
    void pullParameters() noexcept;
    void handleMidi(const juce::uint8* data, int size) noexcept;
    void applyNoteEvent(const lmw::dsp::NoteEvent&) noexcept;
    // Spec S7.4, S7.6: the effects after the synth (in place on the output), per sample in the order
    // COMB, REVERB, main; their inputs follow the patch (effect ports). Audio thread.
    void processEffects(float* left, float* right, int n) noexcept;
    struct Stereo { float l = 0.0f, r = 0.0f; };
    Stereo combSample(Stereo in) noexcept;
    Stereo delaySample(Stereo in) noexcept;           // Spec S7.8
    Stereo reverbSample(Stereo in, bool stereo) noexcept;
    juce::Reverb::Parameters reverbParameters() const noexcept;

    // MidiKeyboardState::Listener — called on the message thread by on-screen / QWERTY keys.
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    // /serial: parameter changes (any thread, also the audio thread for host automation) and changes of
    // the state tree (notes, name, schematic, restored state; message thread).
    void noteChange() noexcept { changeSerial.fetch_add(1, std::memory_order_relaxed); }
    void parameterValueChanged(int, float) override { noteChange(); }
    // Spec S6.14: a gesture (a fader drag from press to release, a typed value, a choice; setNormalised)
    // is one step; inside a larger step it joins that one. Not while an undo or redo is applied.
    void parameterGestureChanged(int index, bool starting) override;
    // JUCE copies parameter values into the state tree on a timer (PARAM "value"): already counted by
    // parameterValueChanged, and later, so /serial would tick again on its own after a write.
    void valueTreePropertyChanged(juce::ValueTree& t, const juce::Identifier& id) override {
        if (!(t.hasType("PARAM") && id == juce::Identifier("value"))) noteChange();
    }
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override { noteChange(); }
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override { noteChange(); }
    void valueTreeRedirected(juce::ValueTree&) override { noteChange(); }
    std::atomic<std::uint64_t> changeSerial{0};
    mutable std::mutex writtenMutex;                   // guards writtenTexts (9P thread and message thread)
    std::map<std::string, std::string> writtenTexts;   // paramID -> float text last written (S10.10)

    std::map<std::string, FloatInfo> floatInfo;   // filled by createLayout(); declared before apvts
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;   // on-screen + computer keyboard input
    lmw::dsp::Engine engine;

    // Spec S10.2: no locks on the audio thread. Keyboard notes go GUI -> audio, external MIDI
    // notes go audio -> GUI (display only), each through its own lock-free SPSC queue.
    lmw::dsp::SpscQueue<lmw::dsp::NoteEvent, 256> guiToAudio;
    lmw::dsp::SpscQueue<lmw::dsp::NoteEvent, 256> audioToGui;
    lmw::dsp::SpscQueue<lmw::dsp::PatchCommand, 256> patchToAudio;   // a whole patch: 26 x (1 + 4)
    inline static const juce::Identifier notesProp{"notes"}, nameProp{"patchName"}, schematicProp{"schematicLayout"},
                                         soundProp{"soundRecorded"};   // Spec S7.7
    std::uint32_t loadedPatches = 0;                  // message thread only
    std::unique_ptr<class NinePService> ninep;        // Spec S10: 9P server (started in the constructor)
    std::string ninepAddress;
    lmw::ui::History history;                         // Spec S6.14, message thread
    bool applyingHistory = false;
    mutable lmw::dsp::Patch guiPatch;                 // message thread only (routing refreshed by currentPatch())
    std::uint32_t patchSerial = 0;                    // message thread only
    std::atomic<float> modWheelValue{0.0f};           // written by the audio thread
    bool applyingMidiInput = false;   // message thread only: don't echo displayed notes back
    static constexpr double pitchBendRangeSemitones = 2.0;   // Spec S3.4

    struct RawParams {                 // atomics owned by apvts, read on the audio thread
        std::atomic<float> *voices{}, *mixNoise{}, *vcaInitial{},
            *e1a{}, *e1d{}, *e1s{}, *e1r{}, *e2a{}, *e2d{}, *e2s{}, *e2r{}, *lfoRate{}, *lfoWave{}, *gain{},
            *lfo2Rate{}, *lfo2Wave{}, *tremolo{}, *vcaVelocity{}, *revMix{}, *revSize{}, *revDamping{}, *revWidth{},
            *noiseColour{}, *noiseRate{}, *noiseSmooth{},
            *combFreq{}, *combKey{}, *combFeedback{}, *combDamping{}, *combMix{},
            *delayTime{}, *delayFeedback{}, *delayDamping{}, *delaySpread{}, *delayMix{}, *formantVowel{}, *formantVoice{}, *formantRes{}, *formantMix{}, *vcfRouting{};
        std::array<std::atomic<float>*, 2> cutoff{}, resonance{}, vcfEnv{}, vcfKey{}, vcfSlope{}, vcfType{};
        std::array<std::atomic<float>*, 3> wave{}, fm{}, pm{}, octave{}, tune{}, mixVco{};
    } raw;

    // Spec S7.4: reverb (JUCE's Freeverb), wet only, mixed in with our own 20 ms Mix smoothing. While Mix
    // is 0 it is not run at all, so the output is exactly the synth's. Buffers sized in prepareToPlay.
    juce::Reverb reverb;
    juce::SmoothedValue<float> reverbMix;
    bool reverbRunning = false;
    // Spec S7.6: COMB, one comb line per channel; Mix 0 = not run, the input passes exactly.
    lmw::dsp::Comb combL, combR;
    juce::SmoothedValue<float> combMix;
    bool combRunning = false;
    // Spec S7.8: DELAY (stereo); Mix 0 = not run.
    lmw::dsp::Delay delayFx;
    juce::SmoothedValue<float> delayMix;
    bool delayRunning = false;
    Stereo prevComb, prevDelay, prevReverb;            // last sample's effect outputs (feedback order)

    JUCE_DECLARE_WEAK_REFERENCEABLE(LMWSynthProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthProcessor)
};
