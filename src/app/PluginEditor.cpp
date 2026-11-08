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
    setColour(juce::TooltipWindow::backgroundColourId, kBg);           // hover texts: black on white
    setColour(juce::TooltipWindow::textColourId, kFg);
    setColour(juce::TooltipWindow::outlineColourId, kFg);
    setColour(juce::TextButton::buttonColourId, kBg);                  // off: black on white
    setColour(juce::TextButton::textColourOffId, kFg);
    setColour(juce::TextButton::buttonOnColourId, kFg);                // on: white on black
    setColour(juce::TextButton::textColourOnId, kBg);
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

namespace {
const juce::Identifier kCompactProp{"uiCompact"};   // stored in the plugin/standalone state

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
}

void LMWLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                  juce::ComboBox& box) {
    const auto r = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r, 3.0f, box.hasKeyboardFocus(true) ? 2.0f : 1.0f);
    const float cx = static_cast<float>(width) - 9.0f, cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;                                   // small down-arrow in the 16 px arrow area
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.5f));
}

void LMWLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(2, 1, box.getWidth() - 18, box.getHeight() - 2);
    label.setBorderSize(juce::BorderSize<int>(0, 3, 0, 0));
    label.setFont(getComboBoxFont(box));
}

LMWSynthEditor::LMWSynthEditor(LMWSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      keyboard(p.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&lookAndFeel);
    namespace P = lmw::param;
    using lmw::dsp::In;
    const juce::String arrow = juce::String::fromUTF8("\xe2\x86\x92");

    // Hover texts (Spec S6.1): where each input comes from and what the fader does to it.
    auto fmTip = [this] {
        const float wheel = processor.modWheel();
        const float fm = processor.state().getRawParameterValue(lmw::param::fm)->load();
        return describeInput(In::VcoFm) + "\nVibrato depth = FM + mod wheel x 1 st"
             + "\nNow: FM " + juce::String(fm, 1) + " st + wheel " + pct(wheel) + " = "
             + juce::String(fm + wheel * lmw::dsp::modWheelFmSemitones, 1) + " st";
    };
    auto inputTip = [this](In in, const juce::String& what) {
        return [this, in, what] { return describeInput(in) + "\n" + what; };
    };

    // Spec S6.1: module panels. Row 0: POLY | VCO | MIXER | VCF | VCA | LFO | OUT; row 1: envelopes.
    // Titles name the normalled destination (S5.2) so the prewired patch is visible.
    groups = {
        {"POLY", 0, {addFader(P::voices, "Voices")}, {}},
        {"VCO", 0, {addChoice(P::wave, "Wave", {"Sine", "Saw", "Square", "Triangle"}),
                    addFader(P::fm, "FM", fmTip)}, {}},
        {"MIXER", 0, {addFader(P::mixVco, "VCO", inputTip(In::MixIn1, "Level of mixer input 1")),
                      addFader(P::mixNoise, "Noise", inputTip(In::MixIn2, "Level of mixer input 2"))}, {}},
        {"VCF", 0, {addFader(P::cutoff, "Cutoff", inputTip(In::VcfIn, "Lowpass 24 dB/oct cutoff")),
                    addFader(P::resonance, "Reso", inputTip(In::VcfIn, "Resonance; self-oscillates near the top")),
                    addFader(P::vcfEnv, "Env 2", inputTip(In::VcfCutoff, "Cutoff modulation depth in octaves"))}, {}},
        {"VCA", 0, {addFader(P::vcaInitial, "Initial", [this] {
                        return describeInput(In::VcaIn) + "\n" + describeInput(In::VcaCv)
                             + "\nGain = Initial + CV (0..1). Above 0, notes keep sounding after release.";
                    })}, {}},
        {"LFO", 0, {addChoice(P::lfoWave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),
                    addFader(P::lfoRate, "Rate", [] { return juce::String("Shared LFO; lfo.out feeds vco.fm (normalled)"); })}, {}},
        {"OUT", 0, {addFader(P::gain, "Volume", inputTip(In::OutIn, "All voices summed, then soft limiter"))}, {}},
        {"ENV 1 " + arrow + " VCA", 1,
            {addFader(P::env1Attack, "Attack"), addFader(P::env1Decay, "Decay"),
             addFader(P::env1Sustain, "Sustain"), addFader(P::env1Release, "Release")}, {}},
        {"ENV 2 " + arrow + " VCF", 1,
            {addFader(P::env2Attack, "Attack"), addFader(P::env2Decay, "Decay"),
             addFader(P::env2Sustain, "Sustain"), addFader(P::env2Release, "Release")}, {}},
    };
    jassert(usedFaders == faders.size() && usedChoices == choices.size());

    // Spec S6.4: compact view toggle, remembered in the saved state.
    compactButton.setClickingTogglesState(true);
    compactButton.setTooltip("Smaller margins, faders and value boxes");
    compactButton.onClick = [this] {
        processor.state().state.setProperty(kCompactProp, compactButton.getToggleState(), nullptr);
        setCompact(compactButton.getToggleState());
    };
    addAndMakeVisible(compactButton);

    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    addAndMakeVisible(keyboard);

    // Child widgets (e.g. slider value boxes) take their colours when created, before they had this
    // look-and-feel; re-send it so every child uses the black-on-white theme (Spec S6.2).
    sendLookAndFeelChange();

    const bool compact = processor.state().state.getProperty(kCompactProp, false);
    compactButton.setToggleState(compact, juce::dontSendNotification);
    setCompact(compact);                         // sets the window size

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

juce::String LMWSynthEditor::describeInput(lmw::dsp::In in) const {
    const auto& patch = processor.currentPatch();
    const auto idx = [](auto e) { return static_cast<std::size_t>(e); };
    const juce::String inName(lmw::dsp::inputNames[idx(in)].data(), lmw::dsp::inputNames[idx(in)].size());
    const auto src = patch.source(in);
    const juce::String srcName(lmw::dsp::outputNames[idx(src)].data(), lmw::dsp::outputNames[idx(src)].size());
    return "Input " + inName + " " + juce::String::fromUTF8("\xe2\x86\x90") + " " + srcName
         + (patch.isPatched(in) ? " (cable)" : " (normalled)");
}

LMWSynthEditor::Item LMWSynthEditor::addFader(const char* paramId, const juce::String& text,
                                              std::function<juce::String()> tip) {
    Fader& k = faders[usedFaders++];
    k.slider.setSliderSnapsToMousePosition(false);   // drag moves relative; a click does not jump the value
    k.slider.tip = std::move(tip);
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

void LMWSynthEditor::setCompact(bool compact) {
    m = compact ? compactMetrics : normalMetrics;
    for (std::size_t i = 0; i < usedFaders; ++i) {
        faders[i].slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, m.boxW, m.boxH);
        faders[i].label.setFont(juce::FontOptions(m.labelFont));
    }
    for (std::size_t i = 0; i < usedChoices; ++i) choices[i].label.setFont(juce::FontOptions(m.labelFont));

    int columns[2] = {0, 0}, groupsInRow[2] = {0, 0};
    for (const auto& g : groups) { columns[g.row] += static_cast<int>(g.items.size()); ++groupsInRow[g.row]; }
    int width = 0;
    for (int r = 0; r < 2; ++r) width = std::max(width, columns[r] * m.colW + (groupsInRow[r] - 1) * m.gap);
    const int pad = m.gap + 2;
    setSize(pad + width + pad, pad + m.titleH + 2 * m.rowH + m.gap + m.gap + m.keysH + pad);
    resized();
    repaint();
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kFg);
    const int pad = m.gap + 2;
    g.setFont(juce::FontOptions(m.titleH * 0.6f, juce::Font::bold));
    g.drawText("LMW-Synth", juce::Rectangle<int>(pad, pad, getWidth() - 2 * pad, m.titleH), juce::Justification::centredLeft);
    const float lineY = static_cast<float>(pad + m.titleH + m.gap / 2);   // between title row and modules
    g.drawLine(static_cast<float>(pad), lineY, static_cast<float>(getWidth() - pad), lineY, 1.0f);

    g.setFont(juce::FontOptions(m.titleFont, juce::Font::bold));
    for (const auto& grp : groups) {
        g.drawRect(grp.bounds, 1);
        g.drawText(grp.title, grp.bounds.withHeight(m.header - 4), juce::Justification::centred);
        g.drawHorizontalLine(grp.bounds.getY() + m.header - 4, static_cast<float>(grp.bounds.getX()),
                             static_cast<float>(grp.bounds.getRight()));
    }
}

