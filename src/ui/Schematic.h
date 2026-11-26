// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.7: the schematic view — geometry and connections, pure C++ (no JUCE) so it can be tested.
// Phase 12b: editable like the Synth tab (dragging cables between ports; CableEdit.h).
// Phase 12c: movable blocks — a Layout holds each block's position; saved with the state and patch files.
// Every module is a block on a fixed canvas (units, scaled to the window by the view), signal flowing
// left to right: inputs on a block's left edge, outputs on its right edge (a triangle's output at its tip).
// Connections come from the patch: an input's cables (pins with levels, S5.14), or its normalled source
// while it has none.
#pragma once
#include "dsp/Patch.h"
#include "ui/ParamText.h"
#include "ui/Theme.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::ui::schematic {

using lmw::dsp::In;
using lmw::dsp::Out;

// VCO, VCF, VCA, LFO, MIX: triangles pointing right; ENV: ADSR curve in a rectangle; KEYS: a small
// keyboard; NOISE: a noise trace in a rectangle; OUT: a circle with an arrow leaving it to the right.
enum class Shape : std::uint8_t { Triangle, Adsr, Keys, Noise, Out };

struct Point { float x = 0, y = 0; };
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const noexcept { return x + w; }
    float bottom() const noexcept { return y + h; }
    bool contains(Point p) const noexcept { return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom(); }
    bool intersects(const Rect& o) const noexcept { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
};

struct Block {
    std::string_view name;          // shown above the block
    Shape shape;
    Rect box;                        // canvas units
    std::vector<In> inputs;          // top to bottom on the left edge
    std::vector<Out> outputs;        // top to bottom on the right edge
    theme::Unit unit() const noexcept { return theme::unitOf(name); }
};

inline constexpr float canvasWidth = 168.0f, canvasHeight = 72.0f;
// Spec S6.22 (39b, user 2026-11-26: "allow elements to use the complete page"): blocks may be placed anywhere on the
// page the view shows (at least the canvas above); stored layouts are accepted up to this size.
inline constexpr float maxPageWidth = 1000.0f, maxPageHeight = 500.0f;   // 152 before the effects (S7.6), 136 before FORMANT (S5.16)
// Names are drawn inside the symbols (user 2026-11-13), so a block needs no room above it.

