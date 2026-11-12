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

// Spec S6.9: unit colours — light tints behind black title text, one per unit type. Units without a
// colour (MIXER, KEYS, OUT, effects) keep white.
enum class Unit : std::uint8_t { None, Vco, Noise, Vcf, Vca, Env, Lfo, Count };
inline constexpr std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unitDesign{{
    {255, 255, 255},   // None: white
    {255, 179, 179},   // VCO: red
    {255, 204, 153},   // NOISE: orange
    {255, 240, 160},   // VCF: yellow
    {184, 235, 184},   // VCA: green
    {179, 209, 255},   // ENV (ADSR): blue
    {217, 179, 255},   // LFO: purple
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
    if (k == "vcf") return Unit::Vcf;
    if (k == "vca") return Unit::Vca;
    if (k == "env") return Unit::Env;
    if (k == "lfo") return Unit::Lfo;
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
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};   // title bar fills (S6.9)

    Rgb unitColour(Unit u) const noexcept { return unit[static_cast<std::size_t>(u)]; }

    // (foreground, background) pairs actually drawn by the GUI.
    std::array<std::pair<Rgb, Rgb>, 5 + static_cast<std::size_t>(Unit::Count)> pairs() const noexcept {
        std::array<std::pair<Rgb, Rgb>, 5 + static_cast<std::size_t>(Unit::Count)> p{{
            {foreground, background},             // text, faders, value boxes, outlines, cables, black keys
            {background, foreground},             // highlighted menu text, fader cap line, output sockets
            {whiteKeyDown, background},           // pressed white key
            {blackKeyDown, foreground},           // pressed black key
            {textOn(whiteKeyDown), whiteKeyDown}, // note name (C4) on a pressed white key
        }};
        for (std::size_t i = 0; i < unit.size(); ++i)
            p[5 + i] = {foreground, unit[i]};     // black title text, line and focus ring on a unit colour
        return p;
    }
};

inline Palette palette(double minRatio) noexcept {
    Palette p;
    p.minRatio = minRatio;
    p.whiteKeyDown = enforceContrast(keyDownDesign, background, minRatio);
    p.blackKeyDown = enforceContrast(keyDownDesign, foreground, minRatio);
    // Unit colours are lightened (towards white) until black text on them meets the ratio; at Max (21:1)
    // that is white, so the bars are black and white like everything else.
    for (std::size_t i = 0; i < p.unit.size(); ++i) p.unit[i] = enforceContrast(unitDesign[i], foreground, minRatio);
    return p;
}
} // namespace lmw::ui::theme
