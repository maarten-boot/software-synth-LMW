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

namespace lmw::param {   // Spec S5a.1
inline constexpr const char* voices      = "voice.count";
inline constexpr const char* wave        = "vco.wave";
inline constexpr const char* fm          = "vco.fm";
inline constexpr const char* mixVco      = "mix.vco";
inline constexpr const char* mixNoise    = "mix.noise";
inline constexpr const char* cutoff      = "vcf.cutoff";
inline constexpr const char* resonance   = "vcf.res";
inline constexpr const char* vcfEnv      = "vcf.env";
inline constexpr const char* vcaInitial  = "vca.initial";
inline constexpr const char* env1Attack  = "env1.attack";
inline constexpr const char* env1Decay   = "env1.decay";
inline constexpr const char* env1Sustain = "env1.sustain";
inline constexpr const char* env1Release = "env1.release";
inline constexpr const char* env2Attack  = "env2.attack";
inline constexpr const char* env2Decay   = "env2.decay";
inline constexpr const char* env2Sustain = "env2.sustain";
inline constexpr const char* env2Release = "env2.release";
inline constexpr const char* lfoRate     = "lfo.rate";
inline constexpr const char* lfoWave     = "lfo.wave";
inline constexpr const char* gain        = "out.gain";
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    juce::MidiKeyboardState& keyboardState() noexcept { return keyboard; }
    // Message thread: show notes that arrived from external MIDI on the on-screen keyboard.
    void updateKeyboardFromMidiInput();
    // Message thread: drop queued display events and clear the keyboard display (editor opened).
    void resetKeyboardDisplay();
    // Message thread (GUI, later 9P): patch or unpatch a cable (Spec S5.4). Lock-free; returns
    // false if the queue is full. `from` = Patch::none removes the cable.
    bool requestPatch(lmw::dsp::In input, std::int8_t from) noexcept {
        if (!patchToAudio.push({input, from})) return false;
        if (from == lmw::dsp::Patch::none) guiPatch.disconnect(input);
        else guiPatch.connect(input, static_cast<lmw::dsp::Out>(from));
        ++patchSerial;
        return true;
    }
    // Message thread: replace all cables (restored state; later patch files). False if the queue
    // could not take every change (the GUI mirror then shows what was sent).
    bool setPatch(const lmw::dsp::Patch& p) noexcept {
        bool ok = true;
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto in = static_cast<lmw::dsp::In>(i);
            const std::int8_t want = p.isPatched(in) ? static_cast<std::int8_t>(p.source(in)) : lmw::dsp::Patch::none;
            const std::int8_t have = guiPatch.isPatched(in) ? static_cast<std::int8_t>(guiPatch.source(in)) : lmw::dsp::Patch::none;
            if (want != have) ok = requestPatch(in, want) && ok;
        }
        return ok;
    }
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
    lmw::dsp::SpscQueue<lmw::dsp::PatchCommand, 64> patchToAudio;
    lmw::dsp::Patch guiPatch;                         // message thread only
    std::uint32_t patchSerial = 0;                    // message thread only
    std::atomic<float> modWheelValue{0.0f};           // written by the audio thread
    bool applyingMidiInput = false;   // message thread only: don't echo displayed notes back
    static constexpr double pitchBendRangeSemitones = 2.0;   // Spec S3.4

    struct RawParams {                 // atomics owned by apvts, read on the audio thread
        std::atomic<float> *voices{}, *wave{}, *fm{}, *mixVco{}, *mixNoise{}, *cutoff{}, *resonance{}, *vcfEnv{}, *vcaInitial{},
            *e1a{}, *e1d{}, *e1s{}, *e1r{}, *e2a{}, *e2d{}, *e2s{}, *e2r{}, *lfoRate{}, *lfoWave{}, *gain{};
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthProcessor)
};