inline const std::vector<Block>& blocks() {
    static const std::vector<Block> b{
        {"LFO 1",  Shape::Triangle, {2, 6, 10, 9},    {In::Lfo1Rate}, {Out::Lfo1}},   // rate input: S5.19
        {"KEYS",   Shape::Keys,     {2, 23, 12, 11},  {}, {Out::KeysPitch, Out::KeysGate, Out::KeysVel, Out::KeysMod}},   // mod: S6.24
        // VCO 1-3: 12 x 14 like the VCFs since S5.20 (three inputs: pitch, fm, pm); 11 x 10 before
        {"VCO 1",  Shape::Triangle, {26, 2, 12, 14},  {In::Vco1Pitch, In::Vco1Fm, In::Vco1Pm}, {Out::Vco1}},
        {"VCO 2",  Shape::Triangle, {26, 17, 12, 14}, {In::Vco2Pitch, In::Vco2Fm, In::Vco2Pm}, {Out::Vco2}},
        {"VCO 3",  Shape::Triangle, {26, 32, 12, 14}, {In::Vco3Pitch, In::Vco3Fm, In::Vco3Pm}, {Out::Vco3}},
        {"NOISE",  Shape::Noise,    {26, 54, 11, 8},  {}, {Out::Noise, Out::Rand}},
        {"MIX",    Shape::Triangle, {48, 14, 12, 26}, {In::MixIn1, In::MixIn2, In::MixIn3, In::MixIn4}, {Out::Mix}},
        {"VCF 1",  Shape::Triangle, {68, 20, 12, 14}, {In::Vcf1In, In::Vcf1Cutoff, In::Vcf1Res}, {Out::Vcf1}},
        {"VCF 2",  Shape::Triangle, {86, 20, 12, 14}, {In::Vcf2In, In::Vcf2Cutoff, In::Vcf2Res}, {Out::Vcf2}},
        {"FORMANT", Shape::Triangle, {104, 20, 12, 14}, {In::FormantIn, In::FormantVowel}, {Out::Formant}},   // S5.16
        {"MIX 2",  Shape::Triangle, {121, 19, 10, 16}, {In::Mix2In1, In::Mix2In2, In::Mix2In3}, {Out::Mix2}},   // S5.26
        {"VCA",    Shape::Triangle, {136, 20, 12, 14},{In::VcaIn, In::VcaCv, In::VcaMod, In::VcaVel}, {Out::Vca}},
        {"OUT",    Shape::Triangle, {154, 22, 10, 10}, {In::OutIn, In::OutPan}, {Out::OutOut}},   // S7.6: feeds the effects; S5.25: pan
        {"ENV 1",  Shape::Adsr,     {50, 58, 12, 8},  {In::Env1Gate}, {Out::Env1}},
        {"ENV 2",  Shape::Adsr,     {50, 45, 12, 8},  {In::Env2Gate}, {Out::Env2}},
        {"LFO 2",  Shape::Triangle, {110, 50, 10, 9}, {In::Lfo2Rate}, {Out::Lfo2}},
        // Spec S7.6: the effects rack; MAIN (what is heard) has the line-out symbol OUT had before.
        {"COMB",   Shape::Triangle, {88, 40, 12, 10}, {In::CombIn}, {Out::CombOut}},
        {"ENSEMBLE", Shape::Triangle, {104, 40, 12, 10}, {In::EnsembleIn}, {Out::EnsembleOut}},   // S7.11
        {"DELAY",  Shape::Triangle, {120, 40, 12, 10}, {In::DelayIn}, {Out::DelayOut}},   // S7.8
        {"REVERB", Shape::Triangle, {136, 40, 12, 10}, {In::ReverbIn}, {Out::ReverbOut}},
        {"MAIN",   Shape::Out,      {152, 41, 8, 8}, {In::MainIn}, {}},
    };
    return b;
}

// A block's id in saved layouts: its name in lower case without spaces ("VCO 1" -> "vco1").
inline std::string blockId(std::string_view name) {
    std::string id;
    for (char c : name)
        if (c != ' ') id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return id;
}

// Phase 12c: where each block is (its top-left corner, canvas units, on a 1-unit grid). The default is the
// drawing of Phase 12a. A block may be moved anywhere on the page where it does not overlap another block and stays clear
// of the legend at the bottom. The page is the canvas, or larger: the part of the view the Schematic shows (S6.22, 39b).
class Layout {
public:
    Layout() { for (const auto& b : blocks()) pos.push_back({b.box.x, b.box.y}); }

    Point position(std::size_t i) const { return pos[i]; }
    Rect box(std::size_t i) const {
        auto r = blocks()[i].box;
        r.x = pos[i].x;
        r.y = pos[i].y;
        return r;
    }
    // The space a block takes: its box (names are inside; blocks may touch, not overlap).
    static Rect room(const Rect& r) noexcept { return r; }

