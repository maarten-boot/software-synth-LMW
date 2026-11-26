// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.14-order (unit): the 24 effect orders, their normalled chain, the /effects text and the Schematic places.
#include "dsp/Patch.h"
#include "ui/Schematic.h"
#include <cstdio>
#include <set>

using namespace lmw::dsp;
namespace sc = lmw::ui::schematic;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    // 0 = the order before S7.14; 23 = reversed; all 24 different and each its own index.
    CHECK((effectOrder(0) == EffectOrder{0, 1, 2, 3}));
    CHECK((effectOrder(23) == EffectOrder{3, 2, 1, 0}));
    std::set<EffectOrder> seen;
    for (int i = 0; i < numEffectOrders; ++i) {
        seen.insert(effectOrder(i));
        CHECK(effectOrderIndex(effectOrder(i)) == i);
    }
    CHECK(seen.size() == 24);
    // Default order: exactly the normalled table of before.
    for (int i = static_cast<int>(In::CombIn); i < numInputs; ++i)
        CHECK(normals(static_cast<In>(i), VcfRouting::Serial).s[0].source == normalled[static_cast<std::size_t>(i)]);
    // Every order: one chain out.out -> ... -> main.in through all four.
    for (int i = 0; i < numEffectOrders; ++i) {
        Out at = Out::OutOut;
        for (const int e : effectOrder(i)) {
            CHECK(normals(effectIn(e), VcfRouting::Serial, i).s[0].source == at);
            at = effectOut(e);
        }
        CHECK(normals(In::MainIn, VcfRouting::Serial, i).s[0].source == at);
        CHECK(normals(In::Vcf1In, VcfRouting::Serial, i).s[0].source == Out::Mix);   // synth inputs untouched
    }
    Patch p;
    p.setEffectOrder(effectOrderIndex({3, 0, 1, 2}));
    CHECK(p.normalLevel(In::ReverbIn, Out::OutOut) == 1.0f && p.normalLevel(In::CombIn, Out::ReverbOut) == 1.0f
          && p.normalLevel(In::MainIn, Out::DelayOut) == 1.0f && p.normalLevel(In::MainIn, Out::ReverbOut) == 0.0f);
    // The /effects text.
    CHECK(parseEffectOrder("comb ensemble delay reverb") == 0);
    CHECK(parseEffectOrder("Reverb, DELAY,ensemble , comb\n") == 23);
    CHECK(parseEffectOrder("comb comb delay reverb") == -1);
    CHECK(parseEffectOrder("comb ensemble delay") == -1);
    CHECK(parseEffectOrder("comb ensemble delay reverb chorus") == -1);
    CHECK(parseEffectOrder("") == -1);
    // Schematic: the four blocks take each other's places, left to right in the new order; back again = default.
    {
        sc::Layout l;
        const auto comb = sc::Layout::indexOf("COMB"), ens = sc::Layout::indexOf("ENSEMBLE"),
                   dly = sc::Layout::indexOf("DELAY"), rev = sc::Layout::indexOf("REVERB");
        CHECK(rev < sc::blocks().size());
        const auto combAt = l.position(comb), revAt = l.position(rev);
        l.arrange({rev, comb, ens, dly});
        CHECK(l.position(rev).x == combAt.x && l.position(dly).x == revAt.x && l.position(comb).x < l.position(ens).x);
        for (std::size_t i = 0; i < sc::blocks().size(); ++i) CHECK(l.fits(i, l.position(i)));
        l.arrange({comb, ens, dly, rev});
        CHECK(l.isDefault());
    }
    if (failures == 0) std::printf("T-S7.14-order: all passed\n");
    return failures == 0 ? 0 : 1;
}
