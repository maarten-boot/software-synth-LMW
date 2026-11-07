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

    wave.addItemList({"Sine", "Saw", "Square", "Triangle"}, 1);
    waveAttachment = std::make_unique<ComboAttachment>(p.state(), lmw::param::wave, wave);
    waveLabel.setText("Wave", juce::dontSendNotification);
    waveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wave);
    addAndMakeVisible(waveLabel);

    setupFader(attack,  lmw::param::attack,  "Attack");
    setupFader(decay,   lmw::param::decay,   "Decay");
    setupFader(sustain, lmw::param::sustain, "Sustain");
    setupFader(release, lmw::param::release, "Release");
    setupFader(gain,    lmw::param::gain,    "Volume");
    setupFader(voices,  lmw::param::voices,  "Voices");

    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    addAndMakeVisible(keyboard);

    // Child widgets (e.g. slider value boxes) take their colours when created, before they had this
    // look-and-feel; re-send it so every child uses the black-on-white theme (Spec S6.2).
    sendLookAndFeelChange();

    setSize(614, 450);   // 12 + 7 columns x 80 + 3 gaps x 10 + 12
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

void LMWSynthEditor::setupFader(Fader& k, const char* paramId, const juce::String& text) {
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 26);
    k.slider.setSliderSnapsToMousePosition(false);   // drag moves relative; a click does not jump the value
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

void LMWSynthEditor::layoutFader(Fader& f, juce::Rectangle<int> column) {
    column.reduce(4, 0);
    f.label.setBounds(column.removeFromTop(22));
    f.slider.setBounds(column);
}

void LMWSynthEditor::resized() {
    constexpr int colW = 80, gap = 10, header = 28;
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(36);
    keyboard.setBounds(area.removeFromBottom(90));
    area.removeFromBottom(10);

    // Groups: POLY (Voices) | VCO (Wave) | ENV (A D S R) | OUT (Volume)
    const std::array<int, 4> columns{1, 1, 4, 1};
    for (size_t i = 0; i < groups.size(); ++i) {
        groups[i].bounds = area.removeFromLeft(columns[i] * colW);
        area.removeFromLeft(gap);
    }
    auto inner = [&](size_t i) { return groups[i].bounds.reduced(0, 4).withTrimmedTop(header); };

    layoutFader(voices, inner(0));
    auto vco = inner(1).reduced(6, 0);
    waveLabel.setBounds(vco.removeFromTop(22));
    wave.setBounds(vco.removeFromTop(28));

    auto env = inner(2);
    for (Fader* f : {&attack, &decay, &sustain, &release}) layoutFader(*f, env.removeFromLeft(colW));
    layoutFader(gain, inner(3));
}
