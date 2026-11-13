// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.7-schematic: schematic geometry and connections (Spec S6.7, Phase 12a).
#include "ui/Schematic.h"
#include "ui/CableEdit.h"
#include <cstdio>
#include <set>

namespace sc = lmw::ui::schematic;
using lmw::dsp::In;
using lmw::dsp::Out;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    const auto& blocks = sc::blocks();
    {   // Every port belongs to exactly one block; every block lies inside the canvas; blocks do not overlap.
        std::multiset<int> ins, outs;
        for (const auto& b : blocks) {
            for (auto i : b.inputs) ins.insert(static_cast<int>(i));
            for (auto o : b.outputs) outs.insert(static_cast<int>(o));
            CHECK(b.box.x >= 0 && b.box.right() <= sc::canvasWidth);
            CHECK(b.box.y >= 0 && b.box.bottom() <= sc::canvasHeight - 4);   // legend below
        }
        for (int i = 0; i < lmw::dsp::numInputs; ++i) CHECK(ins.count(i) == 1);
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) CHECK(outs.count(o) == 1);
        for (std::size_t a = 0; a < blocks.size(); ++a)
            for (std::size_t b = a + 1; b < blocks.size(); ++b) {
                auto ra = blocks[a].box, rb = blocks[b].box;
                if (ra.intersects(rb)) std::printf("overlap: %s / %s\n", blocks[a].name.data(), blocks[b].name.data());
                CHECK(!ra.intersects(rb));
            }
        CHECK(blocks.size() == 14);
    }
    {   // Ports: inputs on the left edge, outputs on the right edge (a triangle's output at its tip); distinct.
        for (const auto& b : blocks) {
            for (auto i : b.inputs) {
                const auto p = sc::inputPoint(i);
                CHECK(p.x == b.box.x && p.y > b.box.y && p.y < b.box.bottom());
            }
            for (auto o : b.outputs) {
                const auto p = sc::outputPoint(o);
                CHECK(p.x == b.box.right() && p.y > b.box.y && p.y < b.box.bottom());
                if (b.shape == sc::Shape::Triangle) CHECK(p.y == b.box.y + b.box.h / 2);
            }
        }
        // Shapes as agreed: triangles for VCO/VCF/VCA/LFO/MIX, ADSR for ENV, keyboard, noise, out circle.
        for (const auto& b : blocks) {
            const std::string_view n = b.name;
            const auto want = n.starts_with("ENV") ? sc::Shape::Adsr : n == "KEYS" ? sc::Shape::Keys
                            : n == "NOISE" ? sc::Shape::Noise : n == "OUT" ? sc::Shape::Out : sc::Shape::Triangle;
            CHECK(b.shape == want);
        }
        // Hit test finds each port at its own place.
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto h = sc::portAt(sc::inputPoint(static_cast<In>(i)), 0.5f);
            CHECK(h && h->input && h->index == i);
        }
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) {
            const auto h = sc::portAt(sc::outputPoint(static_cast<Out>(o)), 0.5f);
            CHECK(h && !h->input && h->index == o);
        }
        CHECK(!sc::portAt({1, 60}, 0.5f));
    }
    {   // Default patch: the 20 normalled connections (22 inputs, vcf1.res and vcf2.res have none).
        lmw::dsp::Patch p;
        const auto c = sc::connections(p);
        CHECK(c.size() == 20);
        for (const auto& x : c) CHECK(x.normalled && x.level == 1.0f);
        CHECK(sc::describeInput(p, In::Vcf1Cutoff) == "env2.out → vcf1.cutoff (normalled)");
        CHECK(sc::describeInput(p, In::Vcf1Res) == "vcf1.res: not connected");
        CHECK(sc::describeOutput(p, Out::KeysPitch) == "keys.pitch → vco1.pitch (normalled), vco2.pitch (normalled), vco3.pitch (normalled)");
        CHECK(sc::describeOutput(p, Out::Rand) == "noise.rand → nothing");
    }
    {   // Cables replace the normalled line of their input; several pins each give a line with their level.
        lmw::dsp::Patch p;
        CHECK(p.connect(In::Vcf1Cutoff, Out::Lfo2, 0.5f));
        CHECK(p.connect(In::Vcf1Cutoff, Out::Env2));
        CHECK(p.connect(In::Vcf1Res, Out::KeysVel));
        const auto c = sc::connections(p);
        CHECK(c.size() == 22);   // 19 normalled + 3 cables
        int cables = 0;
        for (const auto& x : c)
            if (!x.normalled) {
                ++cables;
                CHECK(x.to == In::Vcf1Cutoff || x.to == In::Vcf1Res);
                if (x.from == Out::Lfo2) CHECK(x.level == 0.5f);
            } else {
                CHECK(x.to != In::Vcf1Cutoff);
            }
        CHECK(cables == 3);
        CHECK(sc::describeInput(p, In::Vcf1Cutoff) == "lfo2.out (50 %) + env2.out → vcf1.cutoff (cables, summed)");
        CHECK(sc::describeInput(p, In::Vcf1Res) == "keys.vel → vcf1.res (cable)");
        CHECK(sc::describeOutput(p, Out::Lfo2) == "lfo2.out → vcf1.cutoff (50 %), vca.mod (normalled)");
    }
    {   // Curves leave to the right and enter from the left, also for feedback (vcf2.out -> vcf1.in).
        const auto cv = sc::curve(sc::outputPoint(Out::Vcf2), sc::inputPoint(In::Vcf1In));
        CHECK(cv[1].x > cv[0].x && cv[2].x < cv[3].x);
        const auto mid = sc::curveAt(cv, 0.5f);
        CHECK(std::isfinite(mid.x) && std::isfinite(mid.y));
        CHECK(sc::curveAt(cv, 0).x == cv[0].x && sc::curveAt(cv, 1).y == cv[3].y);
    }
    {   // Internal links: key tracking only while Key > 0, velocity -> VCA always; targets on the block's lower edge.
        auto l = sc::internalLinks(0.0f, 0.0f);
        CHECK(l.size() == 1 && l[0].from == Out::KeysVel && l[0].block == "VCA" && l[0].label == "velocity");
        l = sc::internalLinks(0.5f, 0.0f);
        CHECK(l.size() == 2 && l[0].from == Out::KeysPitch && l[0].block == "VCF 1" && l[0].label == "key 50 %");
        l = sc::internalLinks(0.25f, 1.0f);
        CHECK(l.size() == 3 && l[1].block == "VCF 2" && l[1].label == "key 100 %");
        for (const auto& x : l) {
            const auto bi = sc::findBlock(x.block);
            CHECK(bi.has_value());
            if (!bi) continue;
            const auto box = sc::defaultLayout().box(*bi);
            const auto t = sc::internalTarget(*bi);
            CHECK(t.x > box.x && t.x < box.right() && t.y > box.y + box.h / 2 && t.y <= box.bottom());
            CHECK(!sc::portAt(t, 1.0f));                   // not on a socket
            const auto cv = sc::internalCurve(sc::outputPoint(x.from), t);
            CHECK(cv[2].y > t.y);                           // enters from below
        }
        CHECK(!sc::findBlock("nothing"));
        CHECK(sc::blockNote("VCF 1", 0.5f, 0.0f) == "keys.pitch \u2192 cutoff (internal: Key 50 %)");
        CHECK(sc::blockNote("VCF 2", 0.5f, 0.0f) == "key tracking off (Key 0 %)");
        CHECK(sc::blockNote("VCA", 0, 0).find("velocity") != std::string::npos);
        CHECK(sc::blockNote("VCO 2", 0, 0).find("mod wheel") != std::string::npos);
        CHECK(sc::blockNote("MIX", 0, 0).empty());
    }
    {   // Phase 12b: drag rules shared with the Synth tab (CableEdit.h).
        namespace ce = lmw::ui::cables;
        using V = std::vector<ce::Op>;
        const int lfo2 = static_cast<int>(Out::Lfo2), env2 = static_cast<int>(Out::Env2), vel = static_cast<int>(Out::KeysVel);
        const int cut = static_cast<int>(In::Vcf1Cutoff), res = static_cast<int>(In::Vcf1Res), mod = static_cast<int>(In::VcaMod);
        lmw::dsp::Patch p;
        // From an output: a new cable to the input it is dropped on; nothing when dropped elsewhere or on an output.
        auto g = ce::grab(p, {true, lfo2});
        CHECK(!g.moving() && g.fixed == (ce::PortRef{true, lfo2}));
        CHECK(ce::drop(g, ce::PortRef{false, cut}) == (V{{cut, lfo2, 1.0f}}));
        CHECK(ce::drop(g, std::nullopt).empty());
        CHECK(ce::drop(g, ce::PortRef{true, env2}).empty());
        // From an empty input: a new cable from the output it is dropped on.
        g = ce::grab(p, {false, res});
        CHECK(!g.moving() && g.fixed == (ce::PortRef{false, res}));
        CHECK(ce::drop(g, ce::PortRef{true, vel}) == (V{{res, vel, 1.0f}}));
        CHECK(ce::drop(g, ce::PortRef{false, cut}).empty());
        // From a plugged input: picks up the newest cable; moved with its level, or removed, or put back.
        CHECK(p.connect(In::Vcf1Cutoff, Out::Env2));
        CHECK(p.connect(In::Vcf1Cutoff, Out::Lfo2, 0.5f));
        g = ce::grab(p, {false, cut});
        CHECK(g.moving() && g.detachedInput == cut && g.detachedSource == lfo2 && g.detachedLevel == 0.5f);
        CHECK(g.fixed == (ce::PortRef{true, lfo2}));
        CHECK(ce::drop(g, ce::PortRef{false, mod}) == (V{{mod, lfo2, 0.5f}, {cut, lfo2, 0.0f}}));
        CHECK(ce::drop(g, std::nullopt) == (V{{cut, lfo2, 0.0f}}));
        CHECK(ce::drop(g, ce::PortRef{false, cut}).empty());
        CHECK(ce::drop(g, ce::PortRef{true, env2}) == (V{{cut, lfo2, 0.0f}}));   // an output is no target: removed
        // Port order for the keyboard: every port once, block by block, inputs before outputs.
        const auto order = sc::portOrder();
        CHECK(static_cast<int>(order.size()) == lmw::dsp::numInputs + lmw::dsp::numOutputs);
        CHECK(order.front().input == false && order.front().index == static_cast<int>(Out::Lfo1));   // LFO 1 first
        std::set<std::pair<bool, int>> seen;
        for (const auto& h : order) seen.insert({h.input, h.index});
        CHECK(seen.size() == order.size());
        // Filtered hit test: only the wanted kind.
        CHECK(!sc::portAt(sc::inputPoint(In::Vcf1In), 0.5f, false));
        CHECK(sc::portAt(sc::inputPoint(In::Vcf1In), 0.5f, true).has_value());
        CHECK(!sc::portAt(sc::outputPoint(Out::Vca), 0.5f, true));
    }
    {   // Phase 12c: movable blocks.
        sc::Layout l;
        CHECK(l.isDefault());
        for (std::size_t i = 0; i < sc::blocks().size(); ++i) CHECK(l.fits(i, l.position(i)));   // default is valid
        const auto vcf2 = *sc::findBlock("VCF 2"), lfo2 = *sc::findBlock("LFO 2"), vcf1 = *sc::findBlock("VCF 1");
        // A free spot: moves (snapped to the grid); ports and internal targets follow.
        CHECK(l.move(lfo2, {110.4f, 55.6f}));
        CHECK(l.position(lfo2).x == 110 && l.position(lfo2).y == 56 && !l.isDefault());
        CHECK(sc::outputPoint(Out::Lfo2, l).x == 120 && sc::outputPoint(Out::Lfo2, l).y == 56 + 4.5f);
        CHECK(sc::outputPoint(Out::Lfo2).x == 106);                                    // default unchanged
        CHECK(sc::portAt(sc::outputPoint(Out::Lfo2, l), 0.5f, false, l).has_value());
        CHECK(!sc::portAt(sc::outputPoint(Out::Lfo2), 0.5f, false, l));
        CHECK(sc::blockAt({112, 60}, l) == lfo2 && !sc::blockAt({112, 60}));
        const auto t0 = sc::internalTarget(vcf2), t1 = (l.move(vcf2, {88, 21}), sc::internalTarget(vcf2, l));
        CHECK(t1.y == t0.y + 1 && t1.x == t0.x);
        // Refused: on another block, off the canvas, into the legend; nothing changes.
        const auto before = l.serialize();
        CHECK(!l.move(vcf2, sc::defaultLayout().position(vcf1)));
        CHECK(!l.move(vcf2, {79, 20}));                          // overlaps VCF 1 (ends at 80)
        CHECK(l.fits(vcf2, {80, 20}));                           // touching is allowed
        CHECK(!l.move(vcf2, {130, 20}) && !l.move(vcf2, {88, -1}) && !l.move(vcf2, {88, 60}));
        CHECK(l.serialize() == before);
        // Text form: every block; round trip; partial text keeps the others at their default place.
        CHECK(sc::Layout().serialize().starts_with("lfo1:2,6;keys:2,23;vco1:26,4;"));
        const auto back = sc::Layout::parse(before);
        CHECK(back && *back == l);
        const auto partial = sc::Layout::parse(" lfo2 : 110 , 56 ");
        CHECK(partial && partial->position(lfo2).x == 110 && partial->position(vcf1).x == 68);
        CHECK(sc::Layout::parse("") && sc::Layout::parse("")->isDefault());
        CHECK(!sc::Layout::parse("xyz:1,2"));                   // unknown block
        CHECK(!sc::Layout::parse("lfo2:110"));                  // malformed
        CHECK(!sc::Layout::parse("lfo2:-5,3"));                 // negative
        CHECK(!sc::Layout::parse("vcf2:68,20"));                // on top of VCF 1
        CHECK(sc::blockId("ENV 2") == "env2");
        // Keyboard: the block that holds a port.
        CHECK(sc::blockOf({true, static_cast<int>(In::Vcf2Cutoff)}) == vcf2);
        CHECK(sc::blockOf({false, static_cast<int>(Out::Lfo2)}) == lfo2);
    }
    CHECK(sc::portLabel("vcf1.cutoff") == "cutoff" && sc::portLabel("keys.gate") == "gate");
    CHECK(sc::blocks()[0].unit() == lmw::ui::theme::Unit::Lfo);

    std::printf(failures == 0 ? "test_schematic: all passed\n" : "test_schematic: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
