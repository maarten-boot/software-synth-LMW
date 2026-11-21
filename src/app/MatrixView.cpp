// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MatrixView.h"
#include "ui/Theme.h"
#include <cmath>

namespace {
juce::Colour colour(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
const juce::Colour kBg = colour(lmw::ui::theme::background);
const juce::Colour kFg = colour(lmw::ui::theme::foreground);

juce::String name(std::string_view s) { return juce::String(s.data(), s.size()); }
juce::String percent(float level) { return juce::String(juce::roundToInt(level * 100.0f)) + " %"; }
// Module part of a port name ("vcf1.cutoff" -> "vcf1"): a thicker line where it changes.
juce::String module(std::string_view s) { return name(s.substr(0, s.find('.'))); }
}

MatrixView::MatrixView(std::function<const lmw::dsp::Patch&()> p, std::function<bool(int, int, float)> set)
    : patch(std::move(p)), setPin(std::move(set)) {
    setWantsKeyboardFocus(true);
    setTitle("Patch matrix");
    setDescription("Rows: inputs, columns: outputs. Arrow keys move, Space toggles, plus and minus change the level, "
                   "Shift+F10 opens the cell menu.");
    levelLabel.setJustificationType(juce::Justification::centredTop);
    levelLabel.setColour(juce::Label::textColourId, kFg);
    levelLabel.setColour(juce::Label::backgroundColourId, kBg);
    addAndMakeVisible(levelLabel);
    levelSlider.setRange(0.0, 100.0, 1.0);
    levelSlider.setTextValueSuffix(" %");
    levelSlider.setTitle("Cable level");
    levelSlider.setTooltip("Level of the cable in the selected cell, 0-100 %; 0 removes it.\nScale: linear");   // S6.8
    levelSlider.setWantsKeyboardFocus(true);              // Tab from the matrix; Up/Down change the level
    levelSlider.onValueChange = [this] {
        if (!syncing) this->set(cursor, static_cast<float>(levelSlider.getValue()) / 100.0f);   // not the ctor argument
    };
    addAndMakeVisible(levelSlider);
    syncSlider();
}

void MatrixView::resized() {
    auto right = getLocalBounds().removeFromRight(sliderWidth()).withTrimmedTop(headerHeight()).reduced(6, 4);
    levelLabel.setBounds(right.removeFromTop(juce::roundToInt(fontSize * 5.0f)));
    levelSlider.setBounds(right.withSizeKeepingCentre(std::min(right.getWidth(), juce::roundToInt(fontSize * 6.0f)),
                                                      right.getHeight()));
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, juce::roundToInt(fontSize * 5.0f),
                                juce::roundToInt(fontSize * 1.8f));
}

void MatrixView::refresh() { syncSlider(); repaint(); }

// The slider follows the cursor cell: its level, or off where a level cannot be set.
void MatrixView::syncSlider() {
    const auto in = static_cast<lmw::dsp::In>(cursor.input);
    const float l = level(cursor);
    const juce::String link = name(lmw::dsp::outputNames[static_cast<std::size_t>(cursor.output)]) + "\n"
                            + juce::String::fromUTF8("\xe2\x86\x93") + "\n"
                            + name(lmw::dsp::inputNames[static_cast<std::size_t>(cursor.input)]);
    juce::String why;
    if (!lmw::dsp::canConnect(in, static_cast<lmw::dsp::Out>(cursor.output))) why = "synth and effects do not connect";   // S7.6
    else if (isGate(cursor.input)) why = "on/off only";
    else if (l <= 0.0f && patch().pinCount(in) >= lmw::dsp::Patch::maxPins) why = "input full (4 cables)";
    levelLabel.setFont(juce::FontOptions(fontSize));
    levelLabel.setText(link + (why.isEmpty() ? juce::String() : "\n(" + why + ")"), juce::dontSendNotification);
    const juce::ScopedValueSetter<bool> s(syncing, true);
    levelSlider.setValue(std::round(l * 100.0f), juce::sendNotificationSync);
    levelSlider.setEnabled(why.isEmpty());
    levelSlider.setTooltip((why.isEmpty() ? "Level of the cable " + link.replace("\n", " ") + " (0 % removes it)"
                                          : "No level here: " + why) + "\nScale: linear");   // S6.1
}

bool MatrixView::isGate(int input) noexcept {
    return input == static_cast<int>(lmw::dsp::In::Env1Gate) || input == static_cast<int>(lmw::dsp::In::Env2Gate);
}

