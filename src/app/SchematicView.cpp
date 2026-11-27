// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SchematicView.h"
#include "SocketRing.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace sc = lmw::ui::schematic;

namespace {
juce::Colour colour(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
const juce::Colour kBg = colour(lmw::ui::theme::background);
const juce::Colour kFg = colour(lmw::ui::theme::foreground);
juce::String text(std::string_view s) { return juce::String::fromUTF8(s.data(), static_cast<int>(s.size())); }
}

SchematicView::SchematicView(std::function<const lmw::dsp::Patch&()> p, std::function<std::array<float, 4>()> k,
                             std::function<bool(int, int, float)> c, std::function<void(bool, int, juce::Rectangle<int>)> m)
    : patch(std::move(p)), keyTrack(std::move(k)), connect(std::move(c)), showMenu(std::move(m)) {
    setWantsKeyboardFocus(true);
    setTitle("Schematic");
    setDescription("The instrument as a schematic: dashed lines are normalled connections, solid lines cables, "
                   "dotted arrows internal links set by a fader; a dashed outline marks a unit without effect on the sound "
                   "(long dashes: it passes the sound unchanged) and sparse dots its lines. Drag between ports to patch a cable; Left/Right "
                   "move a port cursor, Up/Down jump to the next or previous symbol, Enter opens its cable menu, "
                   "Delete removes an input's cables.");
}

void SchematicView::poll() {
    if (const auto k = keyTrack(); k != shownKeys) {
        shownKeys = k;
        if (isVisible()) repaint();
    }
}

// Spec S6.22: the scale gives names and port labels (1.7 units) the Synth tab's text size, times the zoom step.
// (Before: it filled the tab, about 19 px at 1920 x 1080; Compact 75 % of that.)
float SchematicView::scale() const noexcept {
    return textPx / 1.7f * zoomFactor();
}

// Spec S6.22 (39b, user 2026-11-26: "allow elements to use the complete page"): the drawing starts at the top-left
// (12 px margin) on a page as large as the view; it is at least the canvas, and reaches 20 units beyond the furthest
// block (8 below it, for the legend), so a block moved to the edge always has room to go on (the view then scrolls).
juce::Point<int> SchematicView::drawingSize() const noexcept {
    const float s = scale();
    const auto e = layout.extent();
    const float w = std::max(sc::canvasWidth, e.x + 20.0f), h = std::max(sc::canvasHeight, e.y + 8.0f);
    return {juce::roundToInt(w * s) + 24, juce::roundToInt(h * s) + 24};
}

void SchematicView::updatePage() {
    const float s = scale();
    layout.setPage((static_cast<float>(getWidth()) - 24.0f) / s, (static_cast<float>(getHeight()) - 24.0f) / s);
}

// Spec S6.22: Ctrl (Cmd) + wheel zooms in steps; the wheel alone goes to the view around it (it scrolls).
void SchematicView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
        if (w.deltaY > 0.0f) setZoom(zoomStep + 1);
        else if (w.deltaY < 0.0f) setZoom(zoomStep - 1);
        return;
    }
    juce::Component::mouseWheelMove(e, w);
}

juce::Point<float> SchematicView::toScreen(sc::Point p) const noexcept {   // S6.22 (39b): from the top-left
    const float s = scale();
    return {12.0f + p.x * s, 12.0f + p.y * s};
}

sc::Point SchematicView::toCanvas(juce::Point<float> p) const noexcept {
    const float s = scale();
    return {(p.x - 12.0f) / s, (p.y - 12.0f) / s};
}

juce::Rectangle<float> SchematicView::toScreen(const sc::Rect& r) const noexcept {
    const auto a = toScreen(sc::Point{r.x, r.y});
    return {a.x, a.y, r.w * scale(), r.h * scale()};
}

lmw::dsp::Use SchematicView::useOf(std::size_t index) const noexcept {
    const auto& b = sc::blocks()[index];
    if (!use || b.outputs.empty()) return lmw::dsp::Use::Used;          // MAIN
    return use->unit(lmw::dsp::portUnit(b.outputs.front()));
}

