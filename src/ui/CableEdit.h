// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.6, S6.7: what dragging a cable does — shared by the Synth tab and the Schematic tab so both
// behave the same. Pure C++ (no JUCE), tested in tests/test_schematic.cpp.
//   Drag from an output, or from an empty input: a new cable from there; dropped on a socket of the
//   other kind it is added (an input takes up to 4).
//   Drag from a plugged input: picks up its newest cable (the output end stays); dropped on another
//   input it moves there with its level, dropped anywhere else it is removed.
#pragma once
#include "dsp/Patch.h"
#include <optional>
#include <vector>

namespace lmw::ui::cables {

struct PortRef {
    bool output = false;
    int index = 0;                   // lmw::dsp::Out or In
    bool operator==(const PortRef&) const = default;
};

struct Grab {
    PortRef fixed;                   // the end that stays where it is
    int detachedInput = -1;          // moving a cable that was plugged into this input...
    int detachedSource = -1;         // ...from this output
    float detachedLevel = 1.0f;
    bool moving() const noexcept { return detachedInput >= 0; }
    // A socket the loose end may be dropped on: the other kind than the fixed end.
    bool accepts(PortRef p) const noexcept { return p.output != fixed.output; }
};

// Starting a drag at `from`.
inline Grab grab(const lmw::dsp::Patch& p, PortRef from) noexcept {
    Grab g;
    const auto in = static_cast<lmw::dsp::In>(from.index);
    if (!from.output && p.isPatched(in)) {
        const auto& pin = p.pin(in, p.pinCount(in) - 1);   // the newest cable
        g.detachedInput = from.index;
        g.detachedSource = pin.source;
        g.detachedLevel = pin.level;
        g.fixed = {true, pin.source};
    } else {
        g.fixed = from;
    }
    return g;
}

// One cable change: connect(input, output, level), level 0 removes that cable.
struct Op {
    int input, output;
    float level;
    bool operator==(const Op&) const = default;
};

// The changes a drop makes, to be applied in order, stopping at the first that fails (e.g. a full input:
// then a moved cable stays where it was).
inline std::vector<Op> drop(const Grab& g, std::optional<PortRef> target) {
    if (target && !g.accepts(*target)) target.reset();
    if (g.moving()) {
        if (!target) return {{g.detachedInput, g.detachedSource, 0.0f}};                    // removed
        if (target->index == g.detachedInput) return {};                                     // put back
        return {{target->index, g.detachedSource, g.detachedLevel}, {g.detachedInput, g.detachedSource, 0.0f}};
    }
    if (!target) return {};
    return g.fixed.output ? std::vector<Op>{{target->index, g.fixed.index, 1.0f}}
                          : std::vector<Op>{{g.fixed.index, target->index, 1.0f}};
}

} // namespace lmw::ui::cables
