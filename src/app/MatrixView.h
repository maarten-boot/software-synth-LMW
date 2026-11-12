// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.8: the Matrix tab — a VCS3-like pin matrix of the patch (S5.14). Rows are the inputs, columns
// the outputs; every crossing shows the pin level (empty = no cable, a black bar filled to the level with
// the % as text). The normalled source of an input without cables is marked with a ring.
//   Mouse: click toggles 0 / 100 %; drag up/down sets the level; double-click types it.
//   Keys:  arrows move the cell cursor; Space or Enter toggles; + / - change by 10 %, Page Up / Down by 1 %;
//          Delete removes the cable; Home / End = 100 % / remove. Other keys (QWERTY notes) pass on.
// Gate inputs (env1.gate, env2.gate) are on/off only. Black and white (Spec S6.2), except the row and column
// labels, which have their unit's colour (S6.9).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Patch.h"
#include "ui/Theme.h"
#include <functional>
#include <optional>

class MatrixView final : public juce::Component, public juce::TooltipClient {
public:
    // setPin(input, output, level): level 0 removes; returns false when it does not fit (the caller
    // tells the user). patch(): the current patch mirror (message thread).
    MatrixView(std::function<const lmw::dsp::Patch&()> patch, std::function<bool(int, int, float)> setPin);

    void setFontSize(float size) { fontSize = size; repaint(); }
    void setPalette(const lmw::ui::theme::Palette& p) { palette = p; repaint(); }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    juce::String getTooltip() override;

    struct Cell { int input = 0, output = 0; bool operator==(const Cell&) const = default; };
    static bool isGate(int input) noexcept;

private:
    juce::Rectangle<int> cellBounds(Cell c) const;
    std::optional<Cell> cellAt(juce::Point<int> p) const;
    float level(Cell c) const;
    void set(Cell c, float level);
    void typeLevel(Cell c);
    int labelWidth() const { return juce::roundToInt(fontSize * 7.5f); }
    int headerHeight() const { return juce::roundToInt(fontSize * 2.8f); }
    int cellWidth() const;
    int cellHeight() const;

    std::function<const lmw::dsp::Patch&()> patch;
    std::function<bool(int, int, float)> setPin;
    float fontSize = 13.0f;
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);
    Cell cursor;                         // keyboard cell cursor
    std::optional<Cell> dragCell;        // cell being dragged
    float dragStartLevel = 0.0f;
    bool dragged = false;
    juce::Point<int> mouse;              // last mouse position, for the tooltip

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixView)
};
