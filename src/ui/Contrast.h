// SPDX-License-Identifier: AGPL-3.0-or-later
// WCAG 2.x contrast utilities (Spec S6.2, S6.3). Pure C++, no JUCE.
#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace lmw::ui {

struct Rgb { std::uint8_t r{}, g{}, b{}; };

// WCAG relative luminance, 0.0 (black) .. 1.0 (white).
double relativeLuminance(Rgb c) noexcept;

// WCAG contrast ratio, 1.0 .. 21.0 (order of arguments irrelevant).
double contrastRatio(Rgb a, Rgb b) noexcept;

enum class ContrastPreset { AA, AAA, Max, Custom };

// Minimum ratio for a preset; `custom` is used (clamped to 4.5..21: never below AA) for Custom.
double minimumRatio(ContrastPreset p, double custom = 4.5) noexcept;

// Parse "AA", "AAA", "Max", "Custom" (case-insensitive).
std::optional<ContrastPreset> parsePreset(std::string_view s) noexcept;

// Returns fg unchanged if it already meets `minRatio` against bg; otherwise
// moves fg towards black or white (whichever direction can reach the target)
// until the ratio is met. Max (21:1) always yields pure black/white.
Rgb enforceContrast(Rgb fg, Rgb bg, double minRatio) noexcept;

} // namespace lmw::ui