int MatrixView::cellWidth() const { return std::max(24, (getWidth() - labelWidth() - sliderWidth() - 1) / lmw::dsp::numOutputs); }
int MatrixView::cellHeight() const { return std::max(14, (getHeight() - headerHeight() - 1) / lmw::dsp::numInputs); }

juce::Rectangle<int> MatrixView::cellBounds(Cell c) const {
    return {labelWidth() + c.output * cellWidth(), headerHeight() + c.input * cellHeight(), cellWidth(), cellHeight()};
}

std::optional<MatrixView::Cell> MatrixView::cellAt(juce::Point<int> p) const {
    const int col = (p.x - labelWidth()) / cellWidth(), row = (p.y - headerHeight()) / cellHeight();
    if (p.x < labelWidth() || p.y < headerHeight() || col >= lmw::dsp::numOutputs || row >= lmw::dsp::numInputs) return std::nullopt;
    return Cell{row, col};
}

int MatrixView::rowAt(juce::Point<int> p) const {
    const int row = (p.y - headerHeight()) / cellHeight();
    if (p.y < headerHeight() || row >= lmw::dsp::numInputs || p.x < 0 || p.x >= labelWidth() + lmw::dsp::numOutputs * cellWidth()) return -1;
    return row;
}

int MatrixView::columnAt(juce::Point<int> p) const {
    const int col = (p.x - labelWidth()) / cellWidth();
    if (p.x < labelWidth() || col >= lmw::dsp::numOutputs || p.y < 0 || p.y >= headerHeight() + lmw::dsp::numInputs * cellHeight()) return -1;
    return col;
}

float MatrixView::level(Cell c) const {
    return patch().level(static_cast<lmw::dsp::In>(c.input), static_cast<lmw::dsp::Out>(c.output));
}

void MatrixView::set(Cell c, float l) {
    l = juce::jlimit(0.0f, 1.0f, std::round(l * 100.0f) / 100.0f);   // whole percent
    if (!lmw::dsp::canConnect(static_cast<lmw::dsp::In>(c.input), static_cast<lmw::dsp::Out>(c.output))) return;   // S7.6
    if (isGate(c.input)) l = l > 0.0f ? 1.0f : 0.0f;                  // gates: on/off
    if (std::fabs(l - level(c)) < 1e-6f) return;           // unchanged
    setPin(c.input, c.output, l);
    syncSlider();
    repaint();
}