void SchematicView::drawBlock(juce::Graphics& g, std::size_t index) const {
    const auto& b = sc::blocks()[index];
    const float s = scale();
    const auto r = toScreen(layout.box(index));
    const auto fill = colour(palette.unitColour(b.unit()));
    const float stroke = std::max(1.0f, 0.18f * s);
    // Spec S6.26: the outline solid, short dashes without effect, long dashes while it passes its input unchanged.
    const auto blockUse = useOf(index);
    auto outline = [&](const juce::Path& shape, float width) {
        if (blockUse == lmw::dsp::Use::Used) { g.strokePath(shape, juce::PathStrokeType(width)); return; }
        const float shortDash[] = {0.7f * s, 0.6f * s}, longDash[] = {2.0f * s, 0.7f * s};
        juce::Path dashed;
        juce::PathStrokeType(std::max(2.0f, width * 1.4f)).createDashedStroke(dashed, shape,
            blockUse == lmw::dsp::Use::Unused ? shortDash : longDash, 2);
        g.fillPath(dashed);
    };
    auto outlineRect = [&](juce::Rectangle<float> box, float width) { juce::Path p; p.addRectangle(box); outline(p, width); };
    const juce::FontOptions label(1.7f * s);

    auto innerLabels = [&](float labelLeft) {   // output labels inside the right edge, for several outputs
        if (b.outputs.size() < 2) return;
        g.setFont(label);
        for (std::size_t k = 0; k < b.outputs.size(); ++k) {
            const auto p = toScreen(sc::outputPoint(b.outputs[k], layout));
            g.drawText(text(sc::portLabel(lmw::dsp::outputNames[static_cast<std::size_t>(b.outputs[k])])),
                       juce::Rectangle<float>(labelLeft, p.y - s, p.x - labelLeft - 0.6f * s, 2 * s), juce::Justification::centredRight, false);
        }
    };

    // The name inside the symbol (user 2026-11-13): bold, black on the unit colour, shrunk to fit `area`
    // (from 1.7 down to 1.2 canvas units).
    auto drawName = [&](juce::Rectangle<float> area, juce::Justification just) {
        const auto name = text(b.name);
        float size = 1.7f * s;
        juce::Font font(juce::FontOptions(size, juce::Font::bold));
        while (size > 1.2f * s && juce::GlyphArrangement::getStringWidth(font, name) > area.getWidth()) {
            size -= 0.05f * s;
            font = juce::Font(juce::FontOptions(size, juce::Font::bold));
        }
        g.setColour(kFg);
        g.setFont(font);
        g.drawText(name, area, just, false);
    };

    switch (b.shape) {
    case sc::Shape::Triangle: {
        juce::Path t;
        t.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
        g.setColour(fill);
        g.fillPath(t);
        g.setColour(kFg);
        outline(t, stroke);
        if (b.inputs.size() > 1) {                    // input labels inside the left edge
            g.setFont(label);
            for (auto in : b.inputs) {
                const auto p = toScreen(sc::inputPoint(in, layout));
                g.drawText(text(sc::portLabel(lmw::dsp::inputNames[static_cast<std::size_t>(in)])),
                           juce::Rectangle<float>(p.x + 0.7f * s, p.y - s, r.getWidth() * 0.6f, 2 * s), juce::Justification::centredLeft, false);
            }
        }
        {   // Name: on a row free of port labels — the centre for 0, 1, 2 or 4 inputs, else between the two
            // middle ports (VCF, VCA: between "in" and "cutoff" / "cv") — left-aligned next to the ports,
            // as wide as the triangle allows there.
            const auto n = b.inputs.size();
            float y = r.getCentreY();
            if (n % 2 == 1 && n >= 3) {                 // one input has no label (LFO rate, S5.19): the centre
                const float lower = r.getY() + r.getHeight() * static_cast<float>(n / 2 + 1) / static_cast<float>(n + 1);
                const float upper = n >= 3 ? r.getY() + r.getHeight() * static_cast<float>(n / 2) / static_cast<float>(n + 1) : r.getY();
                y = (upper + lower) / 2;
            }
            const float halfH = r.getHeight() / 2, textH = 1.8f * s;
            const float f = std::clamp(1.0f - (std::abs(y - r.getCentreY()) + textH / 2) / halfH, 0.0f, 1.0f);   // usable fraction
            const float left = r.getX() + (n == 1 ? 1.8f : 0.8f) * s;   // one input: its socket is on this row
            drawName(juce::Rectangle<float>(left, y - textH / 2, std::max(0.0f, r.getX() + f * r.getWidth() - left), textH),
                     juce::Justification::centredLeft);
        }
        break;
    }
    case sc::Shape::Adsr: {
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(kFg);
        outlineRect(r, stroke);
        drawName(juce::Rectangle<float>(r.getX() + 0.8f * s, r.getY() + 0.2f * s, r.getWidth() - 1.6f * s, 2.0f * s),
                 juce::Justification::centredLeft);   // name in a strip at the top
        const auto at = [&](float fx, float fy) { return juce::Point<float>(r.getX() + fx * r.getWidth(), r.getY() + fy * r.getHeight()); };
        juce::Path curve;                              // attack, decay, sustain, release, below the name
        curve.startNewSubPath(at(0.12f, 0.88f));
        curve.lineTo(at(0.3f, 0.36f));
        curve.lineTo(at(0.45f, 0.6f));
        curve.lineTo(at(0.7f, 0.6f));
        curve.lineTo(at(0.88f, 0.88f));
        g.strokePath(curve, juce::PathStrokeType(stroke * 1.3f, juce::PathStrokeType::mitered));
        break;
    }
    case sc::Shape::Keys: {
        g.setColour(kBg);
        g.fillRect(r);
        g.setColour(kFg);
        outlineRect(r, stroke);
        // Name at the top; four white keys and one black key (between the first two) below it, in the
        // left part of the block.
        drawName(juce::Rectangle<float>(r.getX() + 0.8f * s, r.getY() + 0.2f * s, r.getWidth() * 0.55f - 0.8f * s, 2.0f * s),
                 juce::Justification::centredLeft);
        const auto kb = r.withWidth(r.getWidth() * 0.55f).reduced(0.8f * s).withTrimmedTop(1.6f * s);
        const float kw = kb.getWidth() / 4;
        for (int k = 0; k < 4; ++k) g.drawRect(juce::Rectangle<float>(kb.getX() + k * kw, kb.getY(), kw, kb.getHeight()), stroke * 0.8f);
        g.fillRect(juce::Rectangle<float>(kb.getX() + kw * 0.65f, kb.getY(), kw * 0.7f, kb.getHeight() * 0.6f));
        innerLabels(kb.getRight());
        break;
    }
    case sc::Shape::Noise: {
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(kFg);
        outlineRect(r, stroke);
        // A white-noise trace: fixed pseudo-random samples (the same picture every time).
        drawName(juce::Rectangle<float>(r.getX() + 0.8f * s, r.getY() + 0.2f * s, r.getWidth() * 0.6f - 0.8f * s, 2.0f * s),
                 juce::Justification::centredLeft);   // name at the top, the trace below it
        const auto area = r.withWidth(r.getWidth() * 0.55f).reduced(0.6f * s, 1.0f * s).withTrimmedTop(1.4f * s);
        juce::Path trace;
        std::uint32_t x = 0x2545F491u;
        constexpr int n = 28;
        for (int i = 0; i <= n; ++i) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            const float v = static_cast<float>(x & 0xFFFF) / 65535.0f;
            const juce::Point<float> p(area.getX() + area.getWidth() * static_cast<float>(i) / n, area.getY() + area.getHeight() * v);
            if (i == 0) trace.startNewSubPath(p); else trace.lineTo(p);
        }
        g.strokePath(trace, juce::PathStrokeType(std::max(1.0f, stroke * 0.6f)));
        innerLabels(area.getRight());
        break;
    }
    case sc::Shape::Out: {
        // A circle with an arrow leaving it to the right (the usual line-out sign).
        const float d = std::min(r.getHeight(), r.getWidth() * 0.6f);
        const juce::Rectangle<float> circle(r.getX(), r.getCentreY() - d / 2, d, d);
        g.setColour(kBg);
        g.fillEllipse(circle);
        g.setColour(kFg);
        g.drawEllipse(circle, stroke * 1.3f);
        drawName(circle.reduced(d * 0.12f), juce::Justification::centred);   // "OUT" in the circle
        g.setColour(kFg);
        const juce::Line<float> arrow(circle.getRight(), r.getCentreY(), r.getRight(), r.getCentreY());
        g.drawArrow(arrow, stroke * 1.3f, d * 0.35f, d * 0.35f);
        break;
    }
    }
}

