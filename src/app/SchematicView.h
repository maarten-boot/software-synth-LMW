// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.7: the Schematic tab — the instrument drawn as a stylized schematic, updated live from the patch
// (Phase 12a: view only). Geometry and connections come from ui/Schematic.h. Symbols are black outlines
// filled with their unit colour (S6.9); connections are black: dashed = normalled (behind the blocks),
// solid with a white edge = cable (on top, with its level when below 100 %); thin dotted with an arrow into
// the module = internal link set by a fader (key tracking while Key > 0, velocity -> VCA). Hovering a port
// shows its connections, hovering a block also its internal links.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Patch.h"
#include "ui/Schematic.h"
#include "ui/Theme.h"
#include <array>
#include <functional>

class SchematicView final : public juce::Component, public juce::TooltipClient {
public:
    // keyTrack(): the Key faders of VCF 1 and VCF 2 (0..1), for the internal key-tracking links.
    SchematicView(std::function<const lmw::dsp::Patch&()> patch, std::function<std::array<float, 2>()> keyTrack);

    // Called by the editor's timer: repaints when a fader shown in the schematic changed.
    void poll();

    void setPalette(const lmw::ui::theme::Palette& p) { palette = p; repaint(); }
    void paint(juce::Graphics&) override;
    juce::String getTooltip() override;

private:
    float scale() const noexcept;
    juce::Point<float> toScreen(lmw::ui::schematic::Point p) const noexcept;
    lmw::ui::schematic::Point toCanvas(juce::Point<float> p) const noexcept;
    juce::Rectangle<float> toScreen(const lmw::ui::schematic::Rect& r) const noexcept;
    void drawBlock(juce::Graphics&, const lmw::ui::schematic::Block&) const;

    std::function<const lmw::dsp::Patch&()> patch;
    std::function<std::array<float, 2>()> keyTrack;
    std::array<float, 2> shownKeys{};
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchematicView)
};