void MatrixView::paint(juce::Graphics& g) {
    using namespace lmw::dsp;
    g.fillAll(kBg);
    g.setColour(kFg);
    const juce::FontOptions font(fontSize), bold(fontSize, juce::Font::bold);
    const int lw = labelWidth(), hh = headerHeight(), cw = cellWidth(), ch = cellHeight();
    const int right = lw + numOutputs * cw, bottom = hh + numInputs * ch;

    // Column headers: output names on two lines ("vcf1" / "out"); row labels: input names. Both on their
    // unit's colour (Spec S6.9; white for keys, mix, out and at the Max preset).
    // Highlighted (user 2026-11-21): under the mouse or pinned by a click on the label — a grey band over the
    // cells and the label inverted; the selected cell: its labels bold and framed.
    const bool focused = hasKeyboardFocus(true);
    auto rowLit = [&](int i) { return i == hoverRow || i == pinnedRow; };
    auto columnLit = [&](int o) { return o == hoverColumn || o == pinnedColumn; };
    g.setColour(colour(palette.band));
    for (int i = 0; i < numInputs; ++i) if (rowLit(i)) g.fillRect(lw, hh + i * ch, numOutputs * cw, ch);
    for (int o = 0; o < numOutputs; ++o) if (columnLit(o)) g.fillRect(lw + o * cw, hh, cw, numInputs * ch);
    auto unitFill = [&](std::string_view port) { return colour(palette.unitColour(lmw::ui::theme::unitOf(port))); };
    for (int o = 0; o < numOutputs; ++o) {
        g.setColour(columnLit(o) ? kFg : unitFill(outputNames[static_cast<std::size_t>(o)]));
        g.fillRect(lw + o * cw, 0, cw, hh);
    }
    for (int i = 0; i < numInputs; ++i) {
        g.setColour(rowLit(i) ? kFg : unitFill(inputNames[static_cast<std::size_t>(i)]));
        g.fillRect(0, hh + i * ch, lw, ch);
    }
    g.setFont(bold);
    for (int o = 0; o < numOutputs; ++o) {
        const auto n = outputNames[static_cast<std::size_t>(o)];
        const auto dot = n.find('.');
        auto area = juce::Rectangle<int>(lw + o * cw, 0, cw, hh).reduced(1, 2);
        g.setColour(columnLit(o) ? kBg : kFg);
        g.drawText(name(n.substr(0, dot)), area.removeFromTop(area.getHeight() / 2), juce::Justification::centredBottom, true);
        g.drawText(name(n.substr(dot + 1)), area, juce::Justification::centredTop, true);
    }
    for (int i = 0; i < numInputs; ++i) {
        g.setFont(font);                                    // not bold: long names would be cut off
        g.setColour(rowLit(i) ? kBg : kFg);
        g.drawText(name(inputNames[static_cast<std::size_t>(i)]) + (isGate(i) ? " (on/off)" : ""),
                   juce::Rectangle<int>(0, hh + i * ch, lw - 6, ch), juce::Justification::centredRight, true);
    }
    g.setColour(kFg);
    g.drawRect(juce::Rectangle<int>(0, hh + cursor.input * ch, lw, ch).reduced(1), 2);          // selected cell's labels
    g.drawRect(juce::Rectangle<int>(lw + cursor.output * cw, 0, cw, hh).reduced(1), 2);
    if (rowLit(cursor.input)) { g.setColour(kBg); g.drawRect(juce::Rectangle<int>(0, hh + cursor.input * ch, lw, ch).reduced(3), 1); }
    if (columnLit(cursor.output)) { g.setColour(kBg); g.drawRect(juce::Rectangle<int>(lw + cursor.output * cw, 0, cw, hh).reduced(3), 1); }
    g.setColour(kFg);
    g.setFont(font);

    // Cells.
    const auto& p = patch();
    for (int i = 0; i < numInputs; ++i) {
        const auto in = static_cast<In>(i);
        for (int o = 0; o < numOutputs; ++o) {
            const auto b = cellBounds({i, o}).reduced(2);
            const float l = p.level(in, static_cast<Out>(o));
            if (l > 0.0f) {
                // Black bar filled to the level; the % text white on black and black on white (clipped).
                const auto fill = b.withWidth(std::max(2, juce::roundToInt(static_cast<float>(b.getWidth()) * l)));
                g.setFont(font);
                g.setColour(kFg);
                g.drawText(percent(l), b, juce::Justification::centred, false);
                g.fillRect(fill);
                {
                    juce::Graphics::ScopedSaveState s(g);
                    g.reduceClipRegion(fill);
                    g.setColour(kBg);
                    g.drawText(percent(l), b, juce::Justification::centred, false);
                }
                g.setColour(kFg);
                g.drawRect(b, 1);
            } else if (!p.isPatched(in) && p.normalLevel(in, static_cast<Out>(o)) > 0.0f) {
                g.drawEllipse(b.toFloat().withSizeKeepingCentre(9.0f, 9.0f), 1.5f);    // normalled: ring
            } else if (!canConnect(in, static_cast<Out>(o))) {                          // S7.6: other domain
                const auto r = b.toFloat().reduced(4.0f);
                g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);
            }
        }
    }

    // Grid: thin lines, thicker between modules.
    for (int o = 0; o <= numOutputs; ++o) {
        const bool edge = o == 0 || o == numOutputs
                       || module(outputNames[static_cast<std::size_t>(o)]) != module(outputNames[static_cast<std::size_t>(o - 1)]);
        g.fillRect(lw + o * cw - (edge ? 1 : 0), 0 + (edge ? 0 : hh), edge ? 2 : 1, edge ? bottom : bottom - hh);
    }
    for (int i = 0; i <= numInputs; ++i) {
        const bool edge = i == 0 || i == numInputs
                       || module(inputNames[static_cast<std::size_t>(i)]) != module(inputNames[static_cast<std::size_t>(i - 1)]);
        g.fillRect(edge ? 0 : lw, hh + i * ch - (edge ? 1 : 0), edge ? right : right - lw, edge ? 2 : 1);
    }

    if (focused) g.drawRect(cellBounds(cursor).expanded(1), 3);    // cell cursor (also while the slider has focus)
}

void MatrixView::mouseMove(const juce::MouseEvent& e) {
    const int r = rowAt(e.getPosition()), c = columnAt(e.getPosition());
    if (r != hoverRow || c != hoverColumn) { hoverRow = r; hoverColumn = c; repaint(); }
}

void MatrixView::mouseExit(const juce::MouseEvent&) {
    if (hoverRow >= 0 || hoverColumn >= 0) { hoverRow = hoverColumn = -1; repaint(); }
}

