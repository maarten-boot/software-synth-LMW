// SPDX-License-Identifier: AGPL-3.0-or-later
// Colour theme (Spec S6.2, S6.3). The GUI is black on white (21:1); the only design greys are the
// pressed-key colours of the on-screen keyboard and the unit colours of the title bars (S6.9). `palette(minRatio)` raises every design colour to
// the chosen minimum contrast (presets AA 4.5, AAA 7, Max 21, or a custom ratio); every pair the
// GUI draws is listed by `pairs()` and verified by tests/test_contrast.cpp.
#pragma once
#include "ui/Contrast.h"
#include <array>
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
// Spec S6.6, S6.7 (user 2026-11-22): cables at rest are grey, black only for the socket under the mouse or
// with keyboard focus. Drawn on their own white edge: 5.3:1; darker with the contrast preset (black at Max).
inline constexpr Rgb cableDesign{0x6B, 0x6B, 0x6B};

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
    if (k == "del" || k == "rev") return Unit::Effect;   // DELAY (S7.8), REVERB
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
    Rgb cable{cableDesign};            // cables at rest (Synth and Schematic tabs)
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};   // title bar fills (S6.9)

    Rgb unitColour(Unit u) const noexcept { return unit[static_cast<std::size_t>(u)]; }
    // Spec S6.6 (Phase 21a): cables at rest are drawn see-through — black (with a white edge) at this
    // opacity, so on white they look exactly like `cable` and the panels show through; opaque (1) from
    // AAA 7:1 up, where `cable` itself is drawn.
    float cableAlpha() const noexcept {
        if (minRatio >= 7.0) return 1.0f;
        const float grey = (static_cast<float>(cable.r) + static_cast<float>(cable.g) + static_cast<float>(cable.b)) / (3.0f * 255.0f);
        return 1.0f - grey;
    }

    // (foreground, background) pairs actually drawn by the GUI.
    std::array<std::pair<Rgb, Rgb>, 8 + static_cast<std::size_t>(Unit::Count)> pairs() const noexcept {
        std::array<std::pair<Rgb, Rgb>, 8 + static_cast<std::size_t>(Unit::Count)> p{{
            {foreground, background},             // text, faders, value boxes, outlines, cables, black keys
            {background, foreground},             // highlighted menu text, fader cap line, output sockets
            {whiteKeyDown, background},           // pressed white key
            {blackKeyDown, foreground},           // pressed black key
            {textOn(whiteKeyDown), whiteKeyDown}, // note name (C4) on a pressed white key
            {foreground, rack},                   // panel frames and the drop marker on the rack grey
            {foreground, band},                   // Matrix: rings, strokes and grid on a highlighted band
            {cable, background},                  // a cable at rest on its white edge
        }};
        for (std::size_t i = 0; i < unit.size(); ++i)
            p[8 + i] = {foreground, unit[i]};     // black title text, line and focus ring on a unit colour
        return p;
    }
};

inline Palette palette(double minRatio) noexcept {
    Palette p;
    p.minRatio = minRatio;
    p.whiteKeyDown = enforceContrast(keyDownDesign, background, minRatio);
    p.blackKeyDown = enforceContrast(keyDownDesign, foreground, minRatio);
    p.rack = enforceContrast(rackDesign, foreground, minRatio);
    p.band = enforceContrast(bandDesign, foreground, minRatio);
    p.cable = enforceContrast(cableDesign, background, minRatio);
    // Unit colours are lightened (towards white) until black text on them meets the ratio; at Max (21:1)
    // that is white, so the bars are black and white like everything else.
    for (std::size_t i = 0; i < p.unit.size(); ++i) p.unit[i] = enforceContrast(unitDesign[i], foreground, minRatio);
    return p;
}
} // namespace lmw::ui::theme