    // Would block i fit at `p` (snapped to the grid)?
    bool fits(std::size_t i, Point p) const {
        p = snap(p);
        Rect r = blocks()[i].box;
        r.x = p.x;
        r.y = p.y;
        if (r.x < 0 || r.right() > pageW || r.y < 0 || r.bottom() > pageH - 4) return false;
        for (std::size_t j = 0; j < pos.size(); ++j)
            if (j != i && room(r).intersects(room(box(j)))) return false;
        return true;
    }
    // Spec S6.23 (Phase 40): a group of blocks moved together by `delta` (whole grid steps). It fits when every
    // block stays on the page, clear of the legend, and off every block outside the group.
    bool fitsGroup(const std::vector<std::size_t>& group, Point delta) const {
        delta = snap(delta);
        const auto inGroup = [&](std::size_t j) { return std::find(group.begin(), group.end(), j) != group.end(); };
        for (const auto i : group) {
            if (i >= pos.size()) return false;
            Rect r = box(i);
            r.x += delta.x;
            r.y += delta.y;
            if (r.x < 0 || r.right() > pageW || r.y < 0 || r.bottom() > pageH - 4) return false;
            for (std::size_t j = 0; j < pos.size(); ++j)
                if (!inGroup(j) && room(r).intersects(room(box(j)))) return false;
        }
        return true;
    }
    bool moveGroup(const std::vector<std::size_t>& group, Point delta) {
        if (group.empty() || !fitsGroup(group, delta)) return false;
        delta = snap(delta);
        for (const auto i : group) pos[i] = {pos[i].x + delta.x, pos[i].y + delta.y};
        return true;
    }
    // The blocks a selection rectangle (canvas units, any corner order) touches, in block order.
    std::vector<std::size_t> blocksIn(Point a, Point b) const {
        const Rect r{std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(b.x - a.x), std::fabs(b.y - a.y)};
        std::vector<std::size_t> out;
        for (std::size_t i = 0; i < pos.size(); ++i) if (r.intersects(box(i))) out.push_back(i);
        return out;
    }
    // S6.22 (39b): the page the blocks may use (canvas units), at least the canvas; and how far the blocks reach.
    void setPage(float w, float h) { pageW = std::clamp(w, canvasWidth, maxPageWidth); pageH = std::clamp(h, canvasHeight, maxPageHeight); }
    float pageWidth() const noexcept { return pageW; }
    float pageHeight() const noexcept { return pageH; }
    Point extent() const {
        Point e{0, 0};
        for (std::size_t i = 0; i < pos.size(); ++i) { const auto r = box(i); e.x = std::max(e.x, r.right()); e.y = std::max(e.y, r.bottom()); }
        return e;
    }
    // Moves block i to `p` (snapped) if it fits there.
    bool move(std::size_t i, Point p) {
        if (i >= pos.size() || !fits(i, p)) return false;
        pos[i] = snap(p);
        return true;
    }
    bool isDefault() const { return *this == Layout(); }
    bool operator==(const Layout& o) const {
        for (std::size_t i = 0; i < pos.size(); ++i)
            if (std::lround(pos[i].x) != std::lround(o.pos[i].x) || std::lround(pos[i].y) != std::lround(o.pos[i].y))
                return false;                       // positions are whole grid steps
        return true;
    }

