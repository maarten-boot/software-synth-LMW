// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.7: the Schematic tab — the instrument drawn as a stylized schematic, updated live from the patch
// (Phase 12a: view only). Geometry and connections come from ui/Schematic.h. Symbols are black outlines
// filled with their unit colour (S6.9); connections are black: dashed = normalled (behind the blocks),
// solid with a white edge = cable (on top, with its level when below 100 %). Hovering a port shows its connections.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Patch.h"
#include "ui/Schematic.h"
#include "ui/Theme.h"
#include <functional>

class SchematicView final : public juce::Component, public juce::TooltipClient {
public:
    explicit SchematicView(std::function<const lmw::dsp::Patch&()> patch);

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
    lmw::ui::theme::Palette palette = lmw::ui::theme::palette(4.5);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SchematicView)
};
