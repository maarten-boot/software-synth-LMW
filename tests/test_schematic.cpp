// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.7-schematic: schematic geometry and connections (Spec S6.7, Phase 12a).
#include "ui/Schematic.h"
#include <cstdio>
#include <set>

namespace sc = lmw::ui::schematic;
using lmw::dsp::In;
using lmw::dsp::Out;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    const auto& blocks = sc::blocks();
    {   // Every port belongs to exactly one block; every block lies inside the canvas with room for its name;
        // blocks (with their names) do not overlap.
        std::multiset<int> ins, outs;
        for (const auto& b : blocks) {
            for (auto i : b.inputs) ins.insert(static_cast<int>(i));
            for (auto o : b.outputs) outs.insert(static_cast<int>(o));
            CHECK(b.box.x >= 0 && b.box.right() <= sc::canvasWidth);
            CHECK(b.box.y - sc::nameHeight >= 0 && b.box.bottom() <= sc::canvasHeight - 4);   // legend below
        }
        for (int i = 0; i < lmw::dsp::numInputs; ++i) CHECK(ins.count(i) == 1);
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) CHECK(outs.count(o) == 1);
        for (std::size_t a = 0; a < blocks.size(); ++a)
            for (std::size_t b = a + 1; b < blocks.size(); ++b) {
                auto ra = blocks[a].box, rb = blocks[b].box;
                ra.y -= sc::nameHeight; ra.h += sc::nameHeight;
                rb.y -= sc::nameHeight; rb.h += sc::nameHeight;
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
            const auto* b = sc::findBlock(x.block);
            CHECK(b != nullptr);
            if (b == nullptr) continue;
            const auto t = sc::internalTarget(*b);
            CHECK(t.x > b->box.x && t.x < b->box.right() && t.y > b->box.y + b->box.h / 2 && t.y <= b->box.bottom());
            CHECK(!sc::portAt(t, 1.0f));                   // not on a socket
            const auto cv = sc::internalCurve(sc::outputPoint(x.from), t);
            CHECK(cv[2].y > t.y);                           // enters from below
        }
        CHECK(sc::findBlock("nothing") == nullptr);
        CHECK(sc::blockNote("VCF 1", 0.5f, 0.0f) == "keys.pitch \u2192 cutoff (internal: Key 50 %)");
        CHECK(sc::blockNote("VCF 2", 0.5f, 0.0f) == "key tracking off (Key 0 %)");
        CHECK(sc::blockNote("VCA", 0, 0).find("velocity") != std::string::npos);
        CHECK(sc::blockNote("VCO 2", 0, 0).find("mod wheel") != std::string::npos);
        CHECK(sc::blockNote("MIX", 0, 0).empty());
    }
    CHECK(sc::portLabel("vcf1.cutoff") == "cutoff" && sc::portLabel("keys.gate") == "gate");
    CHECK(sc::blocks()[0].unit() == lmw::ui::theme::Unit::Lfo);

    std::printf(failures == 0 ? "test_schematic: all passed\n" : "test_schematic: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