void LMWSynthEditor::layoutItem(const Item& item, juce::Rectangle<int> column) const {
    column.reduce(m.colW >= 80 ? 4 : 2, 0);
    if (item.fader != nullptr) {
        item.fader->label.setBounds(column.removeFromTop(m.labelH));
        item.fader->slider.setBounds(column);
    } else if (item.choice != nullptr) {
        column.reduce(2, 0);
        item.choice->label.setBounds(column.removeFromTop(m.labelH));
        item.choice->box.setBounds(column.removeFromTop(m.boxH + 2));
    }
}

void LMWSynthEditor::resized() {
    const int pad = m.gap + 2;
    auto area = getLocalBounds().reduced(pad);
    auto title = area.removeFromTop(m.titleH);
    compactButton.setBounds(title.removeFromRight(90).reduced(0, 2));
    area.removeFromTop(m.gap);
    keyboard.setBounds(area.removeFromBottom(m.keysH));
    area.removeFromBottom(m.gap);

    std::array<juce::Rectangle<int>, 2> rows;
    rows[0] = area.removeFromTop(m.rowH);
    area.removeFromTop(m.gap);
    rows[1] = area.removeFromTop(m.rowH);

    for (auto& grp : groups) {
        auto& row = rows[static_cast<std::size_t>(grp.row)];
        grp.bounds = row.removeFromLeft(static_cast<int>(grp.items.size()) * m.colW);
        row.removeFromLeft(m.gap);
        auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
        for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
    }
}
