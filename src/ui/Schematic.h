// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.7: the schematic view — geometry and connections, pure C++ (no JUCE) so it can be tested.
// Every module is a block on a fixed canvas (units, scaled to the window by the view), signal flowing
// left to right: inputs on a block's left edge, outputs on its right edge (a triangle's output at its tip).
// Connections come from the patch: an input's cables (pins with levels, S5.14), or its normalled source
// while it has none.
#pragma once
#include "dsp/Patch.h"
#include "ui/Theme.h"
#include <array>
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

inline constexpr float canvasWidth = 136.0f, canvasHeight = 72.0f;
inline constexpr float nameHeight = 3.5f;   // room above each block for its name

inline const std::vector<Block>& blocks() {
    static const std::vector<Block> b{
        {"LFO 1",  Shape::Triangle, {2, 6, 10, 9},    {}, {Out::Lfo1}},
        {"KEYS",   Shape::Keys,     {2, 23, 12, 9},   {}, {Out::KeysPitch, Out::KeysGate, Out::KeysVel}},
        {"VCO 1",  Shape::Triangle, {26, 4, 11, 10},  {In::Vco1Pitch, In::Vco1Fm}, {Out::Vco1}},
        {"VCO 2",  Shape::Triangle, {26, 18, 11, 10}, {In::Vco2Pitch, In::Vco2Fm}, {Out::Vco2}},
        {"VCO 3",  Shape::Triangle, {26, 32, 11, 10}, {In::Vco3Pitch, In::Vco3Fm}, {Out::Vco3}},
        {"NOISE",  Shape::Noise,    {26, 54, 11, 8},  {}, {Out::Noise, Out::Rand}},
        {"MIX",    Shape::Triangle, {48, 14, 12, 26}, {In::MixIn1, In::MixIn2, In::MixIn3, In::MixIn4}, {Out::Mix}},
        {"VCF 1",  Shape::Triangle, {68, 20, 12, 14}, {In::Vcf1In, In::Vcf1Cutoff, In::Vcf1Res}, {Out::Vcf1}},
        {"VCF 2",  Shape::Triangle, {88, 20, 12, 14}, {In::Vcf2In, In::Vcf2Cutoff, In::Vcf2Res}, {Out::Vcf2}},
        {"VCA",    Shape::Triangle, {108, 20, 12, 14},{In::VcaIn, In::VcaCv, In::VcaMod}, {Out::Vca}},
        {"OUT",    Shape::Out,      {126, 22, 8, 10}, {In::OutIn}, {}},
        {"ENV 1",  Shape::Adsr,     {50, 58, 12, 8},  {In::Env1Gate}, {Out::Env1}},
        {"ENV 2",  Shape::Adsr,     {50, 45, 12, 8},  {In::Env2Gate}, {Out::Env2}},
        {"LFO 2",  Shape::Triangle, {96, 50, 10, 9},  {}, {Out::Lfo2}},
    };
    return b;
}

namespace detail {
inline float slot(const Rect& r, std::size_t k, std::size_t n) noexcept {
    return r.y + r.h * static_cast<float>(k + 1) / static_cast<float>(n + 1);
}
}

// Where an input / output sits on the canvas.
inline Point inputPoint(In in) {
    for (const auto& b : blocks())
        for (std::size_t k = 0; k < b.inputs.size(); ++k)
            if (b.inputs[k] == in) return {b.box.x, detail::slot(b.box, k, b.inputs.size())};
    return {};
}

inline Point outputPoint(Out out) {
    for (const auto& b : blocks())
        for (std::size_t k = 0; k < b.outputs.size(); ++k)
            if (b.outputs[k] == out) {
                if (b.shape == Shape::Triangle) return {b.box.right(), b.box.y + b.box.h / 2};   // the tip
                return {b.box.right(), detail::slot(b.box, k, b.outputs.size())};
            }
    return {};
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
        } else if (const auto n = lmw::dsp::normalled[static_cast<std::size_t>(i)]; n != lmw::dsp::noSource) {
            c.push_back({n, in, 1.0f, true});
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
        const auto n = lmw::dsp::normalled[static_cast<std::size_t>(in)];
        if (n == lmw::dsp::noSource) return name + ": not connected";
        return std::string(lmw::dsp::outputNames[static_cast<std::size_t>(n)]) + " → " + name + " (normalled)";
    }
    std::string s;
    for (int k = 0; k < p.pinCount(in); ++k) {
        const auto& pin = p.pin(in, k);
        if (k > 0) s += " + ";
        s += lmw::dsp::outputNames[static_cast<std::size_t>(pin.source)];
        if (pin.level < 1.0f) s += " (" + percent(pin.level) + ")";
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
            else if (c.level < 1.0f) fed += " (" + percent(c.level) + ")";
        }
    return s + " → " + (fed.empty() ? std::string("nothing") : fed);
}

// The port under a canvas point (within `radius`), for hover texts.
struct PortHit { bool input; int index; };
inline std::optional<PortHit> portAt(Point p, float radius) {
    auto near = [&](Point q) { return std::hypot(q.x - p.x, q.y - p.y) <= radius; };
    for (int i = 0; i < lmw::dsp::numInputs; ++i)
        if (near(inputPoint(static_cast<In>(i)))) return PortHit{true, i};
    for (int o = 0; o < lmw::dsp::numOutputs; ++o)
        if (near(outputPoint(static_cast<Out>(o)))) return PortHit{false, o};
    return std::nullopt;
}

} // namespace lmw::ui::schematic