void MatrixView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    dragCell = cellAt(e.getPosition());
    if (!dragCell && !e.mods.isPopupMenu()) {               // a row or column label: pin / release its band
        const auto p = e.getPosition();
        if (p.x < labelWidth() && rowAt(p) >= 0) pinnedRow = pinnedRow == rowAt(p) ? -1 : rowAt(p);
        else if (p.y < headerHeight() && columnAt(p) >= 0) pinnedColumn = pinnedColumn == columnAt(p) ? -1 : columnAt(p);
        repaint();
        return;
    }
    dragged = false;
    if (dragCell) { cursor = *dragCell; dragStartLevel = level(*dragCell); syncSlider(); repaint(); }
    if (dragCell && e.mods.isPopupMenu()) {                 // right-click: the cell menu, no drag
        const auto c = *dragCell;
        dragCell.reset();
        showCellMenu(c);
    }
}

void MatrixView::mouseDrag(const juce::MouseEvent& e) {
    mouse = e.getPosition();
    if (!dragCell || isGate(dragCell->input)) return;
    if (!dragged && std::abs(e.getDistanceFromDragStartY()) < 3) return;
    dragged = true;
    // 1 % per 1.5 pixels; up = more.
    set(*dragCell, dragStartLevel - static_cast<float>(e.getDistanceFromDragStartY()) / 150.0f);
}

// Phase 17f (user 2026-11-21): a click selects; only an empty cell gets a cable (100 %, a gate "on").
void MatrixView::mouseUp(const juce::MouseEvent& e) {
    if (dragCell && !dragged && e.getNumberOfClicks() == 1 && cellAt(e.getPosition()) == std::optional<Cell>(*dragCell)
        && level(*dragCell) <= 0.0f) {
        set(*dragCell, 1.0f);
        if (level(*dragCell) > 0.0f) { justMade = *dragCell; justMadeAt = juce::Time::getMillisecondCounter(); }
    }
    dragCell.reset();
}

// Double-click removes a cable, except the one the first click of this double-click just made.
void MatrixView::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto c = cellAt(e.getPosition());
    if (!c || e.mods.isPopupMenu()) return;
    const bool fresh = justMade == c
                    && juce::Time::getMillisecondCounter() - justMadeAt <= static_cast<juce::uint32>(juce::MouseEvent::getDoubleClickTimeout()) + 200;
    justMade.reset();
    if (!fresh && level(*c) > 0.0f) set(*c, 0.0f);
}

// The cell menu (right-click, Shift+F10): only what is possible here (S6.2).
void MatrixView::showCellMenu(Cell c) {
    const auto in = static_cast<lmw::dsp::In>(c.input);
    if (!lmw::dsp::canConnect(in, static_cast<lmw::dsp::Out>(c.output))) return;
    const float l = level(c);
    const bool full = l <= 0.0f && patch().pinCount(in) >= lmw::dsp::Patch::maxPins;
    juce::PopupMenu menu;
    menu.addSectionHeader(name(lmw::dsp::outputNames[static_cast<std::size_t>(c.output)]) + " "
                          + juce::String::fromUTF8("\xe2\x86\x92") + " "
                          + name(lmw::dsp::inputNames[static_cast<std::size_t>(c.input)]));
    if (isGate(c.input)) {
        if (l <= 0.0f && !full) menu.addItem(2, "On");
    } else if (!full) {
        if (l < 1.0f) menu.addItem(2, "100 %");
        menu.addItem(3, "Type level...");
    }
    if (l > 0.0f) menu.addItem(1, isGate(c.input) ? "Off (remove cable)" : "Remove cable");
    if (full && l <= 0.0f) menu.addItem(4, "Input full (4 cables)", false);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                           .withTargetScreenArea(localAreaToGlobal(cellBounds(c))),
                       [safe = juce::Component::SafePointer<MatrixView>(this), c](int result) {
                           if (safe == nullptr) return;
                           safe->grabKeyboardFocus();               // computer keys keep playing (S3.2)
                           if (result == 1) safe->set(c, 0.0f);
                           else if (result == 2) safe->set(c, 1.0f);
                           else if (result == 3) safe->typeLevel(c);
                       });
}

void MatrixView::typeLevel(Cell c) {
    auto* w = new juce::AlertWindow("Cable level",
                                    name(lmw::dsp::outputNames[static_cast<std::size_t>(c.output)]) + " "
                                        + juce::String::fromUTF8("\xe2\x86\x92") + " "
                                        + name(lmw::dsp::inputNames[static_cast<std::size_t>(c.input)])
                                        + "\nLevel in percent (0 removes the cable):",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("level", juce::String(juce::roundToInt(level(c) * 100.0f)));
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<MatrixView>(this), w, c](int result) {
            if (safe == nullptr || result != 1) return;
            const auto text = w->getTextEditorContents("level").trim().trimCharactersAtEnd("% ");
            if (text.isEmpty() || !text.containsOnly("0123456789.")) return;
            safe->set(c, static_cast<float>(text.getDoubleValue()) / 100.0f);
        }), true);
    if (auto* field = w->getTextEditor("level")) { field->grabKeyboardFocus(); field->selectAll(); }   // type at once
}

