// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "ui/Contrast.h"

namespace {
juce::Colour toJuce(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
}

LMWSynthEditor::LMWSynthEditor(LMWSynthProcessor& p) : AudioProcessorEditor(&p) {
    setSize(640, 400);
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    // Spec S6.2: white background, black text; colours pass through the
    // contrast enforcer so later themes/presets (S6.3) stay compliant.
    const lmw::ui::Rgb bg{255, 255, 255};
    const auto fg = lmw::ui::enforceContrast({0, 0, 0}, bg, lmw::ui::minimumRatio(lmw::ui::ContrastPreset::AA));
    g.fillAll(toJuce(bg));
    g.setColour(toJuce(fg));
    g.setFont(juce::FontOptions(22.0f));
    g.drawFittedText("LMW Synth  -  Phase 0 skeleton", getLocalBounds(), juce::Justification::centred, 1);
}