void SchematicView::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    const float s = scale();
    const auto& p = patch();
    const auto conns = sc::connections(p);

    // Normalled connections first, so the blocks cover where they pass behind them; cables on top of the
    // blocks, black with a white edge (as on the Synth tab), so a cable crossing a block is seen to cross it.
    const float lineW = std::max(1.0f, 0.2f * s);
    auto pathOf = [&](const sc::Connection& c) {
        const auto cv = sc::curve(sc::outputPoint(c.from, layout), sc::inputPoint(c.to, layout));
        juce::Path path;
        path.startNewSubPath(toScreen(cv[0]));
        path.cubicTo(toScreen(cv[1]), toScreen(cv[2]), toScreen(cv[3]));
        return path;
    };
    // Spec S6.26: a line from a unit without effect is dotted (normalled and cables).
    auto fromUnused = [&](const sc::Connection& c) {
        return use && use->unit(lmw::dsp::portUnit(c.from)) == lmw::dsp::Use::Unused;
    };
    const float sparse[] = {0.25f * s, 0.7f * s};
    g.setColour(kFg);
    for (const auto& c : conns) {
        if (!c.normalled) continue;
        juce::Path dashed;
        const float dash[] = {0.8f * s, 0.6f * s};
        juce::PathStrokeType(lineW).createDashedStroke(dashed, pathOf(c), fromUnused(c) ? sparse : dash, 2);
        g.fillPath(dashed);
    }
    // Internal links (fader-set): thin, dotted, behind the blocks; the arrow ends on the block's edge.
    shownKeys = keyTrack();
    const auto internal = sc::internalLinks(shownKeys[0], shownKeys[1]);
    const float dot[] = {0.25f * s, 0.55f * s};
    for (const auto& l : internal) {
        const auto bi = sc::findBlock(l.block);
        if (!bi) continue;
        const auto cv = sc::internalCurve(sc::outputPoint(l.from, layout), sc::internalTarget(*bi, layout));
        juce::Path path, dotted;
        path.startNewSubPath(toScreen(cv[0]));
        path.cubicTo(toScreen(cv[1]), toScreen(cv[2]), toScreen(cv[3]));
        juce::PathStrokeType(std::max(1.0f, 0.16f * s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dotted, path, dot, 2);
        g.fillPath(dotted);
        const auto tip = toScreen(cv[3]);
        juce::Path head;                                   // arrowhead pointing up into the block
        head.addTriangle(tip.x, tip.y, tip.x - 0.6f * s, tip.y + 1.3f * s, tip.x + 0.6f * s, tip.y + 1.3f * s);
        g.fillPath(head);
    }
    // Spec S6.23: the selected blocks on a box in the highlight colour with a black edge (at Max: white, 3 px edge).
    if (!selected.empty()) {
        const bool edgeOnly = palette.highlight == lmw::ui::Rgb{255, 255, 255};
        for (const auto b : selected) {
            const auto r = toScreen(layout.box(b)).expanded(0.6f * s);
            g.setColour(juce::Colour(palette.highlight.r, palette.highlight.g, palette.highlight.b));
            g.fillRect(r);
            g.setColour(kFg);
            g.drawRect(r, std::max(edgeOnly ? 3.0f : 2.0f, 0.2f * s));
        }
    }
    for (std::size_t i = 0; i < sc::blocks().size(); ++i) drawBlock(g, i);
    // Cables grey at rest; black, thicker and on top for the port under the mouse or the keyboard cursor
    // (user 2026-11-22, as on the Synth tab).
    std::optional<PortRef> cursorPort;
    if (hasKeyboardFocus(false)) {
        const auto order = sc::portOrder();
        if (cursor < order.size()) cursorPort = PortRef{!order[cursor].input, order[cursor].index};
    }
    auto hotEnd = [&](const sc::Connection& c) {
        for (const auto& h : {hover, cursorPort})
            if (h && ((h->output && h->index == static_cast<int>(c.from)) || (!h->output && h->index == static_cast<int>(c.to))))
                return true;
        return false;
    };
    const float alpha = palette.cableAlpha();                // Phase 21a: see-through at rest
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 0 && alpha < 1.0f) g.beginTransparencyLayer(alpha);
        for (const auto& c : conns) {
            if (c.normalled || hotEnd(c) != (pass == 1)) continue;
            if (grab && grab->moving() && static_cast<int>(c.to) == grab->detachedInput
                && static_cast<int>(c.from) == grab->detachedSource) continue;          // being moved
            const auto path = pathOf(c);
            const float w = lineW * (pass == 1 ? 2.4f : 1.8f);
            g.setColour(kBg);
            g.strokePath(path, juce::PathStrokeType(w + 2.0f * std::max(1.0f, 0.12f * s)));
            g.setColour(pass == 1 ? kFg : colour(palette.cableDrawn()));
            if (fromUnused(c)) {                               // S6.26: dotted, as thick as a cable
                juce::Path dotted;
                const float dots[] = {0.5f * s, 0.6f * s};
                juce::PathStrokeType(w).createDashedStroke(dotted, path, dots, 2);
                g.fillPath(dotted);
            } else {
                g.strokePath(path, juce::PathStrokeType(w));
            }
        }
        if (pass == 0 && alpha < 1.0f) g.endTransparencyLayer();
    }

    // Internal link labels just below the arrowheads, on white.
    g.setFont(juce::FontOptions(1.5f * s, juce::Font::italic));
    for (const auto& l : internal) {
        const auto bi = sc::findBlock(l.block);
        if (!bi) continue;
        const auto tip = toScreen(sc::internalTarget(*bi, layout));
        const auto t = juce::String::fromUTF8(l.label.c_str());
        const float w = juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(1.5f * s, juce::Font::italic)), t) + 0.6f * s;
        const juce::Rectangle<float> box(tip.x + 0.9f * s, tip.y + 0.2f * s, w, 1.9f * s);
        g.setColour(kBg);
        g.fillRect(box);
        g.setColour(kFg);
        g.drawText(t, box, juce::Justification::centred, false);
    }
    // Spec S5.24: an LFO whose Delay is above 0 gets "delay 400 ms" under its symbol, as the key labels.
    for (std::size_t k = 0; k < 2; ++k) {
        if (!(shownKeys[2 + k] > 0.0f)) continue;
        const auto bi = sc::findBlock(k == 0 ? "LFO 1" : "LFO 2");
        if (!bi) continue;
        const auto r = toScreen(layout.box(*bi));
        const auto t = juce::String::fromUTF8(sc::lfoDelayLabel(shownKeys[2 + k]).c_str());
        const float w = juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(1.5f * s, juce::Font::italic)), t) + 0.6f * s;
        const juce::Rectangle<float> box(r.getCentreX() - w / 2, r.getBottom() + 0.2f * s, w, 1.9f * s);
        g.setColour(kBg);
        g.fillRect(box);
        g.setColour(kFg);
        g.drawText(t, box, juce::Justification::centred, false);
    }

    // Ports: inputs white with a black ring, outputs black (as on the Synth tab, S6.6).
    const float pr = 0.55f * s;
    // Spec S6.15: the selected port (under the mouse, the keyboard cursor, the drop target) on a box in the
    // highlight colour with a black edge (at Max 21:1 the fill is white and the edge 3 px).
    {
        std::vector<PortRef> lit;
        for (const auto& h : {hover, cursorPort, target})
            if (h && std::find(lit.begin(), lit.end(), *h) == lit.end()) lit.push_back(*h);
        const bool edgeOnly = palette.highlight == lmw::ui::Rgb{255, 255, 255};
        for (const auto& h : lit) {
            const auto box = juce::Rectangle<float>(pr * 4.0f, pr * 4.0f).withCentre(toScreen(portPoint(h)));
            g.setColour(juce::Colour(palette.highlight.r, palette.highlight.g, palette.highlight.b));
            g.fillRect(box);
            g.setColour(kFg);
            g.drawRect(box, std::max(edgeOnly ? 3.0f : 2.0f, 0.25f * s));
        }
    }
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        const auto c = toScreen(sc::inputPoint(in, layout));
        g.setColour(kBg);
        g.fillEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr);
        g.setColour(kFg);
        g.drawEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr, std::max(1.0f, 0.15f * s));
        if (p.isPatched(in)) g.fillEllipse(c.x - pr * 0.45f, c.y - pr * 0.45f, pr * 0.9f, pr * 0.9f);   // cable plugged in
    }
    for (int o = 0; o < lmw::dsp::numOutputs; ++o) {
        const auto c = toScreen(sc::outputPoint(static_cast<lmw::dsp::Out>(o), layout));
        g.fillEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr);
    }
    if (rings) {   // Spec S6.19: type rings around the ports (audio solid, CV double, gate dashed); S6.20: on / off
        const float rr = pr + std::max(2.0f, 0.3f * s), rw = std::max(1.5f, 0.2f * s);
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto in = static_cast<lmw::dsp::In>(i);
            const auto t = lmw::dsp::typeOf(in);
            lmw::ui::drawTypeRing(g, toScreen(sc::inputPoint(in, layout)), rr, rw, t, colour(palette.socket[static_cast<std::size_t>(t)]));
        }
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) {
            const auto out = static_cast<lmw::dsp::Out>(o);
            const auto t = lmw::dsp::typeOf(out);
            lmw::ui::drawTypeRing(g, toScreen(sc::outputPoint(out, layout)), rr, rw, t, colour(palette.socket[static_cast<std::size_t>(t)]));
        }
        g.setColour(kFg);
    }

    // Editing: the cable being dragged (from its fixed end to the target port or the mouse); the drop target
    // and the keyboard port cursor are on the highlight box (S6.15, drawn with the ports).
    if (grab) {
        const auto fixed = toScreen(portPoint(grab->fixed));
        const auto end = target ? toScreen(portPoint(*target)) : loose;
        const auto cv = grab->fixed.output ? sc::curve(toCanvas(fixed), toCanvas(end)) : sc::curve(toCanvas(end), toCanvas(fixed));
        juce::Path path;
        path.startNewSubPath(toScreen(cv[0]));
        path.cubicTo(toScreen(cv[1]), toScreen(cv[2]), toScreen(cv[3]));
        g.setColour(kBg);
        g.strokePath(path, juce::PathStrokeType(lineW * 1.8f + 2.0f * std::max(1.0f, 0.12f * s)));
        g.setColour(kFg);
        g.strokePath(path, juce::PathStrokeType(lineW * 1.8f));
    }
    if (moving && moved) {                                 // Phase 12c: where the dragged block(s) would land (S6.23)
        const sc::Point delta{moving->to.x - moving->from.x, moving->to.y - moving->from.y};
        const bool fits = layout.fitsGroup(moving->group, delta);
        g.setColour(kFg);
        for (const auto b : moving->group) {
            auto box = layout.box(b);
            box.x += std::round(delta.x);
            box.y += std::round(delta.y);
            const auto r = toScreen(box);
            if (fits) {
                g.drawRect(r, std::max(2.0f, 0.25f * s));
            } else {                                       // does not fit: dashed outlines, the drop is refused
                juce::Path outline, dashedOutline;
                outline.addRectangle(r);
                const float d[] = {0.6f * s, 0.5f * s};
                juce::PathStrokeType(std::max(2.0f, 0.25f * s)).createDashedStroke(dashedOutline, outline, d, 2);
                g.fillPath(dashedOutline);
            }
        }
    }
    if (band) {                                            // S6.23: the selection rectangle, dashed
        const auto a = toScreen(band->first), b = toScreen(band->second);
        juce::Path outline, dashedOutline;
        outline.addRectangle(juce::Rectangle<float>(a, b));
        const float d[] = {6.0f, 4.0f};
        juce::PathStrokeType(1.5f).createDashedStroke(dashedOutline, outline, d, 2);
        g.setColour(kFg);
        g.fillPath(dashedOutline);
    }

    // Cable levels below 100 %, black on a white box at the middle of the cable.
    g.setFont(juce::FontOptions(1.6f * s));
    for (const auto& c : conns) {
        if (c.normalled || c.level == 1.0f) continue;   // S5.21: 0 and negative levels shown too
        const auto m = toScreen(sc::curveAt(sc::curve(sc::outputPoint(c.from, layout), sc::inputPoint(c.to, layout)), 0.5f));
        const auto box = juce::Rectangle<float>(4.2f * s, 2.0f * s).withCentre(m);
        g.setColour(kBg);
        g.fillRect(box);
        g.setColour(kFg);
        g.drawRect(box, 1.0f);
        g.drawText(text(sc::percent(c.level)), box, juce::Justification::centred, false);
    }

    // Legend.
    g.setFont(juce::FontOptions(1.7f * s));
    const auto l0 = toScreen(sc::Point{2, layout.pageHeight() - 2.5f});   // S6.22 (39b): at the page's bottom
    juce::Path dashedSample, sample;
    sample.startNewSubPath(l0.x, l0.y);
    sample.lineTo(l0.x + 5 * s, l0.y);
    const float dash[] = {0.8f * s, 0.6f * s};
    juce::PathStrokeType(lineW).createDashedStroke(dashedSample, sample, dash, 2);
    g.fillPath(dashedSample);
    g.drawText("normalled", juce::Rectangle<float>(l0.x + 6 * s, l0.y - s, 12 * s, 2 * s), juce::Justification::centredLeft, false);
    juce::Path solid;
    solid.startNewSubPath(l0.x + 19 * s, l0.y);
    solid.lineTo(l0.x + 24 * s, l0.y);
    g.setColour(colour(palette.cable));                    // grey at rest (user 2026-11-22): as a cable looks on white
    g.strokePath(solid, juce::PathStrokeType(lineW * 1.8f));
    g.setColour(kFg);
    g.drawText("cable (black at the hovered port)", juce::Rectangle<float>(l0.x + 25 * s, l0.y - s, 40 * s, 2 * s),
               juce::Justification::centredLeft, false);
    juce::Path dottedSample, sample2;
    sample2.startNewSubPath(l0.x + 58 * s, l0.y);
    sample2.lineTo(l0.x + 63 * s, l0.y);
    juce::PathStrokeType(std::max(1.0f, 0.16f * s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
        .createDashedStroke(dottedSample, sample2, dot, 2);
    g.fillPath(dottedSample);
    g.drawText("internal (set by a fader)", juce::Rectangle<float>(l0.x + 64 * s, l0.y - s, 30 * s, 2 * s),
               juce::Justification::centredLeft, false);
    // Spec S6.26: the outlines of a unit without effect and of one passing the sound unchanged (after the rings).
    {
        const float shortDash[] = {0.7f * s, 0.6f * s}, longDash[] = {2.0f * s, 0.7f * s};
        float x = l0.x + 140 * s;
        for (const auto& [dash, label] : {std::pair{shortDash, "no effect"}, std::pair{longDash, "Mix 0"}}) {
            juce::Path box, dashed;
            box.addRectangle(x, l0.y - 0.9f * s, 5.0f * s, 1.8f * s);
            juce::PathStrokeType(std::max(2.0f, 0.25f * s)).createDashedStroke(dashed, box, dash, 2);
            g.fillPath(dashed);
            g.drawText(label, juce::Rectangle<float>(x + 5.6f * s, l0.y - s, 9 * s, 2 * s), juce::Justification::centredLeft, false);
            x += 15.5f * s;
        }
    }
    // Spec S6.19: the socket type rings (S6.20: not while they are switched off).
    if (!rings) return;
    const std::pair<lmw::dsp::SignalType, const char*> kinds[] = {
        {lmw::dsp::SignalType::Audio, "audio"}, {lmw::dsp::SignalType::Cv, "CV (modulation)"}, {lmw::dsp::SignalType::Gate, "gate"}};
    float x = l0.x + 98 * s;
    for (const auto& [t, label] : kinds) {
        g.setColour(kBg);
        g.fillEllipse(x - 0.55f * s, l0.y - 0.55f * s, 1.1f * s, 1.1f * s);
        g.setColour(kFg);
        g.drawEllipse(x - 0.55f * s, l0.y - 0.55f * s, 1.1f * s, 1.1f * s, std::max(1.0f, 0.15f * s));
        lmw::ui::drawTypeRing(g, {x, l0.y}, 0.95f * s, std::max(1.5f, 0.2f * s), t, colour(palette.socket[static_cast<std::size_t>(t)]));
        g.setColour(kFg);
        const float w = t == lmw::dsp::SignalType::Cv ? 17.0f : 7.0f;
        g.drawText(label, juce::Rectangle<float>(x + 1.6f * s, l0.y - s, w * s, 2 * s), juce::Justification::centredLeft, false);
        x += (w + 4.0f) * s;
    }
}

std::vector<std::pair<std::string, juce::Rectangle<int>>> SchematicView::blockAreas() const {
    std::vector<std::pair<std::string, juce::Rectangle<int>>> out;
    for (std::size_t i = 0; i < sc::blocks().size(); ++i)
        out.emplace_back(std::string(sc::blocks()[i].name), toScreen(layout.box(i)).getSmallestIntegerContainer());
    return out;
}

juce::String SchematicView::getTooltip() {
    if (grab) return {};
    const auto note = [](const std::string& n) { return n.empty() ? juce::String() : "\n" + juce::String::fromUTF8(n.c_str()); };
    const auto c = toCanvas(getMouseXYRelative().toFloat());
    const auto& p = patch();
    if (const auto hit = sc::portAt(c, 1.2f, std::nullopt, layout)) {
        if (!hit->input)
            return lmw::ui::typeLine(false, hit->index) + "\n"
                 + juce::String::fromUTF8(sc::describeOutput(p, static_cast<lmw::dsp::Out>(hit->index)).c_str())
                 + note(sc::lfoDelayOutputNote(static_cast<lmw::dsp::Out>(hit->index), delays()))
                 + "\nDrag to an input to patch a cable. Right-click or Enter: cable menu.";
        const auto in = static_cast<lmw::dsp::In>(hit->index);
        return lmw::ui::typeLine(true, hit->index) + "\n" + juce::String::fromUTF8(sc::describeInput(p, in).c_str())
             + note(sc::lfoDelayNote(p, in, delays()))
             + "\nDrag to an output to add a cable (up to 4 per input)"
             + (p.isPatched(in) ? "; drag the newest cable away to remove it. Delete: remove all." : ".")
             + "\nRight-click or Enter: cable menu.";
    }
    if (const auto bi = sc::blockAt(c, layout)) {
            const auto& b = sc::blocks()[*bi];
            juce::String t = text(b.name);
            if (useNote && !b.outputs.empty())                    // S6.26: why it has no effect
                if (const auto n = useNote(lmw::dsp::portUnit(b.outputs.front())); n.isNotEmpty()) t += "\n" + n;
            for (auto in : b.inputs) t += "\n" + juce::String::fromUTF8(sc::describeInput(p, in).c_str());
            for (auto out : b.outputs) t += "\n" + juce::String::fromUTF8(sc::describeOutput(p, out).c_str());
            if (const auto bn = sc::blockNote(b.name, shownKeys[0], shownKeys[1], delays()); !bn.empty())
                t += "\n" + juce::String::fromUTF8(bn.c_str());
            return t + "\nDrag the symbol to move it (keyboard: Shift+arrows on one of its ports).";
    }
    return (selected.empty() ? juce::String() : juce::String(static_cast<int>(selected.size())) + (selected.size() == 1 ? " block" : " blocks")
                                                + " selected: drag one to move them; Escape: none\n")   // S6.23
         + "Zoom " + juce::String(juce::roundToInt(zoomFactor() * 100.0f)) + juce::String::fromUTF8(" % \xe2\x80\x94 Ctrl + wheel or Ctrl + / \xe2\x88\x92 to zoom, Ctrl 0 back to 100 %");   // S6.22
}

// ---- Phase 12b: editing, same rules as the Synth tab (ui/CableEdit.h) ----

sc::Point SchematicView::portPoint(PortRef p) const {
    return p.output ? sc::outputPoint(static_cast<lmw::dsp::Out>(p.index), layout) : sc::inputPoint(static_cast<lmw::dsp::In>(p.index), layout);
}

std::optional<SchematicView::PortRef> SchematicView::portUnder(juce::Point<float> where, std::optional<bool> wantInput) const {
    if (const auto h = sc::portAt(toCanvas(where), 1.6f, wantInput, layout)) return PortRef{!h->input, h->index};
    return std::nullopt;
}

void SchematicView::openMenu(PortRef p) {
    const auto c = toScreen(portPoint(p)).roundToInt();
    const int r = juce::roundToInt(scale());
    showMenu(p.output, p.index, localAreaToGlobal(juce::Rectangle<int>(c.x - r, c.y - r, 2 * r, 2 * r)));
}

void SchematicView::cancelDrag() {
    moving.reset();
    moved = false;
    grab.reset();
    target.reset();
    pressed.reset();
    repaint();
}

void SchematicView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    pressed = portUnder(e.position);
    moving.reset();
    moved = false;
    band.reset();
    if (!pressed) {                                        // Phase 12c: on a symbol's body: move it (S6.23: its group)
        if (e.mods.isPopupMenu()) return;
        const bool adding = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown();
        if (const auto bi = sc::blockAt(toCanvas(e.position), layout)) {
            if (adding) {                                  // S6.23: Ctrl / Shift-click adds or takes out
                if (isSelected(*bi)) selected.erase(std::find(selected.begin(), selected.end(), *bi));
                else selected.push_back(*bi);
                repaint();
                return;
            }
            if (!isSelected(*bi)) selected = {*bi};
            moving = BlockMove{*bi, selected, layout.position(*bi), layout.position(*bi)};
            repaint();
            return;
        }
        if (!adding) selected.clear();                     // S6.23: empty space: a selection rectangle
        band = std::make_pair(toCanvas(e.position), toCanvas(e.position));
        bandAdds = adding;
        repaint();
        return;
    }
    const auto order = sc::portOrder();
    for (std::size_t k = 0; k < order.size(); ++k)
        if (order[k].input == !pressed->output && order[k].index == pressed->index) cursor = k;
    repaint();
    if (e.mods.isPopupMenu()) {
        openMenu(*pressed);
        pressed.reset();
    }
}