    // Text form, saved with the state and in patch files: "lfo1:2,6;keys:2,23;..." (every block).
    std::string serialize() const {
        std::string s;
        for (std::size_t i = 0; i < pos.size(); ++i) {
            if (!s.empty()) s += ';';
            s += blockId(blocks()[i].name) + ':' + std::to_string(static_cast<int>(pos[i].x)) + ','
               + std::to_string(static_cast<int>(pos[i].y));
        }
        return s;
    }
    // Reads that text; blocks not named keep their default place. A block added in a later version (FORMANT,
    // S5.16; COMB, REVERB, MAIN, S7.6) that the text does not name, and whose default place is taken, gets the first free place, row
    // by row (a layout saved before it existed). Nothing (use the default) for an unknown block, a malformed
    // entry, or a result where a block does not fit.
    static std::optional<Layout> parse(std::string_view text) {
        Layout l;
        l.setPage(maxPageWidth, maxPageHeight);   // S6.22 (39b): a stored layout may use a large page
        std::vector<bool> named(l.pos.size(), false);
        while (!text.empty()) {
            const auto semi = text.find(';');
            auto item = trim(text.substr(0, semi));
            text = semi == std::string_view::npos ? std::string_view{} : text.substr(semi + 1);
            if (item.empty()) continue;
            const auto colon = item.find(':'), comma = item.find(',');
            if (colon == std::string_view::npos || comma == std::string_view::npos || comma < colon) return std::nullopt;
            const auto id = trim(item.substr(0, colon));
            int x = 0, y = 0;
            if (!number(trim(item.substr(colon + 1, comma - colon - 1)), x) || !number(trim(item.substr(comma + 1)), y))
                return std::nullopt;
            std::size_t i = 0;
            while (i < blocks().size() && blockId(blocks()[i].name) != id) ++i;
            if (i == blocks().size()) return std::nullopt;
            l.pos[i] = {static_cast<float>(x), static_cast<float>(y)};
            named[i] = true;
        }
        const auto later = [](std::string_view name) {   // blocks added after Phase 12c
            return name == "FORMANT" || name == "COMB" || name == "ENSEMBLE" || name == "DELAY" || name == "REVERB" || name == "MAIN"
                || name == "MIX 2";                                   // S5.26
        };
        for (std::size_t i = 0; i < l.pos.size(); ++i) {           // the others: inside, not on each other
            if (!named[i] && later(blocks()[i].name)) continue;
            const Rect r = l.box(i);
            if (r.x < 0 || r.right() > maxPageWidth || r.y < 0 || r.bottom() > maxPageHeight - 4) return std::nullopt;
            for (std::size_t j = 0; j < l.pos.size(); ++j)
                if (j != i && (named[j] || !later(blocks()[j].name)) && r.intersects(l.box(j))) return std::nullopt;
        }
        for (std::size_t i = 0; i < l.pos.size(); ++i) {
            if (named[i] || !later(blocks()[i].name) || l.fits(i, l.pos[i])) continue;
            bool placed = false;
            for (int y = 0; y <= static_cast<int>(canvasHeight) && !placed; ++y)
                for (int x = 0; x <= static_cast<int>(canvasWidth) && !placed; ++x)
                    placed = l.move(i, {static_cast<float>(x), static_cast<float>(y)});
            if (!placed) return std::nullopt;
        }
        return l;
    }

private:
    static Point snap(Point p) noexcept { return {std::round(p.x), std::round(p.y)}; }
    static std::string_view trim(std::string_view s) noexcept {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
        return s;
    }
    static bool number(std::string_view s, int& v) noexcept {
        const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
        return r.ec == std::errc{} && r.ptr == s.data() + s.size() && v >= 0 && v <= 1000;
    }
    std::vector<Point> pos;
    float pageW = canvasWidth, pageH = canvasHeight;
};

inline const Layout& defaultLayout() {
    static const Layout l;
    return l;
}

namespace detail {
inline float slot(const Rect& r, std::size_t k, std::size_t n) noexcept {
    return r.y + r.h * static_cast<float>(k + 1) / static_cast<float>(n + 1);
}
}

// Where an input / output sits on the canvas.
inline Point inputPoint(In in, const Layout& l = defaultLayout()) {
    const auto& bs = blocks();
    for (std::size_t i = 0; i < bs.size(); ++i)
        for (std::size_t k = 0; k < bs[i].inputs.size(); ++k)
            if (bs[i].inputs[k] == in) {
                const auto r = l.box(i);
                return {r.x, detail::slot(r, k, bs[i].inputs.size())};
            }
    return {};
}

inline Point outputPoint(Out out, const Layout& l = defaultLayout()) {
    const auto& bs = blocks();
    for (std::size_t i = 0; i < bs.size(); ++i)
        for (std::size_t k = 0; k < bs[i].outputs.size(); ++k)
            if (bs[i].outputs[k] == out) {
                const auto r = l.box(i);
                if (bs[i].shape == Shape::Triangle) return {r.right(), r.y + r.h / 2};   // the tip
                return {r.right(), detail::slot(r, k, bs[i].outputs.size())};
            }
    return {};
}

