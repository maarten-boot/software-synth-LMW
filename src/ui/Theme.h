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
inline constexpr Rgb background{255, 255, 255};   // the light themes' page and text (S6.2); dark themes: S6.29
inline constexpr Rgb foreground{0, 0, 0};
// Spec S6.29 (Phase 48): the dark themes — off-white on dark grey (not white on black: light text bleeds, worse
// with astigmatism), and a high-contrast one; the keyboard stays a piano (light grey white keys).
inline constexpr Rgb darkForeground{0xE6, 0xE6, 0xE6}, darkBackground{0x1E, 0x1E, 0x1E};   // 13.4:1
inline constexpr Rgb darkHcForeground{0xFF, 0xFF, 0xFF}, darkHcBackground{0x0A, 0x0A, 0x0A};   // ~19.8:1
inline constexpr Rgb darkWhiteKey{0xC8, 0xC8, 0xC8}, darkBlackKey{0x14, 0x14, 0x14};
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
// Spec S6.19 (Phase 32, user 2026-11-25): socket type rings, lines on white (raised to the contrast like the
// cables): audio dark blue 6.7:1, CV dark orange 6.0:1, gate dark magenta 7.0:1. Index = lmw::dsp::SignalType.
inline constexpr std::array<Rgb, 3> socketDesign{{{0x0B, 0x5C, 0xAD}, {0xA0, 0x4A, 0x00}, {0xB0, 0x00, 0x60}}};

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
    if (k == "vca" || k == "mvc") return Unit::Vca;                     // MVCA 1-3 (S5.32): the VCA green
    if (k == "env") return Unit::Env;
    if (k == "lfo") return Unit::Lfo;
    if (k == "mix") return Unit::Mix;
    return Unit::None;
}

// The better of two text colours on `fill` (black and white: at least sqrt(21) ~ 4.58:1 by construction).
inline Rgb textOn(Rgb fill, Rgb a = foreground, Rgb b = background) noexcept {
    return contrastRatio(a, fill) >= contrastRatio(b, fill) ? a : b;
}

struct Palette {
    double minRatio = 4.5;
    Rgb foreground{theme::foreground}, background{theme::background};   // S6.29: text and page of this theme
    bool dark = false;                 // S6.29: a dark theme (small texts semi-bold, choice C)
    Rgb whiteKey{theme::background}, blackKey{theme::foreground};        // S6.29: the keyboard stays a piano
    Rgb keyText{theme::foreground};    // note names and separator lines on the white keys
    Rgb whiteKeyDown{keyDownDesign};   // pressed white key, against the white keys around it
    Rgb blackKeyDown{keyDownDesign};   // pressed black key, against the black keys around it
    Rgb rack{rackDesign};              // behind the panels on the Synth and Effects tabs
    Rgb band{bandDesign};              // Matrix: highlighted row / column
    Rgb highlight{highlightDesign};    // S6.15: box behind the selected socket (white at Max: edge only)
    Rgb cable{0x6B, 0x6B, 0x6B};       // cables at rest as they look on white (opaque; drawn so from AAA up)
    Rgb cableInk{cableDesign};         // the colour drawn at cableOpacity (below AAA)
    float cableOpacity = 1.0f;
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};   // title bar fills (S6.9)
    std::array<Rgb, 3> socket{socketDesign};   // S6.19: type rings (audio, CV, gate) on white

    Rgb unitColour(Unit u) const noexcept { return unit[static_cast<std::size_t>(u)]; }
    Rgb textOn(Rgb fill) const noexcept { return theme::textOn(fill, foreground, background); }
    // Spec S6.29: secondary text (scale values, tags) — 30 % of the way from the text to the page, then >= 7:1.
    Rgb quiet() const noexcept {
        auto m = [](std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>(a + (b - a) * 0.3 + 0.5); };
        const Rgb mid{m(foreground.r, background.r), m(foreground.g, background.g), m(foreground.b, background.b)};
        return enforceContrast(mid, background, std::max(7.0, minRatio));
    }
    // Spec S6.30 (Phase 52): the top of a panel gradient — the unit's title colour, moved towards the page just as
    // far as needed for the text (and the quiet text) on it to keep the contrast setting; none for the white and
    // grey units (KEYS, OUT, MAIN, MIXER, MIX 2: they stay plain).
    std::optional<Rgb> gradientTop(Unit u) const noexcept {
        if (u == Unit::None || u == Unit::Mix || u == Unit::Count) return std::nullopt;
        const Rgb t = unitColour(u), q = quiet();
        for (int k = 0; k <= 20; ++k) {
            const double f = k / 20.0;
            auto c = [f](std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>(a + (b - a) * f + 0.5); };
            const Rgb top{c(t.r, background.r), c(t.g, background.g), c(t.b, background.b)};
            if (contrastRatio(foreground, top) >= minRatio && contrastRatio(q, top) >= minRatio) return top;
        }
        return background;
    }
    // Spec S6.6 (Phase 21a): cables at rest are drawn see-through — cableInk (with a white edge) at this
    // opacity, so on white they look like `cable` and the panels show through; opaque (1) from AAA 7:1 up,
    // where `cable` itself is drawn.
    float cableAlpha() const noexcept { return cableOpacity; }
    Rgb cableDrawn() const noexcept { return cableOpacity < 1.0f ? cableInk : cable; }

    // (foreground, background) pairs actually drawn by the GUI.
    std::array<std::pair<Rgb, Rgb>, 14 + static_cast<std::size_t>(Unit::Count)> pairs() const noexcept {
        std::array<std::pair<Rgb, Rgb>, 14 + static_cast<std::size_t>(Unit::Count)> p{{
            {foreground, background},             // text, faders, value boxes, outlines, cables
            {background, foreground},             // highlighted menu text, fader cap line, output sockets
            {whiteKeyDown, whiteKey},             // pressed white key
            {blackKeyDown, blackKey},             // pressed black key
            {theme::textOn(whiteKeyDown, keyText, whiteKey), whiteKeyDown},   // note name (C4) on a pressed white key
            {foreground, rack},                   // panel frames and the drop marker on the rack grey
            {foreground, band},                   // Matrix: rings, strokes and grid on a highlighted band
            {cable, background},                  // a cable at rest on its white edge
            {foreground, highlight},              // S6.15: socket, label and edge on the highlight box
            {socket[0], background},              // S6.19: audio ring on white
            {socket[1], background},              //        CV ring
            {socket[2], background},              //        gate ring
            {keyText, whiteKey},                  // S6.29: note names and lines on the white keys
            {blackKey, whiteKey},                 //        black keys against the white keys
        }};
        for (std::size_t i = 0; i < unit.size(); ++i)
            p[14 + i] = {foreground, unit[i]};     // title text, line and focus ring on a unit colour
        return p;
    }
};