bool MatrixView::keyPressed(const juce::KeyPress& key) {
    const int code = key.getKeyCode();
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    auto move = [this](int di, int dout) {
        cursor.input = juce::jlimit(0, lmw::dsp::numInputs - 1, cursor.input + di);
        cursor.output = juce::jlimit(0, lmw::dsp::numOutputs - 1, cursor.output + dout);
        syncSlider();
        repaint();
        return true;
    };
    if (code == juce::KeyPress::F10Key && mods.isShiftDown()) { showCellMenu(cursor); return true; }
    if (code == juce::KeyPress::upKey)    return move(-1, 0);
    if (code == juce::KeyPress::downKey)  return move(1, 0);
    if (code == juce::KeyPress::leftKey)  return move(0, -1);
    if (code == juce::KeyPress::rightKey) return move(0, 1);
    if (code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey) {
        set(cursor, level(cursor) > 0.0f ? 0.0f : 1.0f);
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey || code == juce::KeyPress::endKey) {
        set(cursor, 0.0f);
        return true;
    }
    if (code == juce::KeyPress::homeKey) { set(cursor, 1.0f); return true; }
    const float unit = isGate(cursor.input) ? 1.0f : 0.0f;              // gates: any step toggles fully
    if (code == juce::KeyPress::pageUpKey)   { set(cursor, level(cursor) + std::max(unit, 0.01f)); return true; }
    if (code == juce::KeyPress::pageDownKey) { set(cursor, level(cursor) - std::max(unit, 0.01f)); return true; }
    const auto c = key.getTextCharacter();                             // '+' needs Shift on many layouts
    if (c == '+' || c == '=' || c == '-' || c == '_') {
        const float sign = (c == '+' || c == '=') ? 1.0f : -1.0f;
        set(cursor, level(cursor) + sign * std::max(unit, 0.1f));
        return true;
    }
    return false;                        // QWERTY note keys: handled by the editor
}

juce::String MatrixView::getTooltip() {
    const auto mp = getMouseXYRelative();
    const auto c = cellAt(mp);
    if (!c) {
        if (mp.x < labelWidth() && rowAt(mp) >= 0)
            return name(lmw::dsp::inputNames[static_cast<std::size_t>(rowAt(mp))]) + "\nClick: keep this row highlighted (click again to release).";
        if (mp.y < headerHeight() && columnAt(mp) >= 0)
            return name(lmw::dsp::outputNames[static_cast<std::size_t>(columnAt(mp))]) + "\nClick: keep this column highlighted (click again to release).";
        return {};
    }
    const auto in = static_cast<lmw::dsp::In>(c->input);
    const auto out = static_cast<lmw::dsp::Out>(c->output);
    const auto& p = patch();
    const juce::String link = name(lmw::dsp::outputNames[static_cast<std::size_t>(c->output)]) + " "
                            + juce::String::fromUTF8("\xe2\x86\x92") + " " + name(lmw::dsp::inputNames[static_cast<std::size_t>(c->input)]);
    const float l = p.level(in, out);
    juce::String state;
    if (!lmw::dsp::canConnect(in, out))                                      // Spec S7.6
        return link + ": not possible (effect sockets connect only to effect sockets, synth sockets only to synth sockets)";
    if (l > 0.0f) state = "cable at " + percent(l);
    else if (const float n = p.isPatched(in) ? 0.0f : p.normalLevel(in, out); n > 0.0f)
        state = "normalled" + (n < 1.0f ? " at " + percent(n) : juce::String()) + " (used while this input has no cables)";
    else state = "no cable";
    juce::String how = isGate(c->input)
        ? (l > 0.0f ? "Double-click: off. Right-click: menu." : "Click: on. Right-click: menu.")
        : (l > 0.0f ? "Click: select (the slider shows the level). Drag up/down: level. Double-click: remove. Right-click: menu."
                    : "Click: cable at 100 %. Drag up/down: level. Right-click: menu.");
    if (l <= 0.0f && p.pinCount(in) >= lmw::dsp::Patch::maxPins) how = "This input already has 4 cables.";
    return link + ": " + state + "\n" + how;
}
