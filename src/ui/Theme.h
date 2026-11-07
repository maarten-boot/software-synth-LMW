// SPDX-License-Identifier: AGPL-3.0-or-later
// Default colour theme (Spec S6.2). Every foreground/background pair used by
// the GUI is listed in `pairs` and verified by tests/test_contrast.cpp.
#pragma once
#include "ui/Contrast.h"
#include <array>
#include <utility>

namespace lmw::ui::theme {
inline constexpr Rgb background{255, 255, 255};
inline constexpr Rgb foreground{0, 0, 0};
inline constexpr Rgb keyDown{0x76, 0x76, 0x76};   // 4.54:1 on white

// (foreground, background) pairs actually drawn by the GUI.
inline constexpr std::array<std::pair<Rgb, Rgb>, 3> pairs{{
    {foreground, background},   // text, faders, value boxes, outlines, black keys
    {background, foreground},   // highlighted menu text, fader cap centre line
    {keyDown, background},      // pressed white key
}};
} // namespace lmw::ui::theme
