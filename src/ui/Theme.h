// SPDX-License-Identifier: AGPL-3.0-or-later
// Colour theme (Spec S6.2, S6.3). The GUI is black on white (21:1); the only design greys are the
// pressed-key colours of the on-screen keyboard and the unit colours of the title bars (S6.9). `palette(minRatio)` raises every design colour to
// the chosen minimum contrast (presets AA 4.5, AAA 7, Max 21, or a custom ratio); every pair the
// GUI draws is listed by `pairs()` and verified by tests/test_contrast.cpp.
#pragma once
#include "ui/Contrast.h"
#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace lmw::ui::theme {
inline constexpr Rgb background{255, 255, 255};
inline constexpr Rgb foreground{0, 0, 0};
inline constexpr Rgb keyDownDesign{0x76, 0x76, 0x76};   // 4.54:1 on white, 4.63:1 on black
// Spec S6.2 (user 2026-11-21): the Synth and Effects tabs show a very light grey around the panels
// (the panels stay white); 19.6:1 with black. Raised towards white with the contrast preset (white at Max).
inline constexpr Rgb rackDesign{0xF7, 0xF7, 0xF7};
// Spec S6.8 (user 2026-11-21): the Matrix band behind the row and column under the mouse or pinned;
// 15.9:1 with black, raised towards white with the contrast preset (white at Max: the labels still invert).
inline constexpr Rgb bandDesign{0xE0, 0xE0, 0xE0};
// Spec S6.6, S6.7 (user 2026-11-22): cables at rest look grey, black only for the socket under the mouse or
// with keyboard focus: drawn black at 58 % (Phase 21a), which on white looks #6B6B6B, 5.3:1. Colour and
// opacity are theme settings (S6.13, user 2026-11-22).
inline constexpr Rgb cableDesign{0x00, 0x00, 0x00};
inline constexpr float cableOpacityDesign = 0.58f;
// Spec S6.15 (Phase 24a, user 2026-11-24): the selected socket (keyboard focus, under the mouse, drop target)
// sits on a golden-yellow box with a black edge; black on it 12.8:1. A theme item, raised like the unit colours.
inline constexpr Rgb highlightDesign{0xFF, 0xC0, 0x00};

// Spec S6.9: unit colours — light tints behind black title text, one per unit type. Units without a
// colour (KEYS, OUT, SYNTH, MAIN) keep white. Effects (user 2026-11-22): COMB is a filter (VCF yellow),
// DELAY and REVERB change the sound like one — a lighter yellow.
enum class Unit : std::uint8_t { None, Vco, Noise, Vcf, Vca, Env, Lfo, Mix, Effect, Count };
inline constexpr std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unitDesign{{
    {255, 255, 255},   // None: white
    {255, 179, 179},   // VCO: red
    {255, 204, 153},   // NOISE: orange
    {255, 240, 160},   // VCF: yellow
    {184, 235, 184},   // VCA: green
    {179, 209, 255},   // ENV (ADSR): blue
    {217, 179, 255},   // LFO: purple
    {240, 240, 240},   // MIXER: very light grey (user 2026-11-14)
    {255, 248, 208},   // DELAY, REVERB: very light yellow #FFF8D0 (user 2026-11-22)
}};

