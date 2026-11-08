// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
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

class LMWSynthEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LMWSynthEditor(LMWSynthProcessor&);
    ~LMWSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // QWERTY notes work whichever control has focus (Spec S3.2): letter keys not used by the
    // focused control reach the editor and are passed to the on-screen keyboard.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override { processor.updateKeyboardFromMidiInput(); }
    static bool isTypingText();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Fader {
        juce::Slider slider{juce::Slider::LinearVertical, juce::Slider::TextBoxBelow};
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };
    struct Choice {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };
    struct Item {                       // one column in a module group: a fader or a choice box
        Fader* fader = nullptr;
        Choice* choice = nullptr;
    };
    struct Group {                      // a framed module panel (Spec S6.1)
        juce::String title;
        int row = 0;
        std::vector<Item> items;
        juce::Rectangle<int> bounds;
    };
    Item addFader(const char* paramId, const juce::String& text);
    Item addChoice(const char* paramId, const juce::String& text, const juce::StringArray& options);
    static void layoutItem(const Item&, juce::Rectangle<int> column);

    static constexpr int colW = 80, gap = 10, header = 28, rowH = 230;

    LMWSynthProcessor& processor;
    LMWLookAndFeel lookAndFeel;
    std::array<Fader, 17> faders;
    std::array<Choice, 2> choices;
    std::size_t usedFaders = 0, usedChoices = 0;
    std::vector<Group> groups;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