// The block under a canvas point.
inline std::optional<std::size_t> blockAt(Point p, const Layout& l = defaultLayout()) {
    for (std::size_t i = 0; i < blocks().size(); ++i)
        if (l.box(i).contains(p)) return i;
    return std::nullopt;
}

// Short port label shown next to the port ("vcf1.cutoff" -> "cutoff", "keys.gate" -> "gate").
inline std::string_view portLabel(std::string_view name) noexcept { return name.substr(name.find('.') + 1); }

struct Connection {
    Out from;
    In to;
    float level = 1.0f;
    bool normalled = false;          // drawn dashed
};

// Every connection the patch makes: cables (pins) of patched inputs, normalled sources of the others.
inline std::vector<Connection> connections(const lmw::dsp::Patch& p) {
    std::vector<Connection> c;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<In>(i);
        if (p.isPatched(in)) {
            for (int k = 0; k < p.pinCount(in); ++k) {
                const auto& pin = p.pin(in, k);
                c.push_back({static_cast<Out>(pin.source), in, pin.level, false});
            }
        } else {                                     // normalled: 1 source, or 2 at 50 % (parallel VCFs, S5.17)
            const auto ns = p.normals(in);
            for (int k = 0; k < ns.n; ++k) c.push_back({ns.s[static_cast<std::size_t>(k)].source, in, ns.s[static_cast<std::size_t>(k)].level, true});
        }
    }
    return c;
}

// The curve of a connection: a cubic Bezier leaving the output to the right and entering the input from
// the left (also for feedback, which loops back); returns the four control points.
inline std::array<Point, 4> curve(Point a, Point b) noexcept {
    const float d = std::max(6.0f, std::abs(b.x - a.x) * 0.45f);
    return {{a, {a.x + d, a.y}, {b.x - d, b.y}, b}};
}

inline Point curveAt(const std::array<Point, 4>& c, float t) noexcept {
    const float u = 1 - t;
    const float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return {w0 * c[0].x + w1 * c[1].x + w2 * c[2].x + w3 * c[3].x, w0 * c[0].y + w1 * c[1].y + w2 * c[2].y + w3 * c[3].y};
}

inline std::string percent(float level) { return std::to_string(static_cast<int>(std::lround(level * 100.0f))) + " %"; }

// Hover text for an input: its sources in signal-flow order (source on the left, S6.1).
inline std::string describeInput(const lmw::dsp::Patch& p, In in) {
    const std::string name(lmw::dsp::inputNames[static_cast<std::size_t>(in)]);
    if (!p.isPatched(in)) {
        const auto ns = p.normals(in);
        if (ns.n == 0) return name + ": not connected";
        std::string s;
        for (int k = 0; k < ns.n; ++k) {
            const auto& x = ns.s[static_cast<std::size_t>(k)];
            if (k > 0) s += " + ";
            s += lmw::dsp::outputNames[static_cast<std::size_t>(x.source)];
            if (x.level < 1.0f) s += " (" + percent(x.level) + ")";
        }
        return s + " → " + name + (ns.n > 1 ? " (normalled, summed)" : " (normalled)");
    }
    std::string s;
    for (int k = 0; k < p.pinCount(in); ++k) {
        const auto& pin = p.pin(in, k);
        if (k > 0) s += " + ";
        s += lmw::dsp::outputNames[static_cast<std::size_t>(pin.source)];
        if (pin.level != 1.0f) s += " (" + percent(pin.level) + ")";   // S5.21: signed
    }
    return s + " → " + name + (p.pinCount(in) > 1 ? " (cables, summed)" : " (cable)");
}

// Hover text for an output: what it feeds.
inline std::string describeOutput(const lmw::dsp::Patch& p, Out out) {
    std::string s(lmw::dsp::outputNames[static_cast<std::size_t>(out)]);
    std::string fed;
    for (const auto& c : connections(p))
        if (c.from == out) {
            if (!fed.empty()) fed += ", ";
            fed += lmw::dsp::inputNames[static_cast<std::size_t>(c.to)];
            if (c.normalled) fed += " (normalled)";
            else if (c.level != 1.0f) fed += " (" + percent(c.level) + ")";
        }
    return s + " → " + (fed.empty() ? std::string("nothing") : fed);
}

