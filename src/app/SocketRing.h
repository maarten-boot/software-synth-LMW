// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.19 (Phase 32): the socket type ring — around a socket on the Synth and Effects tabs and around a port
// on the Schematic. Never colour alone: audio a solid ring, CV a double ring, gate a dashed ring (user 2026-11-25:
// the dashes were too dominant on the many CV sockets, so they went to the few gate sockets).
#pragma once
#include "dsp/Patch.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lmw::ui {

// A ring of radius r (to the middle of its line) and line width w around c.
inline void drawTypeRing(juce::Graphics& g, juce::Point<float> c, float r, float w, lmw::dsp::SignalType t, juce::Colour colour) {
    g.setColour(colour);
    switch (t) {
        case lmw::dsp::SignalType::Audio:
            g.drawEllipse(c.x - r, c.y - r, 2 * r, 2 * r, w);
            break;
        case lmw::dsp::SignalType::Gate: {                   // about eight dashes around
            juce::Path circle, dashed;
            circle.addEllipse(c.x - r, c.y - r, 2 * r, 2 * r);
            const float len = 6.2832f * r / 8.0f;
            const float dash[] = {len * 0.58f, len * 0.42f};
            juce::PathStrokeType(w).createDashedStroke(dashed, circle, dash, 2);
            g.fillPath(dashed);
            break;
        }
        case lmw::dsp::SignalType::Cv: {                     // two thin rings in the same band
            const float thin = std::max(1.0f, w * 0.5f);
            const float ro = r + w * 0.5f - thin * 0.5f, ri = r - w * 0.5f + thin * 0.5f - 1.0f;
            g.drawEllipse(c.x - ro, c.y - ro, 2 * ro, 2 * ro, thin);
            g.drawEllipse(c.x - ri, c.y - ri, 2 * ri, 2 * ri, thin);
            break;
        }
    }
}

// The first hover line of a socket (S6.19): "Audio output", "CV input — also takes audio (audio-rate FM / PM)",
// "Gate input (on / off)".
inline juce::String typeLine(bool input, int index) {
    using lmw::dsp::SignalType;
    const auto t = input ? lmw::dsp::typeOf(static_cast<lmw::dsp::In>(index)) : lmw::dsp::typeOf(static_cast<lmw::dsp::Out>(index));
    juce::String s = t == SignalType::Audio ? "Audio" : t == SignalType::Cv ? "CV" : "Gate";
    s << (input ? " input" : " output");
    if (t == SignalType::Gate) s << " (on / off)";
    if (input && lmw::dsp::alsoTakesAudio(static_cast<lmw::dsp::In>(index)))
        s << juce::String::fromUTF8(" \xe2\x80\x94 also takes audio (audio-rate FM / PM)");
    return s;
}

} // namespace lmw::ui