// ---- Spec S6.13 (Phase 21d): themes — the colours a user may change, before the contrast is applied.
struct Colours {
    bool dark = false;                         // S6.29: a dark base (keyboard, small text)
    Rgb foreground{theme::foreground};         // S6.29: text
    Rgb background{theme::background};         // S6.29: panels and page
    std::array<Rgb, static_cast<std::size_t>(Unit::Count)> unit{unitDesign};
    Rgb rack{rackDesign};
    Rgb band{bandDesign};
    Rgb cable{cableDesign};
    float cableOpacity = cableOpacityDesign;   // 0.2 .. 1
    Rgb highlight{highlightDesign};            // S6.15
    std::array<Rgb, 3> socket{socketDesign};   // S6.19: audio, CV, gate rings
};
// S6.29: Dark, DarkHC (Dark high contrast) and System (Default or Dark, as the OS is light or dark; the editor
// resolves it). Kept by name in config.ini, so the order may change.
enum class ThemePreset { Default, Soft, Vivid, Monochrome, Dark, DarkHC, System, Custom };
inline constexpr std::array<std::string_view, 8> themeNames{"Default", "Soft", "Vivid", "Monochrome", "Dark", "DarkHC", "System", "Custom"};
inline constexpr std::array<std::string_view, 8> themeLabels{"Default", "Soft", "Vivid", "Monochrome", "Dark", "Dark high contrast",
                                                             "Follow system", "Custom"};

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
inline constexpr std::array<ThemeItem, 17> themeItems{{
    {"vco", "VCO", true}, {"noise", "NOISE", true}, {"vcf", "VCF, FORMANT, COMB", true}, {"vca", "VCA", true},
    {"env", "ENV", true}, {"lfo", "LFO", true}, {"mixer", "MIXER", true}, {"effects", "ENSEMBLE, DELAY, REVERB", true},
    {"background", "Background around the panels", true}, {"band", "Matrix highlight band", true},
    {"cable", "Cables at rest (colour)", false},
    {"highlight", "Selected socket (highlight)", true},
    {"socket_audio", "Audio sockets (ring)", false}, {"socket_cv", "CV sockets (ring)", false},
    {"socket_gate", "Gate sockets (ring)", false},     // S6.19
    {"text", "Text (and lines)", false}, {"page", "Panels (page)", true},   // S6.29: edited on a dark base too
}};
inline Rgb& itemColour(Colours& c, std::size_t item) noexcept {
    if (item < 8) return c.unit[item + 1];                     // Vco .. Effect
    if (item == 8) return c.rack;
    if (item == 9) return c.band;
    if (item == 11) return c.highlight;
    if (item >= 12 && item < 15) return c.socket[item - 12];
    if (item == 15) return c.foreground;                       // S6.29
    if (item == 16) return c.background;
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
            c.socket = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};   // S6.19: the ring styles tell them apart
            break;
        case ThemePreset::Dark:                              // S6.29: dark muted tints behind light text
        case ThemePreset::DarkHC:
            c.dark = true;
            c.foreground = t == ThemePreset::Dark ? darkForeground : darkHcForeground;
            c.background = t == ThemePreset::Dark ? darkBackground : darkHcBackground;
            c.unit = {{c.background, {0x5E, 0x2A, 0x2A}, {0x5E, 0x3D, 0x1C}, {0x4E, 0x45, 0x18}, {0x1F, 0x4A, 0x26},
                       {0x1F, 0x3A, 0x60}, {0x45, 0x29, 0x5E}, {0x38, 0x38, 0x38}, {0x3F, 0x3A, 0x1E}}};
            c.rack = t == ThemePreset::Dark ? Rgb{0x14, 0x14, 0x14} : Rgb{0x00, 0x00, 0x00};
            c.band = Rgb{0x3C, 0x3C, 0x3C};
            c.cable = c.foreground;
            c.highlight = Rgb{0x6B, 0x50, 0x00};             // dark amber behind light text
            c.socket = {{{0x5A, 0xA9, 0xFF}, {0xFF, 0x9F, 0x43}, {0xFF, 0x66, 0xC4}}};
            break;
        case ThemePreset::Default:
        case ThemePreset::System:                            // resolved by the editor; Default if not
        case ThemePreset::Custom:
            break;
    }
    return c;
}

