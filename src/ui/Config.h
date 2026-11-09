// SPDX-License-Identifier: AGPL-3.0-or-later
// User config file (Spec S6.3, S6.4): plain "key = value" lines, '#' starts a comment.
//   contrast       = AA | AAA | Max | Custom
//   contrast_ratio = 4.5 .. 21          (used with Custom)
//   compact        = true | false       (default view for a new window; a saved state overrides it)
// Unknown keys and unreadable values are ignored (the default is kept). Pure C++, no JUCE.
#pragma once
#include "ui/Contrast.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace lmw::ui {

struct Settings {
    ContrastPreset contrast = ContrastPreset::AA;
    double customRatio = 7.0;
    std::optional<bool> compact;

    double minRatio() const noexcept { return minimumRatio(contrast, customRatio); }
};

namespace detail {
inline std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}
inline std::string lower(std::string_view s) {
    std::string r;
    for (char c : s) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return r;
}
} // namespace detail

inline Settings parseConfig(std::string_view text) {
    Settings s;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (const auto hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const auto key = detail::lower(detail::trim(line.substr(0, eq)));
        const auto value = detail::trim(line.substr(eq + 1));
        if (key == "contrast") {
            if (auto p = parsePreset(value)) s.contrast = *p;
        } else if (key == "contrast_ratio") {
            double r = 0;
            const std::string v(value);
            if (std::sscanf(v.c_str(), "%lf", &r) == 1 && r > 0) s.customRatio = minimumRatio(ContrastPreset::Custom, r);
        } else if (key == "compact") {
            const auto v = detail::lower(value);
            if (v == "true" || v == "yes" || v == "1") s.compact = true;
            else if (v == "false" || v == "no" || v == "0") s.compact = false;
        }
    }
    return s;
}

inline std::string writeConfig(const Settings& s) {
    const char* names[] = {"AA", "AAA", "Max", "Custom"};
    char ratio[32];
    std::snprintf(ratio, sizeof ratio, "%.2f", s.customRatio);
    std::string out =
        "# LMW-Synth settings. Edit while LMW-Synth is closed, or reopen its window afterwards.\n"
        "# contrast: AA (4.5:1), AAA (7:1), Max (21:1) or Custom (uses contrast_ratio, 4.5 .. 21)\n";
    out += "contrast = " + std::string(names[static_cast<int>(s.contrast)]) + "\n";
    out += "contrast_ratio = " + std::string(ratio) + "\n";
    if (s.compact) out += std::string("# compact: default view for a new window (a saved state overrides it)\ncompact = ")
                        + (*s.compact ? "true" : "false") + "\n";
    return out;
}

} // namespace lmw::ui
