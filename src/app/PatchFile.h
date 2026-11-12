// SPDX-License-Identifier: AGPL-3.0-or-later
// Patch files (Spec S7): plain, hand-editable text.
//
//   # LMW-Synth patch
//   format = 1
//   state_version = 3
//   name = Fat bass
//
//   [parameters]
//   vcf1.cutoff = 1800        # 1.80 kHz      ('#' starts a comment outside [notes])
//   vco1.wave = Saw
//
//   [cables]
//   vco1.fm = env2.out@0.5+lfo1.out   # input = pins (output[@level], summed), as in the saved state (PatchText.h, S5.14)
//
//   [notes]
//   Free text, kept exactly - also lines that start with '#' or '['. Always the last section.
//
// Pure C++ (no JUCE): the processor turns parameters into text and back.
#pragma once
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::patchfile {

inline constexpr int currentFormat = 1;

struct Parameter {
    std::string id, value, comment;   // comment: written after '#', ignored when reading
};

struct Patch {
    int format = currentFormat;
    int stateVersion = 0;             // parameter / port names of this state version (StateMigration.h)
    std::string name;
    std::vector<Parameter> parameters;
    std::vector<std::pair<std::string, std::string>> cables;   // input, output
    std::string notes;
};

namespace detail {
inline std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}
inline int toInt(std::string_view s, int fallback) {
    int v = 0;
    bool any = false;
    for (char c : s) {
        if (c < '0' || c > '9') return fallback;
        v = v * 10 + (c - '0');
        any = true;
        if (v > 1000000) return fallback;
    }
    return any ? v : fallback;
}
} // namespace detail

inline std::string write(const Patch& p) {
    std::string out = "# LMW-Synth patch\n";
    out += "format = " + std::to_string(p.format) + "\n";
    out += "state_version = " + std::to_string(p.stateVersion) + "\n";
    // A name is one line; line breaks would end it early.
    std::string name;
    for (char c : p.name) name += (c == '\n' || c == '\r') ? ' ' : c;
    out += "name = " + name + "\n\n[parameters]\n";
    for (const auto& prm : p.parameters) {
        out += prm.id + " = " + prm.value;
        if (!prm.comment.empty()) out += "    # " + prm.comment;
        out += "\n";
    }
    out += "\n[cables]\n";
    for (const auto& [in, from] : p.cables) out += in + " = " + from + "\n";
    out += "\n[notes]\n" + p.notes;
    return out;
}

// Reads what it understands; returns nothing if this is not an LMW-Synth patch (no "format" line
// before the first section) or its format is newer than this program.
inline std::optional<Patch> parse(std::string_view text) {
    Patch p;
    p.format = 0;
    enum class Section { Header, Parameters, Cables } section = Section::Header;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        std::string_view raw = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
        const auto line = detail::trim(raw);
        if (line == "[notes]") {                       // the rest of the file, verbatim
            p.notes = std::string(text);
            break;
        }
        if (line == "[parameters]") { section = Section::Parameters; continue; }
        if (line == "[cables]")     { section = Section::Cables; continue; }
        std::string_view content = line;
        if (const auto hash = content.find('#'); hash != std::string_view::npos) content = content.substr(0, hash);
        const auto eq = content.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string key(detail::trim(content.substr(0, eq)));
        const std::string value(detail::trim(content.substr(eq + 1)));
        if (key.empty()) continue;
        switch (section) {
            case Section::Header:
                if (key == "format") p.format = detail::toInt(value, -1);
                else if (key == "state_version") p.stateVersion = detail::toInt(value, 0);
                else if (key == "name") {
                    // The name may contain '#': take it from the raw line.
                    const auto rawEq = raw.find('=');
                    p.name = std::string(detail::trim(raw.substr(rawEq + 1)));
                }
                break;
            case Section::Parameters: p.parameters.push_back({key, value, {}}); break;
            case Section::Cables:     p.cables.emplace_back(key, value); break;
        }
    }
    if (p.format < 1 || p.format > currentFormat) return std::nullopt;
    return p;
}

} // namespace lmw::patchfile
