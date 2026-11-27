// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.8: the Matrix tab — a VCS3-like pin matrix of the patch (S5.14). Rows are the inputs, columns
// the outputs; every crossing shows the pin level (empty = no cable, a black bar filled to the level with
// the % as text). The normalled source of an input without cables is marked with a ring.
//   Mouse (user 2026-11-21, Phase 17f): a click selects a cell — the level slider shows its level and a
//          cable keeps it; a click on an empty cell makes a 100 % cable. Drag up/down sets the level;
//          double-click removes a cable (not the one its own first click just made); right-click opens a
//          menu: 100 %, Type level..., Remove cable (only what is possible).
//   Keys:  arrows move the cell cursor; Space or Enter toggles; + / - change by 10 %, Page Up / Down by 1 %;
//          Delete removes the cable; Home / End = 100 % / remove; Shift+F10 opens the cell menu. Other keys
//          (QWERTY notes) pass on.
// Gate inputs (env1.gate, env2.gate) are on/off only. Black and white (Spec S6.2), except the row and column
// labels, which have their unit's colour (S6.9).
// Finding a crossing (user 2026-11-21): the row and column under the mouse get a grey band and their labels
// invert (white on black); clicking a row or column label pins that band until clicked again; the selected
// cell's labels have a black frame.
// A level slider on the right (0-100 %, user 2026-11-18; -100..+100 % on modulation inputs, S5.21) shows and sets the level of the cell under the
// cursor; it is active for every cell that takes a level (not on gate inputs, nor on an empty cell of a full input).
// Spec S6.25: a view shows a list of inputs (rows) and outputs (columns): the Matrix tab the synth's, the Effects tab
// the effects' (in the effects order); setPorts changes them.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Patch.h"
#include "ui/Theme.h"
#include <functional>
#include <optional>
#include <vector>

class MatrixView final : public juce::Component, public juce::TooltipClient {
public:
    // setPin(input, output, level): lmw::dsp::removePin removes; returns false when it does not fit (the caller
    // tells the user). patch(): the current patch mirror (message thread).
    MatrixView(std::function<const lmw::dsp::Patch&()> patch, std::function<bool(int, int, float)> setPin);
    std::function<void(bool started)> onLevelDrag;   // Spec S6.14: a level drag is one undo step
    // Spec S6.25: the rows (input ports) and columns (output ports) shown, top to bottom and left to right.
    void setPorts(std::vector<int> inputs, std::vector<int> outputs);
    static std::vector<int> synthInputs();   // the synth's inputs, as In (not the effects')
    static std::vector<int> synthOutputs();
    static std::vector<int> effectInputs(int effectOrder);    // the effects' inputs in chain order, then main.in
    static std::vector<int> effectOutputs(int effectOrder);   // out.out, then the effects' outputs in chain order
    const std::vector<int>& rowPorts() const noexcept { return rows; }
    const std::vector<int>& columnPorts() const noexcept { return columns; }
    int crossedCells() const;                // cells whose input and output cannot connect (S7.6): 0 with these lists
    // The size the grid wants at its font: labels, `cell` wide cells, the slider (the Effects tab sizes it so).
    juce::Point<int> preferredSize(float cellWidthInFonts, float cellHeightInFonts) const;

    void setFontSize(float size) { fontSize = size; resized(); syncSlider(); repaint(); }
    void setPalette(const lmw::ui::theme::Palette& p) { palette = p; repaint(); }

    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();                      // the patch changed elsewhere: update the level slider
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    void focusOfChildComponentChanged(FocusChangeType) override { repaint(); }   // the slider: keep the cursor shown
    juce::String getTooltip() override;

    struct Cell { int input = 0, output = 0; bool operator==(const Cell&) const = default; };
    static bool isGate(int input) noexcept;

private:
    juce::Rectangle<int> cellBounds(Cell c) const;
    std::optional<Cell> cellAt(juce::Point<int> p) const;
    int rowAt(juce::Point<int> p) const;     // the input row under p (cell or row label), else -1
    int columnAt(juce::Point<int> p) const;  // the output column under p (cell or column label), else -1
    float level(Cell c) const;
    void set(Cell c, float level);
    bool has(Cell c) const;              // a cable (S5.21: its level may be 0)
    void remove(Cell c);
    void typeLevel(Cell c);
    void showCellMenu(Cell c);
    int labelWidth() const { return juce::roundToInt(fontSize * 7.5f); }
    int headerHeight() const { return juce::roundToInt(fontSize * 2.8f); }
    int sliderWidth() const { return juce::roundToInt(fontSize * 8.0f); }
    // The label zones (user 2026-11-24: on all four sides): input names left and right, output names top and bottom.
    bool inRowLabel(juce::Point<int> p) const {
        const int right = labelWidth() + numColumns() * cellWidth();
        return p.x < labelWidth() || (p.x >= right && p.x < right + labelWidth());
    }
    bool inColumnLabel(juce::Point<int> p) const {
        const int bottom = headerHeight() + numRows() * cellHeight();
        return p.y < headerHeight() || (p.y >= bottom && p.y < bottom + headerHeight());
    }
    void syncSlider();
    int numRows() const noexcept { return static_cast<int>(rows.size()); }
    int numColumns() const noexcept { return static_cast<int>(columns.size()); }
    int rowOf(int input) const;              // display row of an input port, -1 if not shown
    int columnOf(int output) const;
    int cellWidth() const;
    int cellHeight() const;

    std::function<const lmw::dsp::Patch&()> patch;
    std::function<bool(int, int, float)> setPin;
    std::vector<int> rows = synthInputs(), columns = synthOutputs();
    float fontSize = 13.0f;
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);
    Cell cursor;                         // keyboard cell cursor (ports, not display positions)
    std::optional<Cell> dragCell;        // cell being dragged
    float dragStartLevel = 0.0f;
    bool dragged = false;
    int hoverRow = -1, hoverColumn = -1;  // under the mouse (display positions)
    int pinnedRow = -1, pinnedColumn = -1;  // kept highlighted by a click on its label
    std::optional<Cell> justMade;        // the cell a single click just made a cable in, and when
    juce::uint32 justMadeAt = 0;
    juce::Point<int> mouse;              // last mouse position, for the tooltip
    juce::Label levelLabel;              // the cursor cell ("lfo1.out -> vcf1.cutoff") and why the slider is off
    juce::Slider levelSlider{juce::Slider::LinearVertical, juce::Slider::TextBoxBelow};
    bool syncing = false;                // setting the slider from the patch, not from the user

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixView)
};