// Unit type of a panel title ("VCO 1", "ENV 2 → VCF") or a port's module name ("vcf1", "lfo2.out"):
// by its first three letters, any case.
inline constexpr Unit unitOf(std::string_view name) noexcept {
    if (name.size() < 3) return Unit::None;
    char c[3]{};
    for (int i = 0; i < 3; ++i) {
        const char ch = name[static_cast<std::size_t>(i)];
        c[i] = (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    }
    const std::string_view k(c, 3);
    if (k == "vco") return Unit::Vco;
    if (k == "noi") return Unit::Noise;
    if (k == "vcf" || k == "for" || k == "com") return Unit::Vcf;   // FORMANT (S5.16) and COMB (S7.6) are filters too
    if (k == "del" || k == "rev" || k == "ens") return Unit::Effect;   // DELAY (S7.8), REVERB, ENSEMBLE (S7.11)
    if (k == "vca") return Unit::Vca;
    if (k == "env") return Unit::Env;
    if (k == "lfo") return Unit::Lfo;
    if (k == "mix") return Unit::Mix;
    return Unit::None;
}

// The better of black and white as text on `fill` (at least sqrt(21) ~ 4.58:1 by construction).
inline Rgb textOn(Rgb fill) noexcept {
    return contrastRatio(foreground, fill) >= contrastRatio(background, fill) ? foreground : background;
}

struct Palette {
    double minRatio = 4.5;
    Rgb whiteKeyDown{keyDownDesign};   // pressed white key, against the white keys around it
    Rgb blackKeyDown{keyDownDesign};   // pressed black key, against the black keys around it
    Rgb rack{rackDesign};              // behind the panels on the Synth and Effects tabs
    Rgb band{bandDesign};              // Matrix: highlighted row / column
    Rgb highlight{highlightDesign};    // S6.15: box behind the selected socket (white at Max: edge only)
    Rgb cable{0x6B, 0x6B, 0x6B};       // cables at rest as they look on white (opaque; drawn so from AAA up)
    Rgb cableInk{cableDesign};         // the colour drawn at cableOpacity (below AAA)
    float cableOpacity = 1.0f;
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};   // title bar fills (S6.9)

    Rgb unitColour(Unit u) const noexcept { return unit[static_cast<std::size_t>(u)]; }
    // Spec S6.6 (Phase 21a): cables at rest are drawn see-through — cableInk (with a white edge) at this
    // opacity, so on white they look like `cable` and the panels show through; opaque (1) from AAA 7:1 up,
    // where `cable` itself is drawn.
    float cableAlpha() const noexcept { return cableOpacity; }
    Rgb cableDrawn() const noexcept { return cableOpacity < 1.0f ? cableInk : cable; }

    // (foreground, background) pairs actually drawn by the GUI.
    std::array<std::pair<Rgb, Rgb>, 9 + static_cast<std::size_t>(Unit::Count)> pairs() const noexcept {
        std::array<std::pair<Rgb, Rgb>, 9 + static_cast<std::size_t>(Unit::Count)> p{{
            {foreground, background},             // text, faders, value boxes, outlines, cables, black keys
            {background, foreground},             // highlighted menu text, fader cap line, output sockets
            {whiteKeyDown, background},           // pressed white key
            {blackKeyDown, foreground},           // pressed black key
            {textOn(whiteKeyDown), whiteKeyDown}, // note name (C4) on a pressed white key
            {foreground, rack},                   // panel frames and the drop marker on the rack grey
            {foreground, band},                   // Matrix: rings, strokes and grid on a highlighted band
            {cable, background},                  // a cable at rest on its white edge
            {foreground, highlight},              // S6.15: socket, label and edge on the highlight box
        }};
        for (std::size_t i = 0; i < unit.size(); ++i)
            p[9 + i] = {foreground, unit[i]};     // black title text, line and focus ring on a unit colour
        return p;
    }
};

// ---- Spec S6.13 (Phase 21d): themes — the colours a user may change, before the contrast is applied.
struct Colours {
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};
    Rgb rack{rackDesign};
    Rgb band{bandDesign};
    Rgb cable{cableDesign};
    float cableOpacity = cableOpacityDesign;   // 0.2 .. 1
    Rgb highlight{highlightDesign};            // S6.15
};
enum class ThemePreset { Default, Soft, Vivid, Monochrome, Custom };
inline constexpr std::array<std::string_view, 5> themeNames{"Default", "Soft", "Vivid", "Monochrome", "Custom"};

inline std::optional<ThemePreset> parseTheme(std::string_view name) noexcept {
    for (std::size_t i = 0; i < themeNames.size(); ++i) {
        const auto n = themeNames[i];
        if (n.size() != name.size()) continue;
        bool same = true;
        for (std::size_t k = 0; k < n.size() && same; ++k) {
            const char a = n[k], b = name[k];
            same = (a | 0x20) == (b | 0x20);
        }
        if (same) return static_cast<ThemePreset>(i);
    }
    return std::nullopt;
}

