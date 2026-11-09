// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.2 / T-S6.3: contrast utilities.
#include "ui/Contrast.h"
#include "ui/Theme.h"
#include "ui/Config.h"
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
    CHECK(minimumRatio(ContrastPreset::Custom, 2.0) == 4.5);       // contrast can only be raised
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

    // T-S6.3a: every pair the GUI draws meets the chosen minimum, for every preset and custom ratios.
    for (double r : {4.5, 5.0, 7.0, 9.3, 12.0, 15.0, 21.0}) {
        const auto pal = lmw::ui::theme::palette(r);
        for (const auto& [fg, bg] : pal.pairs()) CHECK(contrastRatio(fg, bg) >= r - 1e-9);
    }
    // AA keeps the design grey; Max inverts pressed keys (black white key, white black key).
    CHECK(lmw::ui::theme::palette(4.5).whiteKeyDown.r == 0x76);
    const auto maxPal = lmw::ui::theme::palette(21.0);
    CHECK(maxPal.whiteKeyDown.r == 0 && maxPal.blackKeyDown.r == 255);

    // T-S6.3b: config file.
    {
        const auto s = parseConfig("# comment\ncontrast = aaa\ncontrast_ratio = 9.5  # ignored for AAA\ncompact=yes\nfoo = bar\n");
        CHECK(s.contrast == ContrastPreset::AAA && near(s.customRatio, 9.5) && s.compact == true);
        CHECK(near(s.minRatio(), 7.0));
        const auto c = parseConfig("contrast = Custom\ncontrast_ratio = 12");
        CHECK(near(c.minRatio(), 12.0) && !c.compact.has_value());
        const auto bad = parseConfig("contrast = purple\ncontrast_ratio = x\ncompact = maybe\n= 3\nnonsense");
        CHECK(bad.contrast == ContrastPreset::AA && near(bad.customRatio, 7.0) && !bad.compact);
        CHECK(near(parseConfig("contrast=custom\ncontrast_ratio=2").minRatio(), 4.5));   // floor AA
        const auto round = parseConfig(writeConfig(c));
        CHECK(round.contrast == c.contrast && near(round.customRatio, c.customRatio) && round.compact == c.compact);
        Settings w; w.contrast = ContrastPreset::Max; w.compact = false;
        const auto r2 = parseConfig(writeConfig(w));
        CHECK(r2.contrast == ContrastPreset::Max && r2.compact == false);
        // Spec S10.4: 9P over TCP, off by default.
        CHECK(!parseConfig("").tcpEndpoint());
        const auto tcp = parseConfig("ninep_tcp = 5640").tcpEndpoint();
        CHECK(tcp && tcp->first == "127.0.0.1" && tcp->second == 5640);
        const auto tcp2 = parseConfig("ninep_tcp = 0.0.0.0:7000").tcpEndpoint();
        CHECK(tcp2 && tcp2->first == "0.0.0.0" && tcp2->second == 7000);
        CHECK(!parseConfig("ninep_tcp = 70000").tcpEndpoint() && !parseConfig("ninep_tcp = x:y").tcpEndpoint());
        Settings withTcp; withTcp.ninepTcp = "5641";
        CHECK(parseConfig(writeConfig(withTcp)).ninepTcp == "5641");
    }

    std::printf(failures ? "%d failure(s)\n" : "all contrast tests passed\n", failures);
    return failures ? 1 : 0;
}
