// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui/Contrast.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace lmw::ui {
namespace {
double channel(std::uint8_t v) noexcept {
    const double c = v / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
Rgb mix(Rgb a, Rgb b, double t) noexcept {
    auto m = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}
} // namespace

double relativeLuminance(Rgb c) noexcept {
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

double contrastRatio(Rgb a, Rgb b) noexcept {
    const double la = relativeLuminance(a), lb = relativeLuminance(b);
    const double hi = std::max(la, lb), lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

double minimumRatio(ContrastPreset p, double custom) noexcept {
    switch (p) {
        case ContrastPreset::AA:  return 4.5;
        case ContrastPreset::AAA: return 7.0;
        case ContrastPreset::Max: return 21.0;
        case ContrastPreset::Custom: break;
    }
    return std::clamp(custom, 1.0, 21.0);
}

std::optional<ContrastPreset> parsePreset(std::string_view s) noexcept {
    std::string lower;
    for (char ch : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    if (lower == "aa") return ContrastPreset::AA;
    if (lower == "aaa") return ContrastPreset::AAA;
    if (lower == "max") return ContrastPreset::Max;
    if (lower == "custom") return ContrastPreset::Custom;
    return std::nullopt;
}

Rgb enforceContrast(Rgb fg, Rgb bg, double minRatio) noexcept {
    if (contrastRatio(fg, bg) >= minRatio) return fg;
    const Rgb black{0, 0, 0}, white{255, 255, 255};
    // Pick the extreme that gives the most contrast against bg.
    const Rgb target = contrastRatio(black, bg) >= contrastRatio(white, bg) ? black : white;
    if (contrastRatio(target, bg) < minRatio) return target; // best achievable
    double lo = 0.0, hi = 1.0;                               // binary search on mix factor
    for (int i = 0; i < 20; ++i) {
        const double mid = (lo + hi) / 2;
        (contrastRatio(mix(fg, target, mid), bg) >= minRatio ? hi : lo) = mid;
    }
    Rgb out = mix(fg, target, hi);
    if (contrastRatio(out, bg) < minRatio) out = target;     // guard rounding
    return out;
}

} // namespace lmw::ui