// The editable items: the unit colours (not None), the three greys and the socket highlight (S6.15).
// key: config.ini "color_<key>". New items go at the end (indices are kept).
struct ThemeItem { std::string_view key, label; bool textOnIt; };   // textOnIt: black text on it (else a line on white)
inline constexpr std::array<ThemeItem, 12> themeItems{{
    {"vco", "VCO", true}, {"noise", "NOISE", true}, {"vcf", "VCF, FORMANT, COMB", true}, {"vca", "VCA", true},
    {"env", "ENV", true}, {"lfo", "LFO", true}, {"mixer", "MIXER", true}, {"effects", "ENSEMBLE, DELAY, REVERB", true},
    {"background", "Background around the panels", true}, {"band", "Matrix highlight band", true},
    {"cable", "Cables at rest (colour)", false},
    {"highlight", "Selected socket (highlight)", true},
}};
inline Rgb& itemColour(Colours& c, std::size_t item) noexcept {
    if (item < 8) return c.unit[item + 1];                     // Vco .. Effect
    if (item == 8) return c.rack;
    if (item == 9) return c.band;
    if (item == 11) return c.highlight;
    return c.cable;
}
inline Rgb itemColour(const Colours& c, std::size_t item) noexcept { return itemColour(const_cast<Colours&>(c), item); }

inline Colours presetColours(ThemePreset t) noexcept {
    Colours c;
    auto blend = [](Rgb a, Rgb b, double f) {               // a towards b by f
        auto m = [f](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(x + (y - x) * f + 0.5); };
        return Rgb{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
    };
    switch (t) {
        case ThemePreset::Soft:                              // paler tints, half way to white
            for (std::size_t i = 1; i < c.unit.size(); ++i) c.unit[i] = blend(unitDesign[i], background, 0.5);
            c.highlight = Rgb{0xFF, 0xE0, 0x8A};
            break;
        case ThemePreset::Vivid:                             // stronger tints (black text still >= 8:1)
            c.unit = {{{255, 255, 255}, {255, 140, 140}, {255, 176, 102}, {255, 226, 92}, {140, 220, 140},
                       {140, 182, 255}, {196, 140, 255}, {224, 224, 224}, {255, 236, 150}}};
            c.highlight = Rgb{0xFF, 0xB0, 0x00};
            break;
        case ThemePreset::Monochrome:                        // no hues: light greys only
            for (std::size_t i = 1; i < c.unit.size(); ++i) c.unit[i] = Rgb{232, 232, 232};
            c.unit[static_cast<std::size_t>(Unit::Mix)] = Rgb{244, 244, 244};
            c.unit[static_cast<std::size_t>(Unit::Effect)] = Rgb{244, 244, 244};
            c.highlight = Rgb{0xD0, 0xD0, 0xD0};
            break;
        case ThemePreset::Default:
        case ThemePreset::Custom:
            break;
    }
    return c;
}

// The palette: every design colour raised to the chosen minimum contrast (S6.3), from the theme's colours.
inline Palette palette(double minRatio, const Colours& c = Colours{}) noexcept {
    Palette p;
    p.minRatio = minRatio;
    p.whiteKeyDown = enforceContrast(keyDownDesign, background, minRatio);
    p.blackKeyDown = enforceContrast(keyDownDesign, foreground, minRatio);
    p.rack = enforceContrast(c.rack, foreground, minRatio);
    p.band = enforceContrast(c.band, foreground, minRatio);
    // Cables at rest: `ink` at `opacity` over white looks `blend`; below AAA drawn so (the opacity raised
    // just enough, or the ink darkened, until the look meets the ratio on white), from AAA up opaque.
    {
        auto over = [](Rgb ink, float a) {
            auto m = [a](std::uint8_t x) { return static_cast<std::uint8_t>(x * a + 255.0f * (1.0f - a) + 0.5f); };
            return Rgb{m(ink.r), m(ink.g), m(ink.b)};
        };
        Rgb ink = c.cable;
        float a = std::clamp(c.cableOpacity, 0.2f, 1.0f);
        if (contrastRatio(ink, background) < minRatio) ink = enforceContrast(ink, background, minRatio);
        while (a < 1.0f && contrastRatio(over(ink, a), background) < minRatio) a = std::min(1.0f, a + 0.01f);
        const Rgb look = over(ink, a);
        if (minRatio >= 7.0) { p.cable = enforceContrast(look, background, minRatio); p.cableInk = p.cable; p.cableOpacity = 1.0f; }
        else { p.cable = look; p.cableInk = ink; p.cableOpacity = a; }
    }
    // Unit colours are lightened (towards white) until black text on them meets the ratio; at Max (21:1)
    // that is white, so the bars are black and white like everything else.
    for (std::size_t i = 0; i < p.unit.size(); ++i) p.unit[i] = enforceContrast(c.unit[i], foreground, minRatio);
    p.highlight = enforceContrast(c.highlight, foreground, minRatio);
    return p;
}
} // namespace lmw::ui::theme
