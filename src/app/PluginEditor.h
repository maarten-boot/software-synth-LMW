// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Spec S6.2: black-on-white look; every colour pair is 21:1.
class LMWLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LMWLookAndFeel();
};

class LMWSynthEditor final : public juce::AudioProcessorEditor {
public:
    explicit LMWSynthEditor(LMWSynthProcessor&);
    ~LMWSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob {
        juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };
    void setupKnob(Knob&, const char* paramId, const juce::String& text);

    LMWSynthProcessor& processor;
    LMWLookAndFeel lookAndFeel;
    juce::ComboBox wave;
    juce::Label waveLabel;
    std::unique_ptr<ComboAttachment> waveAttachment;
    Knob voices, attack, decay, sustain, release, gain;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