void SchematicView::mouseDrag(const juce::MouseEvent& e) {
    if (band) { band->second = toCanvas(e.position); repaint(); return; }   // S6.23
    if (moving) {
        if (!moved && e.getDistanceFromDragStart() < 3) return;
        moved = true;
        const float s = scale();
        moving->to = {std::round(moving->from.x + static_cast<float>(e.getDistanceFromDragStartX()) / s),
                      std::round(moving->from.y + static_cast<float>(e.getDistanceFromDragStartY()) / s)};
        repaint();
        return;
    }
    if (!pressed || e.mods.isPopupMenu()) return;
    if (!grab) {
        if (e.getDistanceFromDragStart() < 3) return;
        grab = lmw::ui::cables::grab(patch(), *pressed);
    }
    loose = e.position;
    target = portUnder(e.position, grab->fixed.output);   // fixed end an output -> look for inputs
    repaint();
}

void SchematicView::mouseUp(const juce::MouseEvent& e) {
    pressed.reset();
    if (band) {                                            // S6.23: select what the rectangle touches
        if (e.getDistanceFromDragStart() >= 3) {
            for (const auto b : layout.blocksIn(band->first, band->second))
                if (!isSelected(b)) selected.push_back(b);
        }
        band.reset();
        repaint();
        return;
    }
    if (moving) {
        const auto m = *moving;
        const bool wasMoved = moved;
        moving.reset();
        moved = false;
        // S6.23: the whole group by the grabbed block's step; one change of the layout, one undo step.
        if (wasMoved && layout.moveGroup(m.group, {m.to.x - m.from.x, m.to.y - m.from.y})) layoutChanged();
        else if (!wasMoved) selected = {m.block};          // a plain click: that block alone
        repaint();
        return;
    }
    if (!grab) return;
    target = portUnder(e.position, grab->fixed.output);
    const auto g = *grab;
    const auto t = target;
    grab.reset();
    target.reset();
    for (const auto& op : lmw::ui::cables::drop(g, t))
        if (!connect(op.input, op.output, op.level)) break;   // e.g. a full input: a moved cable stays
    repaint();
}

