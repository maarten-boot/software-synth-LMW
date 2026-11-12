// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE wrapper around lmw::dsp::Engine (Spec S3, S5). Parameters live in an
// AudioProcessorValueTreeState; IDs are dotted paths ("env1.attack") that map
// 1:1 to future 9P files (/params/env1/attack, Spec S5a.1, S10).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"
#include "dsp/SpscQueue.h"
#include "dsp/MidiParse.h"
#include <array>
#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace lmw::param {   // Spec S5a.1
inline constexpr const char* voices      = "voice.count";
// Spec S5.10: per VCO (index 0..2 = VCO 1..3).
inline constexpr std::array<const char*, 3> vcoWave  {"vco1.wave",   "vco2.wave",   "vco3.wave"};
inline constexpr std::array<const char*, 3> vcoFm    {"vco1.fm",     "vco2.fm",     "vco3.fm"};
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
inline constexpr const char* vcaInitial  = "vca.initial";
inline constexpr const char* vcaTremolo  = "vca.tremolo";   // Spec S5.12
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
}

class LMWSynthProcessor final : public juce::AudioProcessor,
                                private juce::MidiKeyboardState::Listener {
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
    double getTailLengthSeconds() const override { return raw.revMix->load() > 0.0f ? 10.0 : 0.0; }

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
    // removes all pins of the input; level <= 0 removes the pin from `from`; else it is added (a
    // cable at `level`) or its level set. False if the input has no room (maxPins) or the queue is full.
    bool requestPatch(lmw::dsp::In input, std::int8_t from, float level = 1.0f) noexcept {
        auto next = guiPatch;
        if (from == lmw::dsp::Patch::none) next.disconnect(input);
        else if (!next.connect(input, static_cast<lmw::dsp::Out>(from), level)) return false;
        if (!patchToAudio.push({input, from, level})) return false;
        guiPatch = next;
        ++patchSerial;
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
    static std::string valueText(const juce::RangedAudioParameter&);
    static std::optional<float> parseValue(const juce::RangedAudioParameter&, const juce::String& text);
    static void setNormalised(juce::RangedAudioParameter&, float normalised);   // message thread
    const std::string& ninepEndpoints() const noexcept { return ninepAddress; }  // "unix!<path> tcp!..."
    bool loadPatchText(const juce::String& text, juce::String& error);
    // Free-text notes and the patch name, kept in the saved state and in patch files.
    juce::String notes() const { return apvts.state.getProperty(notesProp).toString(); }
    void setNotes(const juce::String& text) { apvts.state.setProperty(notesProp, text, nullptr); }
    juce::String patchName() const { return apvts.state.getProperty(nameProp).toString(); }
    void setPatchName(const juce::String& name) { apvts.state.setProperty(nameProp, name, nullptr); }
    std::uint32_t patchFileVersion() const noexcept { return loadedPatches; }   // changes on every load

    // Increases with every cable change, so the editor can redraw cables changed elsewhere.
    std::uint32_t patchVersion() const noexcept { return patchSerial; }
    // Message thread: the patch as last requested (mirror of the engine's patch) and the latest
    // mod wheel value — for hover texts in the GUI.
    const lmw::dsp::Patch& currentPatch() const noexcept { return guiPatch; }
    float modWheel() const noexcept { return modWheelValue.load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullParameters() noexcept;
    void handleMidi(const juce::uint8* data, int size) noexcept;
    void applyNoteEvent(const lmw::dsp::NoteEvent&) noexcept;
    void applyReverb(float* left, float* right, int n) noexcept;   // Spec S7.4, audio thread
    juce::Reverb::Parameters reverbParameters() const noexcept;

    // MidiKeyboardState::Listener — called on the message thread by on-screen / QWERTY keys.
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;   // on-screen + computer keyboard input
    lmw::dsp::Engine engine;

    // Spec S10.2: no locks on the audio thread. Keyboard notes go GUI -> audio, external MIDI
    // notes go audio -> GUI (display only), each through its own lock-free SPSC queue.
    lmw::dsp::SpscQueue<lmw::dsp::NoteEvent, 256> guiToAudio;
    lmw::dsp::SpscQueue<lmw::dsp::NoteEvent, 256> audioToGui;
    lmw::dsp::SpscQueue<lmw::dsp::PatchCommand, 256> patchToAudio;   // a whole patch: 22 x (1 + 4)
    inline static const juce::Identifier notesProp{"notes"}, nameProp{"patchName"};
    std::uint32_t loadedPatches = 0;                  // message thread only
    std::unique_ptr<class NinePService> ninep;        // Spec S10: 9P server (started in the constructor)
    std::string ninepAddress;
    lmw::dsp::Patch guiPatch;                         // message thread only
    std::uint32_t patchSerial = 0;                    // message thread only
    std::atomic<float> modWheelValue{0.0f};           // written by the audio thread
    bool applyingMidiInput = false;   // message thread only: don't echo displayed notes back
    static constexpr double pitchBendRangeSemitones = 2.0;   // Spec S3.4

    struct RawParams {                 // atomics owned by apvts, read on the audio thread
        std::atomic<float> *voices{}, *mixNoise{}, *vcaInitial{},
            *e1a{}, *e1d{}, *e1s{}, *e1r{}, *e2a{}, *e2d{}, *e2s{}, *e2r{}, *lfoRate{}, *lfoWave{}, *gain{},
            *lfo2Rate{}, *lfo2Wave{}, *tremolo{}, *revMix{}, *revSize{}, *revDamping{}, *revWidth{},
            *noiseColour{}, *noiseRate{}, *noiseSmooth{};
        std::array<std::atomic<float>*, 2> cutoff{}, resonance{}, vcfEnv{}, vcfKey{}, vcfSlope{}, vcfType{};
        std::array<std::atomic<float>*, 3> wave{}, fm{}, octave{}, tune{}, mixVco{};
    } raw;

    // Spec S7.4: reverb (JUCE's Freeverb), wet only, mixed in with our own 20 ms Mix smoothing. While Mix
    // is 0 it is not run at all, so the output is exactly the synth's. Buffers sized in prepareToPlay.
    juce::Reverb reverb;
    std::vector<float> wetLeft, wetRight;
    juce::SmoothedValue<float> reverbMix;
    bool reverbRunning = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LMWSynthProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthProcessor)
};
