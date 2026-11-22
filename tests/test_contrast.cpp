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
    {   // Spec S6.9: unit colours — design tints kept at AA and AAA, lightened above, white at Max.
        using lmw::ui::theme::Unit;
        using lmw::ui::theme::unitOf;
        CHECK(unitOf("VCO 1") == Unit::Vco && unitOf("vco3.out") == Unit::Vco && unitOf("NOISE") == Unit::Noise
              && unitOf("noise.rand") == Unit::Noise && unitOf("VCF 2") == Unit::Vcf && unitOf("vcf1") == Unit::Vcf
              && unitOf("VCA") == Unit::Vca && unitOf("ENV 1 \xe2\x86\x92 VCA") == Unit::Env && unitOf("env2.gate") == Unit::Env
              && unitOf("LFO 2") == Unit::Lfo && unitOf("MIXER") == Unit::Mix && unitOf("mix.in1") == Unit::Mix && unitOf("keys") == Unit::None
              && unitOf("OUT") == Unit::None && unitOf("REVERB") == Unit::Effect && unitOf("delay.in") == Unit::Effect && unitOf("COMB") == Unit::Vcf && unitOf("SYNTH") == Unit::None && unitOf("main.in") == Unit::None && unitOf("") == Unit::None);
        for (double r : {4.5, 7.0}) {
            const auto p = lmw::ui::theme::palette(r);
            for (std::size_t i = 0; i < p.unit.size(); ++i) {
                const auto a = p.unit[i], d = lmw::ui::theme::unitDesign[i];
                CHECK(a.r == d.r && a.g == d.g && a.b == d.b);
            }
        }
        std::printf("unit colours, black text:");
        for (const auto& c : lmw::ui::theme::unitDesign) std::printf(" %.1f", contrastRatio({0, 0, 0}, c));
        std::printf("\n");
        const auto p12 = lmw::ui::theme::palette(12.0);
        for (const auto& c : p12.unit) CHECK(contrastRatio({0, 0, 0}, c) >= 12.0 - 1e-9);
        for (const auto& c : lmw::ui::theme::palette(21.0).unit) CHECK(c.r == 255 && c.g == 255 && c.b == 255);
    }
    {   // Spec S6.2 (2026-11-21): the rack grey around the panels: #F7F7F7 at AA and AAA, white at Max.
        const auto aa = lmw::ui::theme::palette(4.5).rack, aaa = lmw::ui::theme::palette(7.0).rack, mx = lmw::ui::theme::palette(21.0).rack;
        CHECK(aa.r == 0xF7 && aa.g == 0xF7 && aa.b == 0xF7 && aaa.r == 0xF7);
        CHECK(mx.r == 255 && mx.g == 255 && mx.b == 255);
        std::printf("rack grey, black text: %.1f\n", contrastRatio({0, 0, 0}, lmw::ui::theme::rackDesign));
    }
    {   // Spec S6.6 (Phase 21a): see-through cables at AA — black at cableAlpha() over white looks like the
        // cable grey (>= 4.5:1); opaque from AAA up.
        const auto aa = lmw::ui::theme::palette(4.5);
        const float a = aa.cableAlpha();
        const auto v = static_cast<std::uint8_t>(std::lround(255.0 * (1.0 - a)));
        CHECK(a > 0.5f && a < 0.65f && std::abs(int(v) - int(aa.cable.r)) <= 1);
        CHECK(contrastRatio({v, v, v}, {255, 255, 255}) >= 4.5);
        CHECK(lmw::ui::theme::palette(7.0).cableAlpha() == 1.0f && lmw::ui::theme::palette(21.0).cableAlpha() == 1.0f);
        std::printf("cable at rest: black at %.0f %% = #%02X%02X%02X on white\n", a * 100.0, v, v, v);
    }
    {   // Spec S6.13 (Phase 21d): themes. Every preset at every ratio: all pairs meet it; Default = the
        // designs; Soft paler, Vivid stronger, Monochrome without hue; Vivid black text >= 8:1 by design.
        using namespace lmw::ui::theme;
        for (int t = 0; t < 4; ++t)
            for (double r : {4.5, 7.0, 12.0, 21.0}) {
                const auto pal = palette(r, presetColours(static_cast<ThemePreset>(t)));
                for (const auto& [fg, bg] : pal.pairs()) CHECK(contrastRatio(fg, bg) >= r - 1e-9);
            }
        const auto def = presetColours(ThemePreset::Default), soft = presetColours(ThemePreset::Soft),
                   vivid = presetColours(ThemePreset::Vivid), mono = presetColours(ThemePreset::Monochrome);
        CHECK(def.unit == unitDesign && def.cable.r == cableDesign.r);
        for (std::size_t i = 1; i < unitDesign.size(); ++i) {
            CHECK(relativeLuminance(soft.unit[i]) >= relativeLuminance(def.unit[i]));
            CHECK(contrastRatio({0, 0, 0}, vivid.unit[i]) >= 8.0);
            CHECK(mono.unit[i].r == mono.unit[i].g && mono.unit[i].g == mono.unit[i].b);
        }
        CHECK(parseTheme("vivid") == ThemePreset::Vivid && parseTheme("MONOCHROME") == ThemePreset::Monochrome && !parseTheme("pink"));
        // A dark user colour is lightened until black text on it meets the ratio; a light cable is darkened.
        Colours dark; itemColour(dark, 0) = {90, 0, 0}; itemColour(dark, 10) = {240, 240, 240};
        const auto pd = palette(4.5, dark);
        CHECK(contrastRatio({0, 0, 0}, pd.unit[static_cast<std::size_t>(Unit::Vco)]) >= 4.5 - 1e-9);
        CHECK(contrastRatio(pd.cable, {255, 255, 255}) >= 4.5 - 1e-9);
        // config.ini: theme and colour lines; Custom = Default with the user's colours.
        CHECK(lmw::ui::parseHexColour("#ff8000").has_value() && lmw::ui::parseHexColour("FF8000")->g == 0x80
              && !lmw::ui::parseHexColour("#ff80") && !lmw::ui::parseHexColour("#gg0000"));
        const auto cs = lmw::ui::parseConfig("theme = custom\ncolor_vco = #102030\ncolour_cable=#404040\ncolor_nope = #000000\n");
        CHECK(cs.theme == ThemePreset::Custom && cs.custom[0] && cs.custom[0]->g == 0x20 && cs.custom[10]->r == 0x40);
        CHECK(cs.colours().unit[static_cast<std::size_t>(Unit::Vco)].r == 0x10 && cs.colours().unit[static_cast<std::size_t>(Unit::Env)].b == unitDesign[5].b);
        const auto back = lmw::ui::parseConfig(lmw::ui::writeConfig(cs));
        CHECK(back.theme == ThemePreset::Custom && back.colours().unit == cs.colours().unit && back.colours().cable.r == 0x40);
        CHECK(lmw::ui::parseConfig(lmw::ui::writeConfig(lmw::ui::parseConfig("theme = Soft"))).theme == ThemePreset::Soft);
        CHECK(lmw::ui::parseConfig("theme = Soft\ncolor_vco = #102030").colours().unit == soft.unit);   // colours only for Custom
        // Cable colour and opacity (user 2026-11-22): a blue at 40 % is drawn so at AA when it meets 4.5:1 on
        // white, else the opacity is raised just enough; from AAA up solid; too light an ink is darkened.
        Colours cb; itemColour(cb, 10) = {0, 0, 200}; cb.cableOpacity = 0.4f;
        const auto pb = palette(4.5, cb);
        CHECK(pb.cableInk == (Rgb{0, 0, 200}) && pb.cableAlpha() >= 0.4f && pb.cableAlpha() < 1.0f);
        CHECK(contrastRatio(pb.cable, {255, 255, 255}) >= 4.5 - 1e-9);
        Colours faint; faint.cableOpacity = 0.2f;                 // black at 20 %: raised to about 58 %
        CHECK(palette(4.5, faint).cableAlpha() > 0.5f && palette(7.0, faint).cableAlpha() == 1.0f);
        CHECK(contrastRatio(palette(7.0, cb).cable, {255, 255, 255}) >= 7.0 - 1e-9);
        // config: cable_opacity (percent or fraction), theme_name; saved theme files round trip.
        const auto co = lmw::ui::parseConfig("theme = Custom\ncable_opacity = 75\ntheme_name = My dark\n");
        CHECK(co.customOpacity && std::fabs(*co.customOpacity - 0.75f) < 1e-6 && co.themeName == "My dark" && std::fabs(co.colours().cableOpacity - 0.75f) < 1e-6);
        CHECK(std::fabs(*lmw::ui::parseConfig("cable_opacity = 0.3").customOpacity - 0.3f) < 1e-6 && *lmw::ui::parseConfig("cable_opacity = 5").customOpacity >= 0.2f);
        CHECK(lmw::ui::parseConfig(lmw::ui::writeConfig(co)).themeName == "My dark");
        const auto file = lmw::ui::writeTheme("My dark", cb);
        const auto rt = lmw::ui::readTheme(file);
        CHECK(file.find("name = My dark") != std::string::npos && rt.unit == cb.unit && itemColour(rt, 10) == (Rgb{0, 0, 200})
              && std::fabs(rt.cableOpacity - 0.4f) < 1e-6);
    }
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
        // Spec S6.12: keyboard shown / hidden (unset by default; shown|hidden and yes/no forms).
        CHECK(!parseConfig("").keyboard && parseConfig("keyboard = hidden").keyboard == false
              && parseConfig("keyboard=Shown").keyboard == true && !parseConfig("keyboard = maybe").keyboard);
        Settings k; k.keyboard = false;
        CHECK(parseConfig(writeConfig(k)).keyboard == false && writeConfig(k).find("keyboard = hidden") != std::string::npos);
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

    // T-S12-key (S12.6, S12.7): the Assistant lines of config.ini.
    {
        using lmw::ui::parseConfig;
        using lmw::ui::writeConfig;
        const auto none = parseConfig("contrast = AA\n");
        CHECK(none.assistantStore.empty() && !none.assistantLimit && writeConfig(none).find("assistant") == std::string::npos);
        const auto f = parseConfig("assistant_key_store = File\nassistant_key = sk-ant-x#y\nassistant_model = claude-haiku-5-5\n"
                                   "assistant_effort = LOW\nassistant_limit = $3.5\n");
        CHECK(f.assistantStore == "file" && f.assistantKey == "sk-ant-x#y" && f.assistantModel == "claude-haiku-5-5");
        CHECK(f.assistantEffort == "low" && f.assistantLimit && *f.assistantLimit == 3.5);
        const auto back = parseConfig(writeConfig(f));
        CHECK(back.assistantStore == "file" && back.assistantKey == "sk-ant-x#y" && back.assistantModel == "claude-haiku-5-5"
              && back.assistantEffort == "low" && back.assistantLimit && *back.assistantLimit == 3.5);
        auto k = f;
        k.assistantStore = "keychain";                  // the key is not written with the keychain
        CHECK(writeConfig(k).find("sk-ant") == std::string::npos && writeConfig(k).find("assistant_key_store = keychain") != std::string::npos);
        CHECK(parseConfig("assistant_limit = 0").assistantLimit == 0.0);
        CHECK(!parseConfig("assistant_limit = -1").assistantLimit && parseConfig("assistant_effort = huge").assistantEffort.empty()
              && parseConfig("assistant_key_store = cloud").assistantStore.empty());
    }

    std::printf(failures ? "%d failure(s)\n" : "all contrast tests passed\n", failures);
    return failures ? 1 : 0;
}
