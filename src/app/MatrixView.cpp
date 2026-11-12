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
    setDescription("Rows: inputs, columns: outputs. Arrow keys move, Space toggles, plus and minus change the level.");
}

bool MatrixView::isGate(int input) noexcept {
    return input == static_cast<int>(lmw::dsp::In::Env1Gate) || input == static_cast<int>(lmw::dsp::In::Env2Gate);
}

int MatrixView::cellWidth() const { return std::max(24, (getWidth() - labelWidth() - 1) / lmw::dsp::numOutputs); }
int MatrixView::cellHeight() const { return std::max(14, (getHeight() - headerHeight() - 1) / lmw::dsp::numInputs); }

juce::Rectangle<int> MatrixView::cellBounds(Cell c) const {
    return {labelWidth() + c.output * cellWidth(), headerHeight() + c.input * cellHeight(), cellWidth(), cellHeight()};
}

std::optional<MatrixView::Cell> MatrixView::cellAt(juce::Point<int> p) const {
    const int col = (p.x - labelWidth()) / cellWidth(), row = (p.y - headerHeight()) / cellHeight();
    if (p.x < labelWidth() || p.y < headerHeight() || col >= lmw::dsp::numOutputs || row >= lmw::dsp::numInputs) return std::nullopt;
    return Cell{row, col};
}

float MatrixView::level(Cell c) const {
    return patch().level(static_cast<lmw::dsp::In>(c.input), static_cast<lmw::dsp::Out>(c.output));
}

void MatrixView::set(Cell c, float l) {
    l = juce::jlimit(0.0f, 1.0f, std::round(l * 100.0f) / 100.0f);   // whole percent
    if (isGate(c.input)) l = l > 0.0f ? 1.0f : 0.0f;                  // gates: on/off
    if (std::fabs(l - level(c)) < 1e-6f) return;           // unchanged
    setPin(c.input, c.output, l);
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
    auto unitFill = [&](std::string_view port) { return colour(palette.unitColour(lmw::ui::theme::unitOf(port))); };
    for (int o = 0; o < numOutputs; ++o) {
        g.setColour(unitFill(outputNames[static_cast<std::size_t>(o)]));
        g.fillRect(lw + o * cw, 0, cw, hh);
    }
    for (int i = 0; i < numInputs; ++i) {
        g.setColour(unitFill(inputNames[static_cast<std::size_t>(i)]));
        g.fillRect(0, hh + i * ch, lw, ch);
    }
    g.setColour(kFg);
    g.setFont(bold);
    for (int o = 0; o < numOutputs; ++o) {
        const auto n = outputNames[static_cast<std::size_t>(o)];
        const auto dot = n.find('.');
        auto area = juce::Rectangle<int>(lw + o * cw, 0, cw, hh).reduced(1, 2);
        g.drawText(name(n.substr(0, dot)), area.removeFromTop(area.getHeight() / 2), juce::Justification::centredBottom, true);
        g.drawText(name(n.substr(dot + 1)), area, juce::Justification::centredTop, true);
    }
    g.setFont(font);
    for (int i = 0; i < numInputs; ++i)
        g.drawText(name(inputNames[static_cast<std::size_t>(i)]) + (isGate(i) ? " (on/off)" : ""),
                   juce::Rectangle<int>(0, hh + i * ch, lw - 6, ch), juce::Justification::centredRight, true);

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
            } else if (!p.isPatched(in) && normalled[static_cast<std::size_t>(i)] == static_cast<Out>(o)) {
                g.drawEllipse(b.toFloat().withSizeKeepingCentre(9.0f, 9.0f), 1.5f);    // normalled: ring
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

    if (hasKeyboardFocus(false)) g.drawRect(cellBounds(cursor).expanded(1), 3);   // cell cursor
}

void MatrixView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    dragCell = cellAt(e.getPosition());
    dragged = false;
    if (dragCell) { cursor = *dragCell; dragStartLevel = level(*dragCell); repaint(); }
}

void MatrixView::mouseDrag(const juce::MouseEvent& e) {
    mouse = e.getPosition();
    if (!dragCell || isGate(dragCell->input)) return;
    if (!dragged && std::abs(e.getDistanceFromDragStartY()) < 3) return;
    dragged = true;
    // 1 % per 1.5 pixels; up = more.
    set(*dragCell, dragStartLevel - static_cast<float>(e.getDistanceFromDragStartY()) / 150.0f);
}

void MatrixView::mouseUp(const juce::MouseEvent& e) {
    if (dragCell && !dragged && e.getNumberOfClicks() == 1 && cellAt(e.getPosition()) == std::optional<Cell>(*dragCell))
        set(*dragCell, level(*dragCell) > 0.0f ? 0.0f : 1.0f);
    dragCell.reset();
}

void MatrixView::mouseDoubleClick(const juce::MouseEvent& e) {
    if (const auto c = cellAt(e.getPosition()); c && !isGate(c->input)) typeLevel(*c);
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
}

bool MatrixView::keyPressed(const juce::KeyPress& key) {
    const int code = key.getKeyCode();
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    auto move = [this](int di, int dout) {
        cursor.input = juce::jlimit(0, lmw::dsp::numInputs - 1, cursor.input + di);
        cursor.output = juce::jlimit(0, lmw::dsp::numOutputs - 1, cursor.output + dout);
        repaint();
        return true;
    };
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
    const auto c = cellAt(getMouseXYRelative());
    if (!c) return {};
    const auto in = static_cast<lmw::dsp::In>(c->input);
    const auto out = static_cast<lmw::dsp::Out>(c->output);
    const auto& p = patch();
    const juce::String link = name(lmw::dsp::outputNames[static_cast<std::size_t>(c->output)]) + " "
                            + juce::String::fromUTF8("\xe2\x86\x92") + " " + name(lmw::dsp::inputNames[static_cast<std::size_t>(c->input)]);
    const float l = p.level(in, out);
    juce::String state;
    if (l > 0.0f) state = "cable at " + percent(l);
    else if (!p.isPatched(in) && lmw::dsp::normalled[static_cast<std::size_t>(c->input)] == out)
        state = "normalled (used while this input has no cables)";
    else state = "no cable";
    juce::String how = isGate(c->input) ? "Click: on/off." : "Click: on/off. Drag up/down: level. Double-click: type the level.";
    if (l <= 0.0f && p.pinCount(in) >= lmw::dsp::Patch::maxPins) how = "This input already has 4 cables.";
    return link + ": " + state + "\n" + how;
}
