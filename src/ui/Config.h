// SPDX-License-Identifier: AGPL-3.0-or-later
// User config file (Spec S6.3, S6.4): plain "key = value" lines; '#' starts a comment at a line's start or
// before a space ("# note"), so a colour "#RRGGBB" is a value.
//   contrast       = AA | AAA | Max | Custom
//   contrast_ratio = 4.5 .. 21          (used with Custom)
//   compact        = true | false       (default view for a new window; a saved state overrides it)
//   ninep_tcp      = [host:]port        (also serve 9P over TCP; empty = off; host default 127.0.0.1)
//   keyboard       = shown | hidden     (on-screen keyboard for a new window, Spec S6.12; a saved state overrides it)
//   theme          = Default | Soft | Vivid | Monochrome | Custom   (Spec S6.13)
//   color_<item>   = #RRGGBB            (Custom: vco, noise, vcf, vca, env, lfo, mixer, effects, background, band, cable)
//   cable_opacity  = 20 .. 100          (Custom: cables at rest, %)
//   theme_name     = <name>             (Custom taken from a saved theme, Documents/LMW-Synth Presets/THEME)
// Unknown keys and unreadable values are ignored (the default is kept). Pure C++, no JUCE.
#pragma once
#include "ui/Contrast.h"
#include "ui/Theme.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <string_view>
#include <utility>

namespace lmw::ui {

struct Settings {
    ContrastPreset contrast = ContrastPreset::AA;
    double customRatio = 7.0;
    std::optional<bool> compact;
    std::optional<bool> keyboard;              // Spec S6.12: shown (true) / hidden
    std::string ninepTcp;                      // Spec S10.4: "" = no TCP
    theme::ThemePreset theme = theme::ThemePreset::Default;   // Spec S6.13
    std::array<std::optional<Rgb>, theme::themeItems.size()> custom{};   // Custom: the user's colours
    std::optional<float> customOpacity;        // Custom: cables at rest, 0.2 .. 1
    std::string themeName;                     // Custom came from this saved theme (unchanged since)
    // Spec S12.6, S12.7: the Assistant tab (standalone). Empty / unset = the defaults.
    std::string assistantStore;                // "keychain" or "file" (where the API key is kept)
    std::string assistantKey;                  // only with "file": plain text (the file is then the user's only)
    std::string assistantModel;                // e.g. claude-haiku-5-5 (empty: the default, S12.3)
    std::string assistantEffort;               // low, medium, high
    std::optional<double> assistantLimit;      // dollars per session, 0 = none (default 2)
    bool assistantNoticeSeen = false;          // S12.8: the privacy notice was accepted once
    std::vector<std::pair<std::string, std::string>> assistantPrices;   // S12.7: price_<model> = in out cache-read [cache-write]

    // The theme's colours (before the contrast is applied): a preset, or Custom = Default with the
    // user's colours on top.
    theme::Colours colours() const noexcept {
        auto c = theme::presetColours(theme);
        if (theme == theme::ThemePreset::Custom)
            for (std::size_t i = 0; i < custom.size(); ++i)
                if (custom[i]) theme::itemColour(c, i) = *custom[i];
        if (theme == theme::ThemePreset::Custom && customOpacity) c.cableOpacity = std::clamp(*customOpacity, 0.2f, 1.0f);
        return c;
    }

