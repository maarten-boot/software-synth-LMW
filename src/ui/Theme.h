// SPDX-License-Identifier: AGPL-3.0-or-later
// Colour theme (Spec S6.2, S6.3). The GUI is black on white (21:1); the only design greys are the
// pressed-key colours of the on-screen keyboard. `palette(minRatio)` raises every design colour to
// the chosen minimum contrast (presets AA 4.5, AAA 7, Max 21, or a custom ratio); every pair the
// GUI draws is listed by `pairs()` and verified by tests/test_contrast.cpp.
#pragma once
#include "ui/Contrast.h"
#include <array>
#include <utility>

namespace lmw::ui::theme {
inline constexpr Rgb background{255, 255, 255};
inline constexpr Rgb foreground{0, 0, 0};
inline constexpr Rgb keyDownDesign{0x76, 0x76, 0x76};   // 4.54:1 on white, 4.63:1 on black

// The better of black and white as text on `fill` (at least sqrt(21) ~ 4.58:1 by construction).
inline Rgb textOn(Rgb fill) noexcept {
    return contrastRatio(foreground, fill) >= contrastRatio(background, fill) ? foreground : background;
}

struct Palette {
    double minRatio = 4.5;
    Rgb whiteKeyDown{keyDownDesign};   // pressed white key, against the white keys around it
    Rgb blackKeyDown{keyDownDesign};   // pressed black key, against the black keys around it

    // (foreground, background) pairs actually drawn by the GUI.
    std::array<std::pair<Rgb, Rgb>, 5> pairs() const noexcept {
        return {{
            {foreground, background},             // text, faders, value boxes, outlines, cables, black keys
            {background, foreground},             // highlighted menu text, fader cap line, output sockets
            {whiteKeyDown, background},           // pressed white key
            {blackKeyDown, foreground},           // pressed black key
            {textOn(whiteKeyDown), whiteKeyDown}, // note name (C4) on a pressed white key
        }};
    }
};

inline Palette palette(double minRatio) noexcept {
    Palette p;
    p.minRatio = minRatio;
    p.whiteKeyDown = enforceContrast(keyDownDesign, background, minRatio);
    p.blackKeyDown = enforceContrast(keyDownDesign, foreground, minRatio);
    return p;
}
} // namespace lmw::ui::theme
