// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE wrapper around lmw::dsp::Engine (Spec S3, S5). Parameters live in an
// AudioProcessorValueTreeState; IDs are dotted paths ("env.attack") that map
// 1:1 to future 9P files (/params/env/attack, Spec S10).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"

namespace lmw::param {
inline constexpr const char* wave    = "vco.wave";
inline constexpr const char* attack  = "env.attack";
inline constexpr const char* decay   = "env.decay";
inline constexpr const char* sustain = "env.sustain";
inline constexpr const char* release = "env.release";
inline constexpr const char* gain    = "out.gain";
inline constexpr const char* voices  = "voice.count";
}

class LMWSynthProcessor final : public juce::AudioProcessor {
public:
    LMWSynthProcessor();

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullParameters() noexcept;
    void handleMidi(const juce::MidiMessage&) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;   // on-screen + computer keyboard input
    lmw::dsp::Engine engine;

    std::atomic<float>* pWave{}; std::atomic<float>* pAttack{}; std::atomic<float>* pDecay{};
    std::atomic<float>* pSustain{}; std::atomic<float>* pRelease{}; std::atomic<float>* pGain{};
    std::atomic<float>* pVoices{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthProcessor)
};
