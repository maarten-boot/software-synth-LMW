// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.14-history: the undo / redo history (src/ui/History.h, Spec S6.14).
#include "ui/History.h"
#include <cstdio>
#include <string>

using lmw::ui::History;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    History::State s{{"p:vcf1.cutoff", "0.5"}, {"p:vco1.wave", "0.25"}, {"schematic", ""}};
    const auto step = [&](History& h, const std::string& name, const std::string& key, const std::string& value) {
        h.begin(name, s);
        if (value.empty()) s.erase(key); else s[key] = value;
        return h.end(s);
    };
    {   // a step keeps only what changed, before and after; undo / redo order
        History h;
        CHECK(!h.canUndo() && !h.canRedo() && h.undoName().empty() && !h.undo());
        CHECK(step(h, "VCF 1 Cutoff", "p:vcf1.cutoff", "0.3"));
        CHECK(step(h, "cables", "c:vcf1.cutoff", "lfo2.out@0.5"));       // a new key: before is ""
        CHECK(h.size() == 2 && h.undoName() == "cables" && !h.canRedo());
        auto u = h.undo();
        CHECK(u && u->name == "cables" && u->before.size() == 1 && u->before.at("c:vcf1.cutoff").empty()
              && u->after.at("c:vcf1.cutoff") == "lfo2.out@0.5");
        u = h.undo();
        CHECK(u && u->name == "VCF 1 Cutoff" && u->before.size() == 1 && u->before.at("p:vcf1.cutoff") == "0.5"
              && u->after.at("p:vcf1.cutoff") == "0.3");
        CHECK(!h.canUndo() && h.redoName() == "VCF 1 Cutoff" && !h.undo());
        auto r = h.redo();
        CHECK(r && r->name == "VCF 1 Cutoff" && h.undoName() == "VCF 1 Cutoff" && h.redoName() == "cables");
        CHECK(h.lines() == "> VCF 1 Cutoff\n  cables\n");
        // a new step drops the redo step
        CHECK(step(h, "VCO 1 Wave", "p:vco1.wave", "0.5"));
        CHECK(h.size() == 2 && !h.canRedo() && h.lines() == "  VCF 1 Cutoff\n> VCO 1 Wave\n");
        // a removed key: after is ""
        s.erase("c:vcf1.cutoff");
        CHECK(step(h, "cables", "c:vcf1.cutoff", "") == false);          // it was not there: nothing changed
        s["c:lfo1.rate"] = "env2.out";
        CHECK(step(h, "cables", "c:lfo1.rate", ""));
        u = h.undo();
        CHECK(u && u->before.at("c:lfo1.rate") == "env2.out" && u->after.at("c:lfo1.rate").empty());
        h.clear();
        CHECK(h.size() == 0 && !h.canUndo() && h.lines().empty());
    }
    {   // nothing changed: no step; a change and its reverse in one step: no step
        History h;
        h.begin("touch", s);
        CHECK(!h.end(s) && h.size() == 0);
        h.begin("there and back", s);
        const auto old = s["p:vcf1.cutoff"];
        s["p:vcf1.cutoff"] = "0.9";
        s["p:vcf1.cutoff"] = old;
        CHECK(!h.end(s));
    }
    {   // nesting: only the outermost begin / end makes a step, with everything inside it
        History h;
        h.begin("Assistant: make it darker", s);
        CHECK(h.recording() && !h.canUndo());
        CHECK(!step(h, "VCF 1 Cutoff", "p:vcf1.cutoff", "0.1"));          // inner: no step of its own
        CHECK(!step(h, "cables", "c:vcf1.res", "lfo1.out"));
        CHECK(h.end(s) && h.size() == 1 && !h.recording());
        const auto u = h.undo();
        CHECK(u && u->name == "Assistant: make it darker" && u->before.size() == 2);
        CHECK(!h.end(s));                                                 // an end without a begin does nothing
    }
    {   // at most `depth` steps: the oldest go
        History h(3);
        for (int i = 0; i < 5; ++i) CHECK(step(h, "step " + std::to_string(i), "p:x", std::to_string(i)));
        CHECK(h.size() == 3 && h.undoName() == "step 4");
        int n = 0;
        while (h.undo()) ++n;
        CHECK(n == 3 && h.redoName() == "step 2");
    }
    if (failures == 0) std::printf("ok\n");
    else std::printf("%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
