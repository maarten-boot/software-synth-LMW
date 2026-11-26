// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.7: the Schematic tab — the instrument drawn as a stylized schematic, updated live from the patch.
// Geometry and connections come from ui/Schematic.h. Symbols are black outlines filled with their unit
// colour (S6.9); connections are black: dashed = normalled (behind the blocks), solid with a white edge =
// cable (on top, with its level when below 100 %); thin dotted with an arrow into the module = internal
// link set by a fader (key tracking while Key > 0, velocity -> VCA). Hovering a port shows its connections,
// hovering a block also its internal links.
// Phase 12b: editable like the Synth tab (same rules, ui/CableEdit.h): drag from a port to a port of the
// other kind to add a cable; drag a plugged input's newest cable to another input to move it, anywhere else
// to remove it; Escape cancels. Keyboard: arrows move a port cursor; Enter, Shift+F10 or right-click opens
// the cable menu; Delete removes all cables of an input. Other keys (QWERTY notes) pass on.
// Phase 12c: drag a symbol by its body to move it (snapped to the grid; an outline shows where it lands,
// dashed where it does not fit, and the drop is then refused); Shift+arrows move the block that holds the
// port cursor. The layout is kept in the saved state and patch files (Layout::serialize()).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Patch.h"
#include "ui/CableEdit.h"
#include "ui/Schematic.h"
#include "ui/Theme.h"
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class SchematicView final : public juce::Component, public juce::TooltipClient {
public:
    // keyTrack(): the Key faders of VCF 1 and VCF 2 (0..1), for the internal key-tracking links, then the LFO 1 and
    // LFO 2 Delay faders (s, Spec S5.24: a label under the LFO symbol and a line in the hover texts while above 0).
    // connect(input, output, level): as the editor's connect (level 0 removes, output Patch::none removes
    // all cables of the input); false when it does not fit (the editor tells the user).
    // showMenu(output, port, screenArea): the editor's cable menu for that port.
    SchematicView(std::function<const lmw::dsp::Patch&()> patch, std::function<std::array<float, 4>()> keyTrack,
                  std::function<bool(int, int, float)> connect, std::function<void(bool, int, juce::Rectangle<int>)> showMenu);

    // Called by the editor's timer: repaints when a fader shown in the schematic changed.
    void poll();
    // Also picks up a layout changed elsewhere (patch file loaded, state restored).
    void poll(const juce::String& layoutText);

    // Phase 12c: the block positions as text (empty = default); onLayoutChange is called after a move or reset.
    void setLayout(const juce::String& text);
    void resetLayout();
    bool isLayoutDefault() const { return layout.isDefault(); }
    std::function<void(const juce::String&)> onLayoutChange;

    void setPalette(const lmw::ui::theme::Palette& p) { palette = p; repaint(); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    juce::String getTooltip() override;

private:
    using PortRef = lmw::ui::cables::PortRef;
    float scale() const noexcept;
public:
    // Spec S6.22 (Phase 39): drawn at the Synth tab's text size — names and port labels 15 px (compact 13 px) at
    // zoom 100 % — instead of filling the tab; zoom steps 75 / 100 / 125 / 150 % (Ctrl + wheel, Ctrl + / − / 0).
    // The editor shows it in a view: centred when smaller, scrolling when larger (drawingSize()).
    void setCompact(bool compact) { textPx = compact ? 13.0f : 15.0f; if (onSizeChange) onSizeChange(); repaint(); }
    static constexpr std::array<float, 4> zoomSteps{0.75f, 1.0f, 1.25f, 1.5f};
    void setZoom(int step) { zoomStep = std::clamp(step, 0, static_cast<int>(zoomSteps.size()) - 1); if (onSizeChange) onSizeChange(); repaint(); }
    int zoomIndex() const noexcept { return zoomStep; }
    float zoomFactor() const noexcept { return zoomSteps[static_cast<std::size_t>(zoomStep)]; }
    juce::Point<int> drawingSize() const noexcept;        // the drawing with a margin, at the current scale
    std::function<void()> onSizeChange;                    // the editor lays out the view again
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setRings(bool on) { rings = on; repaint(); }                             // Spec S6.20
    bool rings = true;
    // Spec S7.12: every block's name and its area in this component's pixels (for 9P /view).
    std::vector<std::pair<std::string, juce::Rectangle<int>>> blockAreas() const;
private:
    float textPx = 15.0f;                              // S6.22: label size at 100 %
    int zoomStep = 1;                                  // index into zoomSteps (100 %)
    juce::Point<float> toScreen(lmw::ui::schematic::Point p) const noexcept;
    lmw::ui::schematic::Point toCanvas(juce::Point<float> p) const noexcept;
    juce::Rectangle<float> toScreen(const lmw::ui::schematic::Rect& r) const noexcept;
    void drawBlock(juce::Graphics&, std::size_t block) const;
    void layoutChanged();
    lmw::ui::schematic::Point portPoint(PortRef p) const;
    std::optional<PortRef> portUnder(juce::Point<float> where, std::optional<bool> wantInput = std::nullopt) const;
    void openMenu(PortRef p);
    void cancelDrag();

    std::function<const lmw::dsp::Patch&()> patch;
    std::function<std::array<float, 4>()> keyTrack;
    std::function<bool(int, int, float)> connect;
    std::function<void(bool, int, juce::Rectangle<int>)> showMenu;
    std::array<float, 4> shownKeys{};                 // VCF 1 / 2 Key, LFO 1 / 2 Delay
    lmw::ui::schematic::LfoDelays delays() const noexcept { return {shownKeys[2], shownKeys[3]}; }
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);

    // Editing (Phase 12b).
    std::optional<PortRef> pressed;                  // port the mouse went down on
    std::optional<lmw::ui::cables::Grab> grab;       // cable being dragged
    std::optional<PortRef> target;                   // compatible port under the mouse
    std::optional<PortRef> hover;                    // port under the mouse: its cables black (user 2026-11-22)
    juce::Point<float> loose;                        // loose end, component coordinates
    std::size_t cursor = 0;                          // keyboard port cursor (index into portOrder())

    // Moving blocks (Phase 12c).
    lmw::ui::schematic::Layout layout;
    juce::String shownLayout;                        // the text `layout` came from / was saved as
    struct BlockMove { std::size_t block; lmw::ui::schematic::Point from, to; };
    std::optional<BlockMove> moving;                 // block being dragged
    bool moved = false;                              // dragged far enough to count as a move

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchematicView)
};
