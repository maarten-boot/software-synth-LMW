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
const juce::Identifier kLayoutProp{"uiLayout"};     // rack layout (Spec S6.5), RackLayout::serialize()
constexpr int kMaxRows = 4;

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

    // Spec S6.5: rack. A row holds at most as many columns as the widest default row, so the window
    // width never changes; panels can be moved into up to 4 rows. The layout is remembered.
    {
        std::vector<int> columns;
        lmw::ui::RackLayout::Rows defaultRows(2);
        for (std::size_t i = 0; i < groups.size(); ++i) {
            columns.push_back(static_cast<int>(groups[i].items.size()));
            defaultRows[static_cast<std::size_t>(groups[i].row)].push_back(static_cast<int>(i));
            headers.push_back(std::make_unique<PanelHeader>(*this, static_cast<int>(i)));
            addAndMakeVisible(*headers.back());
        }
        int maxColumns = 0;
        for (const auto& row : defaultRows) {
            int c = 0;
            for (int g : row) c += columns[static_cast<std::size_t>(g)];
            maxColumns = std::max(maxColumns, c);
        }
        rack.emplace(columns, defaultRows, maxColumns, kMaxRows);
        rack->parse(processor.state().state.getProperty(kLayoutProp).toString().toStdString());
    }
    resetLayoutButton.setTooltip("Put all module panels back in their original places");
    resetLayoutButton.onClick = [this] {
        rack->reset();
        layoutChanged(-1);
    };
    addAndMakeVisible(resetLayoutButton);

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
    updateSize();
}

void LMWSynthEditor::updateSize() {
    // Width of the widest row, at least that of the default layout, so moving panels never makes
    // the window narrower (Spec S6.5).
    int width = 0;
    for (const auto* rows : {&rack->rows(), &rack->defaultRows()})
        for (const auto& row : *rows)
            width = std::max(width, rack->rowColumns(row) * m.colW + (static_cast<int>(row.size()) - 1) * m.gap);
    const int rows = static_cast<int>(rack->rows().size());
    const int pad = m.gap + 2;
    // title, gap, rows with gaps between, gap, keyboard
    setSize(pad + width + pad, pad + m.titleH + m.gap + rows * m.rowH + (rows - 1) * m.gap + m.gap + m.keysH + pad);
    resized();
    repaint();
}

// ---- Spec S6.5: rack (movable module panels) ----

LMWSynthEditor::PanelHeader::PanelHeader(LMWSynthEditor& e, int panelIndex) : editor(e), panel(panelIndex) {
    const auto& title = editor.groups[static_cast<std::size_t>(panel)].title;
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTitle(title + " panel");
    setDescription("Drag to move. Arrow keys move the panel; Enter opens the move menu.");
    setTooltip("Drag to move this panel.\nArrow keys (when selected) or right-click: move menu.");
}

void LMWSynthEditor::PanelHeader::paint(juce::Graphics& g) {
    g.setColour(kFg);
    g.setFont(juce::FontOptions(editor.m.titleFont, juce::Font::bold));
    g.drawText(editor.groups[static_cast<std::size_t>(panel)].title, getLocalBounds(), juce::Justification::centred);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
    if (hasKeyboardFocus(false)) g.drawRect(getLocalBounds().withTrimmedBottom(1), 2);   // focus ring
}

void LMWSynthEditor::PanelHeader::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) editor.showPanelMenu(panel);
}

void LMWSynthEditor::PanelHeader::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || (!dragging && e.getDistanceFromDragStart() < 4)) return;
    dragging = true;
    editor.dragPanel(panel, e.getEventRelativeTo(&editor).getPosition());
}

void LMWSynthEditor::PanelHeader::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.dropPanel(panel, e.getEventRelativeTo(&editor).getPosition());
}

bool LMWSynthEditor::PanelHeader::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && dragging) { dragging = false; editor.cancelDrag(); return true; }
    if (key.getModifiers().isAnyModifierKeyDown() && !key.getModifiers().isShiftDown()) return false;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey)  { editor.movePanel(panel, 0); return true; }
    if (code == juce::KeyPress::rightKey) { editor.movePanel(panel, 1); return true; }
    if (code == juce::KeyPress::upKey)    { editor.movePanel(panel, 2); return true; }
    if (code == juce::KeyPress::downKey)  { editor.movePanel(panel, 3); return true; }
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())) {
        editor.showPanelMenu(panel);
        return true;
    }
    return false;                        // e.g. QWERTY note keys: handled by the editor
}

std::optional<LMWSynthEditor::DropTarget> LMWSynthEditor::findDrop(int panel, juce::Point<int> p) const {
    if (rowBounds.empty()) return std::nullopt;
    const auto& rows = rack->rows();
    const int nRows = static_cast<int>(rows.size());
    const auto& first = rowBounds.front();
    DropTarget t;
    if (p.y < first.getY()) {                                   // above the first row: new top row
        t.newRow = true;
        t.row = 0;
    } else if (p.y >= rowBounds.back().getBottom()) {           // below the last row: new bottom row
        t.newRow = true;
        t.row = nRows;
    } else {
        for (int r = 0; r < nRows; ++r)
            if (p.y < rowBounds[static_cast<std::size_t>(r)].getBottom() + m.gap) { t.row = r; break; }
    }
    if (t.newRow) {
        const int y = t.row == 0 ? first.getY() - m.gap / 2 : rowBounds.back().getBottom() + m.gap / 2;
        t.marker = {first.getX(), y - 2, getWidth() - 2 * first.getX(), 4};
    } else {
        const auto& rowArea = rowBounds[static_cast<std::size_t>(t.row)];
        int x = rowArea.getX() - m.gap / 2;
        for (int q : rows[static_cast<std::size_t>(t.row)]) {    // insert before the first panel right of p
            if (q == panel) continue;
            const auto& b = groups[static_cast<std::size_t>(q)].bounds;
            if (p.x <= b.getCentreX()) break;
            ++t.index;
            x = b.getRight() + m.gap / 2;
        }
        t.marker = {x - 2, rowArea.getY(), 4, rowArea.getHeight()};
    }
    const auto result = rack->preview(panel, t.row, t.index, t.newRow);
    if (!result || *result == rows) return std::nullopt;        // not allowed (row full) or no change
    return t;
}