// Internal links (set by a fader, not by a socket): drawn thin and dotted with an arrow into the module
// body. Key tracking keys.pitch -> VCF N only while its Key fader is above 0 (velocity -> VCA was one until
// S5.22 made it the normalled input vca.vel). The mod wheel has no block; the VCO hover text names it.
struct InternalLink {
    Out from;
    std::string_view block;          // target block name
    std::string label;               // shown at the arrow ("key 50 %", "velocity")
};

inline std::optional<std::size_t> findBlock(std::string_view name) {
    for (std::size_t i = 0; i < blocks().size(); ++i)
        if (blocks()[i].name == name) return i;
    return std::nullopt;
}

inline std::vector<InternalLink> internalLinks(float vcf1Key, float vcf2Key) {
    std::vector<InternalLink> l;
    if (vcf1Key > 0.0f) l.push_back({Out::KeysPitch, "VCF 1", "key " + percent(vcf1Key)});
    if (vcf2Key > 0.0f) l.push_back({Out::KeysPitch, "VCF 2", "key " + percent(vcf2Key)});
    return l;
}

// Where an internal link enters its block: from below, at a third of the block's width (for a triangle,
// on its lower edge there).
inline Point internalTarget(std::size_t block, const Layout& l = defaultLayout()) {
    const auto r = l.box(block);
    const float x = r.x + r.w * 0.3f;
    const float y = blocks()[block].shape == Shape::Triangle ? r.bottom() - r.h * 0.5f * 0.3f : r.bottom();
    return {x, y};
}

// The curve of an internal link: leaves the output to the right, enters the block from below.
inline std::array<Point, 4> internalCurve(Point a, Point t) noexcept {
    const float d = std::max(6.0f, std::abs(t.x - a.x) * 0.4f);
    return {{a, {a.x + d, a.y}, {t.x, t.y + std::max(8.0f, std::abs(t.y - a.y) * 0.6f)}, t}};
}

// Spec S5.24 (user 2026-11-25: show it, and say that OUT is not delayed): LFO Delay notes. `delays` are
// lfo1.delay and lfo2.delay in seconds. The Delay acts where a voice reads the LFO (every per-voice input);
// out.in and the LFO Rate inputs read the shared LFO once, so it is not delayed there.
using LfoDelays = std::array<float, 2>;
inline std::string lfoDelayLabel(float seconds) { return "delay " + lmw::ui::text::time(seconds); }
inline bool readsSharedLfo(In in) noexcept { return in == In::OutIn || in == In::Lfo1Rate || in == In::Lfo2Rate; }
// The note for an input fed (normalled or by cable) by an LFO whose Delay is above 0; empty if none.
inline std::string lfoDelayNote(const lmw::dsp::Patch& p, In in, const LfoDelays& delays) {
    std::string note;
    for (std::size_t k = 0; k < 2; ++k) {
        const Out lfo = k == 0 ? Out::Lfo1 : Out::Lfo2;
        if (!(delays[k] > 0.0f)) continue;
        bool feeds = false;
        if (p.isPatched(in)) feeds = p.hasPin(in, lfo);
        else { const auto ns = p.normals(in); for (int i = 0; i < ns.n; ++i) feeds = feeds || ns.s[static_cast<std::size_t>(i)].source == lfo; }
        if (!feeds) continue;
        const std::string name(lmw::dsp::outputNames[static_cast<std::size_t>(lfo)]);
        if (!note.empty()) note += "\n";
        note += readsSharedLfo(in)
            ? name + " is not delayed here: the LFO Delay works per note, inside the voices"
            : name + " delayed " + lmw::ui::text::time(delays[k]) + " per note (fades in after each note starts)";
    }
    return note;
}
// The note for an LFO output while its Delay is above 0; empty otherwise.
inline std::string lfoDelayOutputNote(Out out, const LfoDelays& delays) {
    const std::size_t k = out == Out::Lfo1 ? 0 : out == Out::Lfo2 ? 1 : 2;
    if (k > 1 || !(delays[k] > 0.0f)) return {};
    return "Delay " + lmw::ui::text::time(delays[k]) + ": fades in per note in the voices it feeds; "
           "not delayed at out.in or the LFO Rate inputs";
}

