// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.2 / T-S6.3: contrast utilities.
#include "ui/Contrast.h"
#include <cmath>
#include <cstdio>

using namespace lmw::ui;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static bool near(double a, double b, double eps = 0.01) { return std::fabs(a - b) < eps; }

int main() {
    const Rgb black{0, 0, 0}, white{255, 255, 255};
    CHECK(near(contrastRatio(black, white), 21.0));
    CHECK(near(contrastRatio(white, black), 21.0));
    CHECK(near(contrastRatio(white, white), 1.0));
    CHECK(near(contrastRatio(Rgb{0x77, 0x77, 0x77}, white), 4.48));   // known WCAG value
    CHECK(contrastRatio(Rgb{0x76, 0x76, 0x76}, white) >= 4.5);

    CHECK(minimumRatio(ContrastPreset::AA) == 4.5);
    CHECK(minimumRatio(ContrastPreset::AAA) == 7.0);
    CHECK(minimumRatio(ContrastPreset::Max) == 21.0);
    CHECK(minimumRatio(ContrastPreset::Custom, 10.0) == 10.0);
    CHECK(minimumRatio(ContrastPreset::Custom, 99.0) == 21.0);
    CHECK(parsePreset("aaa") == ContrastPreset::AAA);
    CHECK(!parsePreset("xyz"));

    // Enforcement: each preset met for a weak grey on white and on a dark bg.
    for (double r : {4.5, 7.0, 12.0, 21.0}) {
        CHECK(contrastRatio(enforceContrast(Rgb{0xAA, 0xAA, 0xAA}, white, r), white) >= r);
        CHECK(contrastRatio(enforceContrast(Rgb{0x40, 0x40, 0x60}, Rgb{0x20, 0x20, 0x20}, std::min(r, 15.0)),
                            Rgb{0x20, 0x20, 0x20}) >= std::min(r, 15.0));
    }
    // Default theme (S6.2): black on white passes AA unchanged.
    const Rgb t = enforceContrast(black, white, 4.5);
    CHECK(t.r == 0 && t.g == 0 && t.b == 0);

    std::printf(failures ? "%d failure(s)\n" : "all contrast tests passed\n", failures);
    return failures ? 1 : 0;
}