// The palette: every design colour raised to the chosen minimum contrast (S6.3), from the theme's colours, against
// its own text and page (S6.29: light text on dark for the dark themes).
inline Palette palette(double minRatio, const Colours& c = Colours{}) noexcept {
    Palette p;
    p.minRatio = minRatio;
    p.dark = c.dark;
    Rgb fg = c.foreground, bg = c.background;            // text first; the page only if the text cannot get there
    if (contrastRatio(fg, bg) < minRatio) fg = enforceContrast(fg, bg, minRatio);
    if (contrastRatio(fg, bg) < minRatio) bg = enforceContrast(bg, fg, minRatio);
    p.foreground = fg;
    p.background = bg;
    // The keyboard: a piano in every theme — light themes the page and the text, dark ones light grey white keys
    // and near-black black keys, with dark note names on the white keys.
    p.whiteKey = c.dark ? darkWhiteKey : bg;
    p.blackKey = c.dark ? darkBlackKey : fg;
    p.keyText = c.dark ? theme::foreground : fg;
    if (contrastRatio(p.keyText, p.whiteKey) < minRatio) p.whiteKey = enforceContrast(p.whiteKey, p.keyText, minRatio);
    if (contrastRatio(p.blackKey, p.whiteKey) < minRatio) p.blackKey = enforceContrast(p.blackKey, p.whiteKey, minRatio);
    p.whiteKeyDown = enforceContrast(keyDownDesign, p.whiteKey, minRatio);
    p.blackKeyDown = enforceContrast(keyDownDesign, p.blackKey, minRatio);
    if (contrastRatio(theme::textOn(p.whiteKeyDown, p.keyText, p.whiteKey), p.whiteKeyDown) < minRatio)
        p.whiteKeyDown = enforceContrast(p.whiteKeyDown, p.keyText, minRatio);
    p.rack = enforceContrast(c.rack, fg, minRatio);
    p.band = enforceContrast(c.band, fg, minRatio);
    // Cables at rest: `ink` at `opacity` over the page looks `blend`; below AAA drawn so (the opacity raised
    // just enough, or the ink moved, until the look meets the ratio on the page), from AAA up opaque.
    {
        auto over = [bg](Rgb ink, float a) {
            auto m = [a](std::uint8_t x, std::uint8_t b) { return static_cast<std::uint8_t>(x * a + b * (1.0f - a) + 0.5f); };
            return Rgb{m(ink.r, bg.r), m(ink.g, bg.g), m(ink.b, bg.b)};
        };
        Rgb ink = c.cable;
        float a = std::clamp(c.cableOpacity, 0.2f, 1.0f);
        if (contrastRatio(ink, bg) < minRatio) ink = enforceContrast(ink, bg, minRatio);
        while (a < 1.0f && contrastRatio(over(ink, a), bg) < minRatio) a = std::min(1.0f, a + 0.01f);
        const Rgb look = over(ink, a);
        if (minRatio >= 7.0) { p.cable = enforceContrast(look, bg, minRatio); p.cableInk = p.cable; p.cableOpacity = 1.0f; }
        else { p.cable = look; p.cableInk = ink; p.cableOpacity = a; }
    }
    // Unit colours are moved away from the text (lighter behind black text, darker behind light text) until the
    // text on them meets the ratio; at Max (21:1) that is the page colour, so the bars are like everything else.
    for (std::size_t i = 0; i < p.unit.size(); ++i) p.unit[i] = enforceContrast(c.unit[i], fg, minRatio);
    p.highlight = enforceContrast(c.highlight, fg, minRatio);
    for (std::size_t i = 0; i < p.socket.size(); ++i) p.socket[i] = enforceContrast(c.socket[i], bg, minRatio);
    return p;
}

// Spec S6.29: the palette the GUI draws with now (message thread; set by the editor when the theme or the contrast
// changes; one for the process, as the settings are).
inline Palette& current() noexcept {
    static Palette p = palette(4.5);
    return p;
}
} // namespace lmw::ui::theme