void LMWSynthEditor::dragPanel(int panel, juce::Point<int> where) {
    draggedPanel = panel;
    drop = findDrop(panel, where);
    repaint();
}

void LMWSynthEditor::dropPanel(int panel, juce::Point<int> where) {
    const auto t = findDrop(panel, where);
    draggedPanel = -1;
    drop.reset();
    if (t && rack->move(panel, t->row, t->index, t->newRow)) layoutChanged(panel);
    else repaint();
}

void LMWSynthEditor::cancelDrag() {
    draggedPanel = -1;
    drop.reset();
    repaint();
}

bool LMWSynthEditor::movePanel(int panel, int direction) {
    bool moved = false;
    switch (direction) {
        case 0: moved = rack->moveLeft(panel); break;
        case 1: moved = rack->moveRight(panel); break;
        case 2: moved = rack->moveUp(panel); break;
        case 3: moved = rack->moveDown(panel); break;
        default: break;
    }
    if (moved) layoutChanged(panel);
    return moved;
}

void LMWSynthEditor::showPanelMenu(int panel) {
    const auto can = [this, panel](int direction) {
        auto copy = *rack;
        switch (direction) {
            case 0: return copy.moveLeft(panel);
            case 1: return copy.moveRight(panel);
            case 2: return copy.moveUp(panel);
            default: return copy.moveDown(panel);
        }
    };
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);              // black on white like the panel (Spec S6.2)
    menu.addSectionHeader(groups[static_cast<std::size_t>(panel)].title);
    menu.addItem(1, "Move left", can(0));
    menu.addItem(2, "Move right", can(1));
    menu.addItem(3, "Move up", can(2));
    menu.addItem(4, "Move down", can(3));
    menu.addSeparator();
    menu.addItem(5, "Reset layout");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(headers[static_cast<std::size_t>(panel)].get()),
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), panel](int result) {
                           if (safe == nullptr) return;
                           if (result >= 1 && result <= 4) {
                               safe->movePanel(panel, result - 1);
                           } else if (result == 5) {
                               safe->rack->reset();
                               safe->layoutChanged(panel);
                           }
                       });
}

void LMWSynthEditor::layoutChanged(int focusPanel) {
    processor.state().state.setProperty(kLayoutProp, juce::String(rack->serialize()), nullptr);
    updateSize();
    if (focusPanel >= 0) {
        auto& h = *headers[static_cast<std::size_t>(focusPanel)];
        if (h.isShowing()) h.grabKeyboardFocus();
    }
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kFg);
    const int pad = m.gap + 2;
    g.setFont(juce::FontOptions(m.titleH * 0.6f, juce::Font::bold));
    g.drawText("LMW-Synth", juce::Rectangle<int>(pad, pad, getWidth() - 2 * pad, m.titleH), juce::Justification::centredLeft);
    const float lineY = static_cast<float>(pad + m.titleH + m.gap / 2);   // between title row and modules
    g.drawLine(static_cast<float>(pad), lineY, static_cast<float>(getWidth() - pad), lineY, 1.0f);

    for (std::size_t i = 0; i < groups.size(); ++i)          // titles are drawn by the PanelHeaders
        g.drawRect(groups[i].bounds, static_cast<int>(i) == draggedPanel ? 3 : 1);
    if (drop) g.fillRect(drop->marker);                     // drop marker (Spec S6.5)
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
    if (!rack) return;
    const int pad = m.gap + 2;
    auto area = getLocalBounds().reduced(pad);
    auto title = area.removeFromTop(m.titleH);
    compactButton.setBounds(title.removeFromRight(90).reduced(0, 2));
    title.removeFromRight(m.gap);
    resetLayoutButton.setBounds(title.removeFromRight(110).reduced(0, 2));
    area.removeFromTop(m.gap);
    keyboard.setBounds(area.removeFromBottom(m.keysH));
    area.removeFromBottom(m.gap);

    // Spec S6.5: panels in the order of the rack layout.
    rowBounds.clear();
    for (const auto& rowPanels : rack->rows()) {
        if (!rowBounds.empty()) area.removeFromTop(m.gap);
        auto row = area.removeFromTop(m.rowH);
        rowBounds.push_back(row);
        for (int index : rowPanels) {
            auto& grp = groups[static_cast<std::size_t>(index)];
            grp.bounds = row.removeFromLeft(static_cast<int>(grp.items.size()) * m.colW);
            row.removeFromLeft(m.gap);
            headers[static_cast<std::size_t>(index)]->setBounds(grp.bounds.getX() + 1, grp.bounds.getY() + 1,
                                                                 grp.bounds.getWidth() - 2, m.header - 4);
            auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
            for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
        }
    }
}