    // ninep_tcp as host and port; nothing when off or unreadable.
    std::optional<std::pair<std::string, int>> tcpEndpoint() const {
        if (ninepTcp.empty()) return std::nullopt;
        const auto colon = ninepTcp.rfind(':');
        const std::string host = colon == std::string::npos ? "127.0.0.1" : ninepTcp.substr(0, colon);
        const std::string port = colon == std::string::npos ? ninepTcp : ninepTcp.substr(colon + 1);
        int p = 0;
        for (char c : port) { if (c < '0' || c > '9') return std::nullopt; p = p * 10 + (c - '0'); if (p > 65535) return std::nullopt; }
        if (p == 0 || host.empty()) return std::nullopt;
        return std::make_pair(host, p);
    }

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

// "#RRGGBB" (or "RRGGBB") -> colour; anything else nothing.
inline std::optional<Rgb> parseHexColour(std::string_view v) noexcept {
    if (!v.empty() && v.front() == '#') v.remove_prefix(1);
    if (v.size() != 6) return std::nullopt;
    std::uint8_t out[3]{};
    for (int i = 0; i < 3; ++i) {
        int x = 0;
        for (int k = 0; k < 2; ++k) {
            const char c = v[static_cast<std::size_t>(2 * i + k)];
            x *= 16;
            if (c >= '0' && c <= '9') x += c - '0';
            else if (c >= 'a' && c <= 'f') x += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') x += c - 'A' + 10;
            else return std::nullopt;
        }
        out[i] = static_cast<std::uint8_t>(x);
    }
    return Rgb{out[0], out[1], out[2]};
}
inline std::string hexColour(Rgb c) {
    char b[8];
    std::snprintf(b, sizeof b, "#%02X%02X%02X", c.r, c.g, c.b);
    return b;
}

inline Settings parseConfig(std::string_view text) {
    Settings s;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        // '#' starts a comment at the line's start or before a space / the end ("x = 9  # note"), not in a
        // value such as a colour "#FFB3B3" (Spec S6.13).
        for (std::size_t h = line.find('#'); h != std::string_view::npos; h = line.find('#', h + 1)) {
            const bool atStart = detail::trim(line.substr(0, h)).empty();
            const bool spaceAfter = h + 1 >= line.size() || line[h + 1] == ' ' || line[h + 1] == '\t' || line[h + 1] == '\r';
            if (atStart || spaceAfter) { line = line.substr(0, h); break; }
        }
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
        } else if (key == "ninep_tcp") {
            s.ninepTcp = std::string(value);
        } else if (key == "compact") {
            const auto v = detail::lower(value);
            if (v == "true" || v == "yes" || v == "1") s.compact = true;
            else if (v == "false" || v == "no" || v == "0") s.compact = false;
        } else if (key == "theme") {
            if (auto t = theme::parseTheme(value)) s.theme = *t;
        } else if (key.rfind("color_", 0) == 0 || key.rfind("colour_", 0) == 0) {
            const auto item = key.substr(key.find('_') + 1);
            for (std::size_t i = 0; i < theme::themeItems.size(); ++i)
                if (theme::themeItems[i].key == item)
                    if (auto c = parseHexColour(value)) s.custom[i] = *c;
        } else if (key == "cable_opacity") {
            double o = 0;
            const std::string v(value);
            if (std::sscanf(v.c_str(), "%lf", &o) == 1 && o > 0) s.customOpacity = static_cast<float>(std::clamp(o > 1.0 ? o / 100.0 : o, 0.2, 1.0));
        } else if (key == "theme_name") {
            s.themeName = std::string(value);
        } else if (key == "assistant_key_store") {
            const auto v = detail::lower(value);
            if (v == "keychain" || v == "file") s.assistantStore = v;
        } else if (key == "assistant_key") {
            s.assistantKey = std::string(value);
        } else if (key == "assistant_model") {
            s.assistantModel = std::string(value);
        } else if (key == "assistant_effort") {
            const auto v = detail::lower(value);
            if (v == "low" || v == "medium" || v == "high") s.assistantEffort = v;
        } else if (key == "assistant_limit") {
            double d = -1;
            std::string v(value);
            if (!v.empty() && v[0] == '$') v.erase(0, 1);
            if (std::sscanf(v.c_str(), "%lf", &d) == 1 && d >= 0) s.assistantLimit = d;
        } else if (key == "assistant_notice_seen") {
            const auto v = detail::lower(value);
            s.assistantNoticeSeen = v == "true" || v == "yes" || v == "1";
        } else if (key.rfind("price_", 0) == 0 && key.size() > 6 && !value.empty()) {
            s.assistantPrices.emplace_back(key.substr(6), std::string(value));
        } else if (key == "keyboard") {
            const auto v = detail::lower(value);
            if (v == "shown" || v == "show" || v == "true" || v == "yes" || v == "1") s.keyboard = true;
            else if (v == "hidden" || v == "hide" || v == "false" || v == "no" || v == "0") s.keyboard = false;
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
    out += "# ninep_tcp: also serve 9P over TCP, e.g. 5640 or 127.0.0.1:5640 (no authentication: keep it local)\n";
    out += "ninep_tcp = " + s.ninepTcp + "\n";
    if (s.compact) out += std::string("# compact: default view for a new window (a saved state overrides it)\ncompact = ")
                        + (*s.compact ? "true" : "false") + "\n";
    if (s.keyboard) out += std::string("# keyboard: on-screen keyboard shown or hidden in a new window (a saved state overrides it)\nkeyboard = ")
                         + (*s.keyboard ? "shown" : "hidden") + "\n";
    out += "# theme: Default, Soft, Vivid, Monochrome or Custom (Custom uses the color_ lines below; every colour\n"
           "# is still raised to the contrast above)\n";
    out += "theme = " + std::string(theme::themeNames[static_cast<std::size_t>(s.theme)]) + "\n";
    if (s.theme == theme::ThemePreset::Custom) {
        const auto c = s.colours();
        for (std::size_t i = 0; i < theme::themeItems.size(); ++i)
            out += "color_" + std::string(theme::themeItems[i].key) + " = " + hexColour(theme::itemColour(c, i)) + "\n";
        char o[16];
        std::snprintf(o, sizeof o, "%.0f", static_cast<double>(c.cableOpacity) * 100.0);
        out += "cable_opacity = " + std::string(o) + "\n";
        if (!s.themeName.empty()) out += "theme_name = " + s.themeName + "\n";
    }
    if (!s.assistantStore.empty() || !s.assistantModel.empty() || !s.assistantEffort.empty() || s.assistantLimit
        || s.assistantNoticeSeen || !s.assistantPrices.empty()) {
        out += "# Assistant tab (standalone): assistant_key_store keychain (the system keychain) or file (assistant_key\n"
               "# below, plain text); assistant_model; assistant_effort low, medium or high; assistant_limit: dollars\n"
               "# per session before it asks to go on (0 = no limit)\n";
        if (!s.assistantStore.empty()) out += "assistant_key_store = " + s.assistantStore + "\n";
        if (s.assistantStore == "file" && !s.assistantKey.empty()) out += "assistant_key = " + s.assistantKey + "\n";
        if (!s.assistantModel.empty()) out += "assistant_model = " + s.assistantModel + "\n";
        if (!s.assistantEffort.empty()) out += "assistant_effort = " + s.assistantEffort + "\n";
        if (s.assistantLimit) {
            char l[32];
            std::snprintf(l, sizeof l, "%.2f", *s.assistantLimit);
            out += "assistant_limit = " + std::string(l) + "\n";
        }
        if (s.assistantNoticeSeen) out += "assistant_notice_seen = true\n";
        if (!s.assistantPrices.empty())
            out += "# price_<model>: dollars per million tokens: input output cache-read [cache-write], e.g.\n"
                   "# price_claude-opus-5-5 = 4 20 0.2 5 (replaces the built-in price when prices change)\n";
        for (const auto& [model, text] : s.assistantPrices) out += "price_" + model + " = " + text + "\n";
    }
    return out;
}

// Spec S6.13: a saved theme, Documents/LMW-Synth Presets/THEME/<name>.lmwtheme — the Custom lines of
// config.ini (read with parseConfig, which ignores everything else).
inline std::string writeTheme(const std::string& name, const theme::Colours& c) {
    std::string out = "# LMW-Synth theme (hand-editable; as the Custom lines of config.ini)\nformat = 1\nname = " + name + "\n";
    for (std::size_t i = 0; i < theme::themeItems.size(); ++i)
        out += "color_" + std::string(theme::themeItems[i].key) + " = " + hexColour(theme::itemColour(c, i)) + "\n";
    char o[16];
    std::snprintf(o, sizeof o, "%.0f", static_cast<double>(c.cableOpacity) * 100.0);
    return out + "cable_opacity = " + o + "\n";
}
// The colours of a saved theme (Default's for anything the file leaves out).
inline theme::Colours readTheme(std::string_view text) {
    auto s = parseConfig(text);
    s.theme = theme::ThemePreset::Custom;
    return s.colours();
}

} // namespace lmw::ui
