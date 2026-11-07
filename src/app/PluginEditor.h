// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include "PluginProcessor.h"

// Spec S6.2: black-on-white look; every colour pair is 21:1.
class LMWLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LMWLookAndFeel();
    // Spec S6.1: vertical faders, drawn only in black on white (no grey tints).
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override { return 7; }
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

    struct Fader {
        juce::Slider slider{juce::Slider::LinearVertical, juce::Slider::TextBoxBelow};
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };
    struct Group {
        juce::String title;
        juce::Rectangle<int> bounds;
    };
    void setupFader(Fader&, const char* paramId, const juce::String& text);
    static void layoutFader(Fader&, juce::Rectangle<int> column);

    LMWSynthProcessor& processor;
    LMWLookAndFeel lookAndFeel;
    juce::ComboBox wave;
    juce::Label waveLabel;
    std::unique_ptr<ComboAttachment> waveAttachment;
    Fader attack, decay, sustain, release, gain, voices;
    std::array<Group, 3> groups{{{"VCO", {}}, {"ENV", {}}, {"OUT", {}}}};
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
