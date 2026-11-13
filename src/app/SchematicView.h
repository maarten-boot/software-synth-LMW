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

class SchematicView final : public juce::Component, public juce::TooltipClient {
public:
    // keyTrack(): the Key faders of VCF 1 and VCF 2 (0..1), for the internal key-tracking links.
    // connect(input, output, level): as the editor's connect (level 0 removes, output Patch::none removes
    // all cables of the input); false when it does not fit (the editor tells the user).
    // showMenu(output, port, screenArea): the editor's cable menu for that port.
    SchematicView(std::function<const lmw::dsp::Patch&()> patch, std::function<std::array<float, 2>()> keyTrack,
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
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }
    juce::String getTooltip() override;

private:
    using PortRef = lmw::ui::cables::PortRef;
    float scale() const noexcept;
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
    std::function<std::array<float, 2>()> keyTrack;
    std::function<bool(int, int, float)> connect;
    std::function<void(bool, int, juce::Rectangle<int>)> showMenu;
    std::array<float, 2> shownKeys{};
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);

    // Editing (Phase 12b).
    std::optional<PortRef> pressed;                  // port the mouse went down on
    std::optional<lmw::ui::cables::Grab> grab;       // cable being dragged
    std::optional<PortRef> target;                   // compatible port under the mouse
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
