// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SchematicView.h"
#include <cmath>

namespace sc = lmw::ui::schematic;

namespace {
juce::Colour colour(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
const juce::Colour kBg = colour(lmw::ui::theme::background);
const juce::Colour kFg = colour(lmw::ui::theme::foreground);
juce::String text(std::string_view s) { return juce::String::fromUTF8(s.data(), static_cast<int>(s.size())); }
}

SchematicView::SchematicView(std::function<const lmw::dsp::Patch&()> p, std::function<std::array<float, 2>()> k,
                             std::function<bool(int, int, float)> c, std::function<void(bool, int, juce::Rectangle<int>)> m)
    : patch(std::move(p)), keyTrack(std::move(k)), connect(std::move(c)), showMenu(std::move(m)) {
    setWantsKeyboardFocus(true);
    setTitle("Schematic");
    setDescription("The instrument as a schematic: dashed lines are normalled connections, solid lines cables, "
                   "dotted arrows internal links set by a fader. Drag between ports to patch a cable; arrow keys "
                   "move a port cursor, Enter opens its cable menu, Delete removes an input's cables.");
}

void SchematicView::poll() {
    if (const auto k = keyTrack(); k != shownKeys) {
        shownKeys = k;
        if (isVisible()) repaint();
    }
}

float SchematicView::scale() const noexcept {
    return std::min(static_cast<float>(getWidth()) / sc::canvasWidth, static_cast<float>(getHeight()) / sc::canvasHeight);
}

juce::Point<float> SchematicView::toScreen(sc::Point p) const noexcept {
    const float s = scale();
    const float ox = (static_cast<float>(getWidth()) - sc::canvasWidth * s) / 2;
    const float oy = (static_cast<float>(getHeight()) - sc::canvasHeight * s) / 2;
    return {ox + p.x * s, oy + p.y * s};
}

sc::Point SchematicView::toCanvas(juce::Point<float> p) const noexcept {
    const float s = scale();
    const float ox = (static_cast<float>(getWidth()) - sc::canvasWidth * s) / 2;
    const float oy = (static_cast<float>(getHeight()) - sc::canvasHeight * s) / 2;
    return {(p.x - ox) / s, (p.y - oy) / s};
}

juce::Rectangle<float> SchematicView::toScreen(const sc::Rect& r) const noexcept {
    const auto a = toScreen(sc::Point{r.x, r.y});
    return {a.x, a.y, r.w * scale(), r.h * scale()};
}

void SchematicView::drawBlock(juce::Graphics& g, const sc::Block& b) const {
    const float s = scale();
    const auto r = toScreen(b.box);
    const auto fill = colour(palette.unitColour(b.unit()));
    const float stroke = std::max(1.0f, 0.18f * s);
    const juce::FontOptions label(1.7f * s);

    auto innerLabels = [&](float labelLeft) {   // output labels inside the right edge, for several outputs
        if (b.outputs.size() < 2) return;
        g.setFont(label);
        for (std::size_t k = 0; k < b.outputs.size(); ++k) {
            const auto p = toScreen(sc::outputPoint(b.outputs[k]));
            g.drawText(text(sc::portLabel(lmw::dsp::outputNames[static_cast<std::size_t>(b.outputs[k])])),
                       juce::Rectangle<float>(labelLeft, p.y - s, p.x - labelLeft - 0.6f * s, 2 * s), juce::Justification::centredRight, false);
        }
    };

    switch (b.shape) {
    case sc::Shape::Triangle: {
        juce::Path t;
        t.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
        g.setColour(fill);
        g.fillPath(t);
        g.setColour(kFg);
        g.strokePath(t, juce::PathStrokeType(stroke));
        if (b.inputs.size() > 1) {                    // input labels inside the left edge
            g.setFont(label);
            for (auto in : b.inputs) {
                const auto p = toScreen(sc::inputPoint(in));
                g.drawText(text(sc::portLabel(lmw::dsp::inputNames[static_cast<std::size_t>(in)])),
                           juce::Rectangle<float>(p.x + 0.7f * s, p.y - s, r.getWidth() * 0.6f, 2 * s), juce::Justification::centredLeft, false);
            }
        }
        break;
    }
    case sc::Shape::Adsr: {
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(kFg);
        g.drawRect(r, stroke);
        const auto at = [&](float fx, float fy) { return juce::Point<float>(r.getX() + fx * r.getWidth(), r.getY() + fy * r.getHeight()); };
        juce::Path curve;                              // attack, decay, sustain, release
        curve.startNewSubPath(at(0.12f, 0.85f));
        curve.lineTo(at(0.3f, 0.15f));
        curve.lineTo(at(0.45f, 0.45f));
        curve.lineTo(at(0.7f, 0.45f));
        curve.lineTo(at(0.88f, 0.85f));
        g.strokePath(curve, juce::PathStrokeType(stroke * 1.3f, juce::PathStrokeType::mitered));
        break;
    }
    case sc::Shape::Keys: {
        g.setColour(kBg);
        g.fillRect(r);
        g.setColour(kFg);
        g.drawRect(r, stroke);
        // Four white keys and one black key (between the first two), in the left part of the block.
        const auto kb = r.withWidth(r.getWidth() * 0.55f).reduced(0.8f * s);
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
        g.drawRect(r, stroke);
        // A white-noise trace: fixed pseudo-random samples (the same picture every time).
        const auto area = r.withWidth(r.getWidth() * 0.55f).reduced(0.6f * s, 1.0f * s);
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
        g.fillEllipse(circle.withSizeKeepingCentre(d * 0.25f, d * 0.25f));
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
        const auto cv = sc::curve(sc::outputPoint(c.from), sc::inputPoint(c.to));
        juce::Path path;
        path.startNewSubPath(toScreen(cv[0]));
        path.cubicTo(toScreen(cv[1]), toScreen(cv[2]), toScreen(cv[3]));
        return path;
    };
    g.setColour(kFg);
    for (const auto& c : conns) {
        if (!c.normalled) continue;
        juce::Path dashed;
        const float dash[] = {0.8f * s, 0.6f * s};
        juce::PathStrokeType(lineW).createDashedStroke(dashed, pathOf(c), dash, 2);
        g.fillPath(dashed);
    }
    // Internal links (fader-set): thin, dotted, behind the blocks; the arrow ends on the block's edge.
    shownKeys = keyTrack();
    const auto internal = sc::internalLinks(shownKeys[0], shownKeys[1]);
    const float dot[] = {0.25f * s, 0.55f * s};
    for (const auto& l : internal) {
        const auto* b = sc::findBlock(l.block);
        if (b == nullptr) continue;
        const auto cv = sc::internalCurve(sc::outputPoint(l.from), sc::internalTarget(*b));
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
    for (const auto& b : sc::blocks()) drawBlock(g, b);
    for (const auto& c : conns) {
        if (c.normalled) continue;
        if (grab && grab->moving() && static_cast<int>(c.to) == grab->detachedInput
            && static_cast<int>(c.from) == grab->detachedSource) continue;          // being moved
        const auto path = pathOf(c);
        g.setColour(kBg);
        g.strokePath(path, juce::PathStrokeType(lineW * 1.8f + 2.0f * std::max(1.0f, 0.12f * s)));
        g.setColour(kFg);
        g.strokePath(path, juce::PathStrokeType(lineW * 1.8f));
    }

    // Internal link labels just below the arrowheads, on white.
    g.setFont(juce::FontOptions(1.5f * s, juce::Font::italic));
    for (const auto& l : internal) {
        const auto* b = sc::findBlock(l.block);
        if (b == nullptr) continue;
        const auto tip = toScreen(sc::internalTarget(*b));
        const auto t = juce::String::fromUTF8(l.label.c_str());
        const float w = juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(1.5f * s, juce::Font::italic)), t) + 0.6f * s;
        const juce::Rectangle<float> box(tip.x + 0.9f * s, tip.y + 0.2f * s, w, 1.9f * s);
        g.setColour(kBg);
        g.fillRect(box);
        g.setColour(kFg);
        g.drawText(t, box, juce::Justification::centred, false);
    }

    // Block names above the blocks, last, on a white backing so a cable passing behind keeps them readable.
    const juce::Font nameFont(juce::FontOptions(2.1f * s, juce::Font::bold));
    g.setFont(nameFont);
    for (const auto& b : sc::blocks()) {
        const auto r = toScreen(b.box);
        const auto name = text(b.name);
        const float w = juce::GlyphArrangement::getStringWidth(nameFont, name) + 0.8f * s;
        const juce::Rectangle<float> area(r.getCentreX() - w / 2, r.getY() - sc::nameHeight * s, w, sc::nameHeight * s - 0.3f * s);
        g.setColour(kBg);
        g.fillRect(area);
        g.setColour(kFg);
        g.drawText(name, area, juce::Justification::centred, false);
    }

    // Ports: inputs white with a black ring, outputs black (as on the Synth tab, S6.6).
    const float pr = 0.55f * s;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        const auto c = toScreen(sc::inputPoint(in));
        g.setColour(kBg);
        g.fillEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr);
        g.setColour(kFg);
        g.drawEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr, std::max(1.0f, 0.15f * s));
        if (p.isPatched(in)) g.fillEllipse(c.x - pr * 0.45f, c.y - pr * 0.45f, pr * 0.9f, pr * 0.9f);   // cable plugged in
    }
    for (int o = 0; o < lmw::dsp::numOutputs; ++o) {
        const auto c = toScreen(sc::outputPoint(static_cast<lmw::dsp::Out>(o)));
        g.fillEllipse(c.x - pr, c.y - pr, 2 * pr, 2 * pr);
    }

    // Editing: the cable being dragged (from its fixed end to the target port or the mouse), a ring around
    // the drop target, and the keyboard port cursor (a square) while the view has focus.
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
        if (target) {
            const auto t = toScreen(portPoint(*target));
            g.drawEllipse(t.x - pr * 1.9f, t.y - pr * 1.9f, pr * 3.8f, pr * 3.8f, std::max(1.5f, 0.2f * s));
        }
    }
    if (hasKeyboardFocus(false)) {
        const auto order = sc::portOrder();
        if (cursor < order.size()) {
            const auto c = toScreen(portPoint({!order[cursor].input, order[cursor].index}));
            g.setColour(kFg);
            g.drawRect(juce::Rectangle<float>(pr * 4.0f, pr * 4.0f).withCentre(c), std::max(2.0f, 0.25f * s));
        }
    }

    // Cable levels below 100 %, black on a white box at the middle of the cable.
    g.setFont(juce::FontOptions(1.6f * s));
    for (const auto& c : conns) {
        if (c.normalled || c.level >= 1.0f) continue;
        const auto m = toScreen(sc::curveAt(sc::curve(sc::outputPoint(c.from), sc::inputPoint(c.to)), 0.5f));
        const auto box = juce::Rectangle<float>(4.2f * s, 2.0f * s).withCentre(m);
        g.setColour(kBg);
        g.fillRect(box);
        g.setColour(kFg);
        g.drawRect(box, 1.0f);
        g.drawText(text(sc::percent(c.level)), box, juce::Justification::centred, false);
    }

    // Legend.
    g.setFont(juce::FontOptions(1.7f * s));
    const auto l0 = toScreen(sc::Point{2, sc::canvasHeight - 2.5f});
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
    g.strokePath(solid, juce::PathStrokeType(lineW * 1.8f));
    g.drawText("cable (level shown when below 100 %)", juce::Rectangle<float>(l0.x + 25 * s, l0.y - s, 40 * s, 2 * s),
               juce::Justification::centredLeft, false);
    juce::Path dottedSample, sample2;
    sample2.startNewSubPath(l0.x + 58 * s, l0.y);
    sample2.lineTo(l0.x + 63 * s, l0.y);
    juce::PathStrokeType(std::max(1.0f, 0.16f * s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
        .createDashedStroke(dottedSample, sample2, dot, 2);
    g.fillPath(dottedSample);
    g.drawText("internal (set by a fader)", juce::Rectangle<float>(l0.x + 64 * s, l0.y - s, 30 * s, 2 * s),
               juce::Justification::centredLeft, false);
}

juce::String SchematicView::getTooltip() {
    if (grab) return {};
    const auto c = toCanvas(getMouseXYRelative().toFloat());
    const auto& p = patch();
    if (const auto hit = sc::portAt(c, 1.2f)) {
        if (!hit->input)
            return juce::String::fromUTF8(sc::describeOutput(p, static_cast<lmw::dsp::Out>(hit->index)).c_str())
                 + "\nDrag to an input to patch a cable. Right-click or Enter: cable menu.";
        const auto in = static_cast<lmw::dsp::In>(hit->index);
        return juce::String::fromUTF8(sc::describeInput(p, in).c_str())
             + "\nDrag to an output to add a cable (up to 4 per input)"
             + (p.isPatched(in) ? "; drag the newest cable away to remove it. Delete: remove all." : ".")
             + "\nRight-click or Enter: cable menu.";
    }
    for (const auto& b : sc::blocks())
        if (b.box.contains(c)) {
            juce::String t = text(b.name);
            for (auto in : b.inputs) t += "\n" + juce::String::fromUTF8(sc::describeInput(p, in).c_str());
            for (auto out : b.outputs) t += "\n" + juce::String::fromUTF8(sc::describeOutput(p, out).c_str());
            if (const auto note = sc::blockNote(b.name, shownKeys[0], shownKeys[1]); !note.empty())
                t += "\n" + juce::String::fromUTF8(note.c_str());
            return t;
        }
    return {};
}

// ---- Phase 12b: editing, same rules as the Synth tab (ui/CableEdit.h) ----

sc::Point SchematicView::portPoint(PortRef p) const {
    return p.output ? sc::outputPoint(static_cast<lmw::dsp::Out>(p.index)) : sc::inputPoint(static_cast<lmw::dsp::In>(p.index));
}

std::optional<SchematicView::PortRef> SchematicView::portUnder(juce::Point<float> where, std::optional<bool> wantInput) const {
    if (const auto h = sc::portAt(toCanvas(where), 1.6f, wantInput)) return PortRef{!h->input, h->index};
    return std::nullopt;
}

void SchematicView::openMenu(PortRef p) {
    const auto c = toScreen(portPoint(p)).roundToInt();
    const int r = juce::roundToInt(scale());
    showMenu(p.output, p.index, localAreaToGlobal(juce::Rectangle<int>(c.x - r, c.y - r, 2 * r, 2 * r)));
}

void SchematicView::cancelDrag() {
    grab.reset();
    target.reset();
    pressed.reset();
    repaint();
}

void SchematicView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    pressed = portUnder(e.position);
    if (!pressed) return;
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
    if (code == juce::KeyPress::escapeKey && grab) { cancelDrag(); return true; }
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown()) return false;
    const auto order = sc::portOrder();
    if (order.empty()) return false;
    cursor = std::min(cursor, order.size() - 1);
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::downKey) {
        cursor = (cursor + 1) % order.size();
        repaint();
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::upKey) {
        cursor = (cursor + order.size() - 1) % order.size();
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
