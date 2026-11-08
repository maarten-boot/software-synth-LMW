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
    setColour(juce::Slider::thumbColourId, kFg);
    setColour(juce::Slider::trackColourId, kFg);
    setColour(juce::Slider::backgroundColourId, kFg);
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

void LMWLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      juce::Slider::SliderStyle style, juce::Slider& s) {
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, s);
        return;
    }
    // Empty track = thin line, filled part (value) = thick bar, thumb = black cap with a white centre line.
    // Distinguished by shape, not by colour, so every pair stays black/white (21:1).
    const float cx     = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float top    = static_cast<float>(y);
    const float bottom = static_cast<float>(y + height);
    g.setColour(kFg);
    g.fillRect(juce::Rectangle<float>(cx - 1.0f, top, 2.0f, bottom - top));
    g.fillRect(juce::Rectangle<float>(cx - 3.0f, sliderPos, 6.0f, bottom - sliderPos));

    const auto thumb = juce::Rectangle<float>(30.0f, 14.0f).withCentre({cx, sliderPos});
    g.fillRect(thumb);
    g.setColour(kBg);
    g.fillRect(juce::Rectangle<float>(thumb.getX() + 4.0f, sliderPos - 1.0f, thumb.getWidth() - 8.0f, 2.0f));
    if (s.hasKeyboardFocus(false)) {           // visible focus ring, black on white
        g.setColour(kFg);
        g.drawRect(juce::Rectangle<float>(static_cast<float>(x), top, static_cast<float>(width), bottom - top).expanded(2.0f), 1.0f);
    }
}

LMWSynthEditor::LMWSynthEditor(LMWSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&lookAndFeel);
    namespace P = lmw::param;

    // Spec S6.1: module panels. Row 0: POLY | VCO | MIXER | VCF | LFO | OUT; row 1: the envelopes.
    // Titles name the normalled destination (S5.2) so the prewired patch is visible.
    groups = {
        {"POLY", 0, {addFader(P::voices, "Voices")}, {}},
        {"VCO", 0, {addChoice(P::wave, "Wave", {"Sine", "Saw", "Square", "Triangle"}),
                    addFader(P::fm, "FM")}, {}},
        {"MIXER", 0, {addFader(P::mixVco, "VCO"), addFader(P::mixNoise, "Noise")}, {}},
        {"VCF", 0, {addFader(P::cutoff, "Cutoff"), addFader(P::resonance, "Reso"),
                    addFader(P::vcfEnv, "Env 2")}, {}},
        {"LFO", 0, {addChoice(P::lfoWave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),
                    addFader(P::lfoRate, "Rate")}, {}},
        {"OUT", 0, {addFader(P::gain, "Volume")}, {}},
        {juce::String::fromUTF8("ENV 1 \xe2\x86\x92 VCA"), 1,
            {addFader(P::env1Attack, "Attack"), addFader(P::env1Decay, "Decay"),
             addFader(P::env1Sustain, "Sustain"), addFader(P::env1Release, "Release")}, {}},
        {juce::String::fromUTF8("ENV 2 \xe2\x86\x92 VCF"), 1,
            {addFader(P::env2Attack, "Attack"), addFader(P::env2Decay, "Decay"),
             addFader(P::env2Sustain, "Sustain"), addFader(P::env2Release, "Release")}, {}},
    };
    jassert(usedFaders == faders.size() && usedChoices == choices.size());

    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    addAndMakeVisible(keyboard);

    // Child widgets (e.g. slider value boxes) take their colours when created, before they had this
    // look-and-feel; re-send it so every child uses the black-on-white theme (Spec S6.2).
    sendLookAndFeelChange();

    // Width: widest row (11 columns, 6 groups); height: 2 module rows + keyboard.
    setSize(12 + 11 * colW + 5 * gap + 12, 12 + 36 + 2 * rowH + gap + 10 + 90 + 12);
    p.resetKeyboardDisplay();
    startTimerHz(30);                            // show external MIDI notes on the keyboard
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::Component>(&keyboard)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

LMWSynthEditor::~LMWSynthEditor() { stopTimer(); setLookAndFeel(nullptr); }

bool LMWSynthEditor::isTypingText() {
    return dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr;
}

bool LMWSynthEditor::keyPressed(const juce::KeyPress& key) {
    return !isTypingText() && keyboard.keyPressed(key);
}

bool LMWSynthEditor::keyStateChanged(bool isKeyDown) {
    return !isTypingText() && keyboard.keyStateChanged(isKeyDown);
}

LMWSynthEditor::Item LMWSynthEditor::addFader(const char* paramId, const juce::String& text) {
    Fader& k = faders[usedFaders++];
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 26);
    k.slider.setSliderSnapsToMousePosition(false);   // drag moves relative; a click does not jump the value
    k.attachment = std::make_unique<SliderAttachment>(processor.state(), paramId, k.slider);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider);
    addAndMakeVisible(k.label);
    return {&k, nullptr};
}

LMWSynthEditor::Item LMWSynthEditor::addChoice(const char* paramId, const juce::String& text,
                                               const juce::StringArray& options) {
    Choice& c = choices[usedChoices++];
    c.box.addItemList(options, 1);
    c.attachment = std::make_unique<ComboAttachment>(processor.state(), paramId, c.box);
    c.label.setText(text, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c.box);
    addAndMakeVisible(c.label);
    return {nullptr, &c};
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kFg);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("LMW-Synth", getLocalBounds().removeFromTop(40).reduced(12, 0), juce::Justification::centredLeft);
    g.drawLine(12.0f, 40.0f, static_cast<float>(getWidth() - 12), 40.0f, 1.0f);

    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    for (const auto& grp : groups) {
        g.drawRect(grp.bounds, 1);
        g.drawText(grp.title, grp.bounds.withHeight(24), juce::Justification::centred);
        g.drawHorizontalLine(grp.bounds.getY() + 24, static_cast<float>(grp.bounds.getX()),
                             static_cast<float>(grp.bounds.getRight()));
    }
}

void LMWSynthEditor::layoutItem(const Item& item, juce::Rectangle<int> column) {
    column.reduce(4, 0);
    if (item.fader != nullptr) {
        item.fader->label.setBounds(column.removeFromTop(22));
        item.fader->slider.setBounds(column);
    } else if (item.choice != nullptr) {
        column.reduce(2, 0);
        item.choice->label.setBounds(column.removeFromTop(22));
        item.choice->box.setBounds(column.removeFromTop(28));
    }
}

void LMWSynthEditor::resized() {
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(36);
    keyboard.setBounds(area.removeFromBottom(90));
    area.removeFromBottom(10);

    std::array<juce::Rectangle<int>, 2> rows;
    rows[0] = area.removeFromTop(rowH);
    area.removeFromTop(gap);
    rows[1] = area.removeFromTop(rowH);

    for (auto& grp : groups) {
        auto& row = rows[static_cast<std::size_t>(grp.row)];
        grp.bounds = row.removeFromLeft(static_cast<int>(grp.items.size()) * colW);
        row.removeFromLeft(gap);
        auto inner = grp.bounds.reduced(0, 4).withTrimmedTop(header);
        for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(colW));
    }
}
