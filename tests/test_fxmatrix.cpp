// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.25-fxmatrix (unit): the rows and columns of the Matrix tab (synth) and the effects matrix (Effects tab):
// 39 x 19 (34 before S5.28) and 5 x 5, no cell where the ports cannot connect (S7.6), together every connectable pair exactly once,
// and the effects matrix in the effects order (S7.14): row r's normalled source is column r.
#include "dsp/Patch.h"
#include <cstdio>
#include <set>
#include <utility>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    const auto si = synthMatrixInputs(), so = synthMatrixOutputs();
    std::printf("Matrix tab %zu x %zu\n", si.size(), so.size());
    CHECK(si.size() == 39 && so.size() == 19);                       // 39 since S5.28 (the Shape inputs)
    CHECK(si.front() == static_cast<int>(In::Vco1Pitch) && si.back() == static_cast<int>(In::OutPan));
    CHECK(so.front() == static_cast<int>(Out::KeysPitch) && so.back() == static_cast<int>(Out::Vca));
    for (int order = 0; order < numEffectOrders; ++order) {
        const auto ei = effectsMatrixInputs(order), eo = effectsMatrixOutputs(order);
        CHECK(ei.size() == 5 && eo.size() == 5);
        CHECK(ei.back() == static_cast<int>(In::MainIn) && eo.front() == static_cast<int>(Out::OutOut));
        // Every connectable pair in exactly one of the two matrices; no crossed-out cell in either.
        std::set<std::pair<int, int>> cells;
        int crossed = 0;
        for (const auto& [ins, outs] : {std::pair{si, so}, std::pair{ei, eo}})
            for (const int i : ins) for (const int o : outs) {
                if (!canConnect(static_cast<In>(i), static_cast<Out>(o))) ++crossed;
                CHECK(cells.insert({i, o}).second);
            }
        int connectable = 0;
        for (int i = 0; i < numInputs; ++i) for (int o = 0; o < numOutputs; ++o)
            connectable += canConnect(static_cast<In>(i), static_cast<Out>(o)) ? 1 : 0;
        CHECK(crossed == 0 && static_cast<int>(cells.size()) == connectable);
        // In the chain's order: the normalled source of row r is column r (the diagonal).
        for (std::size_t r = 0; r < ei.size(); ++r)
            CHECK(static_cast<int>(effectNormal(static_cast<In>(ei[r]), order)) == eo[r]);
    }
    const auto rev = effectsMatrixInputs(23);
    std::printf("effects matrix 5 x 5; order 23 rows: %s ... %s\n", inputNames[static_cast<std::size_t>(rev.front())].data(),
                inputNames[static_cast<std::size_t>(rev.back())].data());
    CHECK(rev.front() == static_cast<int>(In::ReverbIn));
    if (failures == 0) std::printf("T-S6.25-fxmatrix: all passed\n");
    return failures == 0 ? 0 : 1;
}