// Extra hover lines for a block: its internal links (and the LFO Delay, S5.24).
inline std::string blockNote(std::string_view name, float vcf1Key, float vcf2Key, const LfoDelays& delays = {}) {
    if (name == "LFO 1" || name == "LFO 2") return lfoDelayOutputNote(name == "LFO 1" ? Out::Lfo1 : Out::Lfo2, delays);
    if (name.starts_with("VCO")) return "mod wheel \u2192 FM depth (internal: FM fader + wheel \u00d7 1 st)";
    if (name == "VCF 1" || name == "VCF 2") {
        const float k = name == "VCF 1" ? vcf1Key : vcf2Key;
        return k > 0.0f ? "keys.pitch \u2192 cutoff (internal: Key " + percent(k) + ")"
                        : std::string("key tracking off (Key 0 %)");
    }
    return {};
}

// The port under a canvas point (within `radius`), for hover texts and editing.
// `wantInput`: only inputs (true), only outputs (false), or either (empty) — e.g. the drop target of a cable.
struct PortHit { bool input; int index; };
inline std::optional<PortHit> portAt(Point p, float radius, std::optional<bool> wantInput = std::nullopt,
                                     const Layout& l = defaultLayout()) {
    auto near = [&](Point q) { return std::hypot(q.x - p.x, q.y - p.y) <= radius; };
    if (wantInput.value_or(true))
        for (int i = 0; i < lmw::dsp::numInputs; ++i)
            if (near(inputPoint(static_cast<In>(i), l))) return PortHit{true, i};
    if (!wantInput.value_or(false))
        for (int o = 0; o < lmw::dsp::numOutputs; ++o)
            if (near(outputPoint(static_cast<Out>(o), l))) return PortHit{false, o};
    return std::nullopt;
}

// The block that holds a port (for moving it with the keyboard).
inline std::size_t blockOf(PortHit h) {
    for (std::size_t i = 0; i < blocks().size(); ++i) {
        const auto& b = blocks()[i];
        if (h.input && std::find(b.inputs.begin(), b.inputs.end(), static_cast<In>(h.index)) != b.inputs.end()) return i;
        if (!h.input && std::find(b.outputs.begin(), b.outputs.end(), static_cast<Out>(h.index)) != b.outputs.end()) return i;
    }
    return 0;
}

// Keyboard order of the ports (Phase 12b): block by block, each block's inputs then its outputs.
inline std::vector<PortHit> portOrder() {
    std::vector<PortHit> v;
    for (const auto& b : blocks()) {
        for (auto i : b.inputs) v.push_back({true, static_cast<int>(i)});
        for (auto o : b.outputs) v.push_back({false, static_cast<int>(o)});
    }
    return v;
}

// Up/Down on the Schematic tab (user 2026-11-16): from port `cursor` (index into portOrder()) to the
// first port of the next (dir > 0) or previous (dir < 0) block in the same order, wrapping around.
inline std::size_t blockJump(std::size_t cursor, int dir) {
    const auto order = portOrder();
    if (order.empty()) return 0;
    cursor = std::min(cursor, order.size() - 1);
    const auto nb = blocks().size();
    const auto here = blockOf(order[cursor]);
    const auto target = dir > 0 ? (here + 1) % nb : (here + nb - 1) % nb;
    for (std::size_t k = 0; k < order.size(); ++k)
        if (blockOf(order[k]) == target) return k;
    return cursor;
}

} // namespace lmw::ui::schematic
