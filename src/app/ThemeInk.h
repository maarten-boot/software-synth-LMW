// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.29 (Phase 48): the text and page colours of the current theme (lmw::ui::theme::current()) as JUCE colours,
// read each time they are used, so a theme change (light <-> dark) reaches every place that used to draw fixed black
// and white. `kFg` (text, lines), `kBg` (page) and `kQuiet` (secondary text: a grey between them, still at least 7:1).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace lmw::app {

struct ThemeInk {
    enum class Which { Fore, Back, Quiet } which;
    juce::Colour colour() const {
        const auto& p = lmw::ui::theme::current();
        lmw::ui::Rgb c = which == Which::Fore ? p.foreground : p.background;
        if (which == Which::Quiet) {                    // 30 % of the way from the text to the page, then >= 7:1
            auto m = [](std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>(a + (b - a) * 0.3 + 0.5); };
            const lmw::ui::Rgb mid{m(p.foreground.r, p.background.r), m(p.foreground.g, p.background.g), m(p.foreground.b, p.background.b)};
            c = lmw::ui::enforceContrast(mid, p.background, std::max(7.0, p.minRatio));
        }
        return juce::Colour(c.r, c.g, c.b);
    }
    operator juce::Colour() const { return colour(); }
    juce::Colour withAlpha(float a) const { return colour().withAlpha(a); }
};

inline constexpr ThemeInk kFg{ThemeInk::Which::Fore}, kBg{ThemeInk::Which::Back}, kQuiet{ThemeInk::Which::Quiet};

inline juce::Colour toJuce(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }

// Spec S6.29 choice C: small texts (tick values, scale tags, socket and port labels) bold in a dark theme, against
// halation (light thin strokes on dark blur together); as before in the light themes.
inline juce::FontOptions smallFont(float size) {
    return lmw::ui::theme::current().dark ? juce::FontOptions(size, juce::Font::bold) : juce::FontOptions(size);
}

// A theme change: every colour a component was given explicitly (setColour, kept by JUCE as a "jcclr_<id>" property)
// that is the old text, page or quiet colour becomes the new one — in every window on the desktop (tabs, dialogs,
// Terms, Assistant), so nothing stays light-on-light or dark-on-dark.
inline void retint(juce::Component& c, const std::array<std::pair<juce::Colour, juce::Colour>, 3>& map) {
    std::vector<std::pair<int, juce::Colour>> changes;
    const auto& props = c.getProperties();
    for (int i = 0; i < props.size(); ++i) {
        const auto name = props.getName(i).toString();
        if (!name.startsWith("jcclr_")) continue;
        const auto old = juce::Colour(static_cast<juce::uint32>(static_cast<juce::int64>(props.getValueAt(i))));
        for (const auto& [from, to] : map)
            if (old == from) { changes.emplace_back(name.substring(6).getHexValue32(), to); break; }
    }
    for (const auto& [id, colour] : changes) c.setColour(id, colour);
    for (auto* child : c.getChildren()) retint(*child, map);
}
inline void retintDesktop(const std::array<std::pair<juce::Colour, juce::Colour>, 3>& map) {
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* top = desktop.getComponent(i)) { retint(*top, map); top->repaint(); }
}

} // namespace lmw::app
