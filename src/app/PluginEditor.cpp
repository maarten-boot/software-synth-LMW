// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "ui/Theme.h"

namespace {
juce::Colour toJuce(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
const juce::Colour kBg      = toJuce(lmw::ui::theme::background);
const juce::Colour kFg      = toJuce(lmw::ui::theme::foreground);
const juce::Colour kKeyDown = toJuce(lmw::ui::theme::keyDown);
}

LMWLookAndFeel::LMWLookAndFeel() {
    using C = juce::LookAndFeel_V4::ColourScheme;
    setColourScheme(C{kBg, kBg, kBg, kFg, kFg, kFg, kBg, kFg, kFg});
    // windowBackground, widgetBackground, menuBackground, outline, defaultText,
    // defaultFill, highlightedText, highlightedFill, menuText
    setColour(juce::Slider::rotarySliderFillColourId, kFg);
    setColour(juce::Slider::rotarySliderOutlineColourId, kFg.withAlpha(0.25f));
    setColour(juce::Slider::thumbColourId, kFg);
    setColour(juce::Slider::textBoxTextColourId, kFg);
    setColour(juce::Slider::textBoxBackgroundColourId, kBg);
    setColour(juce::Slider::textBoxOutlineColourId, kFg);
    setColour(juce::Label::textColourId, kFg);
    setColour(juce::ComboBox::textColourId, kFg);
    setColour(juce::ComboBox::backgroundColourId, kBg);
    setColour(juce::ComboBox::outlineColourId, kFg);
    setColour(juce::ComboBox::arrowColourId, kFg);
    setColour(juce::PopupMenu::backgroundColourId, kBg);
    setColour(juce::PopupMenu::textColourId, kFg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kFg);
    setColour(juce::PopupMenu::highlightedTextColourId, kBg);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, kBg);
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, kFg);
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, kFg);
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, kKeyDown);
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kKeyDown.withAlpha(0.3f));
    setColour(juce::MidiKeyboardComponent::textLabelColourId, kFg);
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, kBg);
    setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, kFg);
}

LMWSynthEditor::LMWSynthEditor(LMWSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&lookAndFeel);

    wave.addItemList({"Sine", "Saw", "Square", "Triangle"}, 1);
    waveAttachment = std::make_unique<ComboAttachment>(p.state(), lmw::param::wave, wave);
    waveLabel.setText("VCO", juce::dontSendNotification);
    waveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wave);
    addAndMakeVisible(waveLabel);

    setupKnob(attack,  lmw::param::attack,  "Attack");
    setupKnob(decay,   lmw::param::decay,   "Decay");
    setupKnob(sustain, lmw::param::sustain, "Sustain");
    setupKnob(release, lmw::param::release, "Release");
    setupKnob(gain,    lmw::param::gain,    "Volume");

    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    addAndMakeVisible(keyboard);

    setSize(720, 360);
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::Component>(&keyboard)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

LMWSynthEditor::~LMWSynthEditor() { setLookAndFeel(nullptr); }

void LMWSynthEditor::setupKnob(Knob& k, const char* paramId, const juce::String& text) {
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    k.attachment = std::make_unique<SliderAttachment>(processor.state(), paramId, k.slider);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider);
    addAndMakeVisible(k.label);
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kFg);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("LMW Synth", getLocalBounds().removeFromTop(40).reduced(12, 0), juce::Justification::centredLeft);
    g.drawLine(12.0f, 40.0f, static_cast<float>(getWidth() - 12), 40.0f, 1.0f);
}

void LMWSynthEditor::resized() {
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(36);
    keyboard.setBounds(area.removeFromBottom(90));
    area.removeFromBottom(10);

    const int w = area.getWidth() / 6;
    auto col = area.removeFromLeft(w).reduced(6);
    waveLabel.setBounds(col.removeFromTop(22));
    wave.setBounds(col.removeFromTop(28));
    for (Knob* k : {&attack, &decay, &sustain, &release, &gain}) {
        auto c = area.removeFromLeft(w).reduced(6);
        k->label.setBounds(c.removeFromTop(22));
        k->slider.setBounds(c);
    }
}