bool SchematicView::keyPressed(const juce::KeyPress& key) {
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::escapeKey && (grab || moving || band)) { band.reset(); cancelDrag(); return true; }
    if (code == juce::KeyPress::escapeKey && !selected.empty()) { selected.clear(); repaint(); return true; }   // S6.23
    const auto mods = key.getModifiers();
    if ((mods.isCommandDown() || mods.isCtrlDown()) && !mods.isAltDown()) {   // Spec S6.22: zoom keys
        const auto c = key.getTextCharacter();
        if (c == '+' || c == '=' || code == juce::KeyPress::numberPadAdd) { setZoom(zoomStep + 1); return true; }
        if (c == '-' || code == juce::KeyPress::numberPadSubtract) { setZoom(zoomStep - 1); return true; }
        if (c == '0' || code == juce::KeyPress::numberPad0) { setZoom(1); return true; }
        if (c == 'a' || c == 'A' || code == 'a' || code == 'A') {   // S6.23: Ctrl + A selects every block
            selected.clear();
            for (std::size_t i = 0; i < sc::blocks().size(); ++i) selected.push_back(i);
            repaint();
            return true;
        }
    }
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    const auto order = sc::portOrder();
    if (order.empty()) return false;
    cursor = std::min(cursor, order.size() - 1);
    if (mods.isShiftDown() && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey
                               || code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)) {
        // Phase 12c: Shift+arrow moves the block that holds the port cursor one grid step (if it fits).
        const auto bi = sc::blockOf(order[cursor]);
        const sc::Point d{code == juce::KeyPress::leftKey ? -1.0f : code == juce::KeyPress::rightKey ? 1.0f : 0.0f,
                          code == juce::KeyPress::upKey ? -1.0f : code == juce::KeyPress::downKey ? 1.0f : 0.0f};
        // S6.23: the whole selection when the cursor's block is in it, else that block alone.
        const std::vector<std::size_t> group = isSelected(bi) ? selected : std::vector<std::size_t>{bi};
        if (layout.moveGroup(group, d)) layoutChanged();
        repaint();
        return true;
    }
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::leftKey) {      // next / previous port
        cursor = code == juce::KeyPress::rightKey ? (cursor + 1) % order.size() : (cursor + order.size() - 1) % order.size();
        repaint();
        return true;
    }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::upKey) {        // first port of next / previous symbol
        cursor = sc::blockJump(cursor, code == juce::KeyPress::downKey ? 1 : -1);
        repaint();
        return true;
    }
    const PortRef port{!order[cursor].input, order[cursor].index};
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && mods.isShiftDown())) {
        openMenu(port);
        return true;
    }
    if (!port.output && (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)) {
        connect(port.index, lmw::dsp::Patch::none, 1.0f);
        repaint();
        return true;
    }
    return false;                                          // QWERTY note keys: handled by the editor
}

// ---- Phase 12c: movable blocks ----

void SchematicView::setLayout(const juce::String& text) {
    shownLayout = text;
    layout = sc::Layout::parse(text.toStdString()).value_or(sc::Layout());
    selected.clear();                                      // S6.23: a new layout (a patch loaded): no selection
    updatePage();
    if (onSizeChange) onSizeChange();                      // a stored layout may reach further (39b)
    repaint();
}

void SchematicView::poll(const juce::String& layoutText) {
    poll();
    if (layoutText != shownLayout && !moving) setLayout(layoutText);   // e.g. a patch file was loaded
}

void SchematicView::resetLayout() {
    layout = sc::Layout();
    updatePage();
    layoutChanged();
    repaint();
}

void SchematicView::layoutChanged() {
    shownLayout = layout.isDefault() ? juce::String() : juce::String(layout.serialize());
    if (onLayoutChange) onLayoutChange(shownLayout);
    if (onSizeChange) onSizeChange();                      // S6.22 (39b): the page grows with the blocks
}

void SchematicView::mouseExit(const juce::MouseEvent&) {
    if (hover) { hover.reset(); repaint(); }
}

void SchematicView::mouseMove(const juce::MouseEvent& e) {
    const auto c = toCanvas(e.position);
    if (const auto h = portUnder(e.position); h != hover) { hover = h; repaint(); }
    if (portUnder(e.position)) setMouseCursor(juce::MouseCursor::CrosshairCursor);
    else if (sc::blockAt(c, layout)) setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
