// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.9: tutorial lesson presets (Phase 17b, user 2026-11-21: form D, content in
// docs/tutorial/Tutorial.md). A lesson sets its starting sound (a base — "Default" or a built-in instrument —
// plus changes and cables; Lesson 11 keeps the sound) and puts the lesson's steps, taken from the document
// at build time (TutorialText.h, cmake/TutorialText.cmake), into the notes. Pure C++ (no JUCE).
#pragma once
#include "TutorialText.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::tutorial {

struct Lesson {
    int number = 0;
    std::string base;                                               // "Default", an instrument name, or "" (keep the sound)
    std::vector<std::pair<std::string, std::string>> values;        // changes on top of the base
    std::string cables;                                             // cable text; empty = none
};

// The whole document.
inline const std::string& text() {
    static const std::string t = [] {
        std::string s;
        for (const char* part : textParts) s += part;
        return s;
    }();
    return t;
}

// "## Lesson n — <title>" -> "<title>"; empty if there is no such lesson.
inline std::string title(int n, std::string_view doc = text()) {
    const std::string head = "## Lesson " + std::to_string(n) + " ";
    const auto at = doc.find(head);
    if (at == std::string_view::npos) return {};
    const auto lineEnd = doc.find('\n', at);
    auto t = doc.substr(at + head.size(), lineEnd - at - head.size());
    const std::string_view dash = "\xe2\x80\x94 ";                  // the em dash after the number
    if (t.substr(0, dash.size()) == dash) t.remove_prefix(dash.size());
    return std::string(t);
}

// The lesson's section, from its heading up to the next "---" ruler; empty if there is no such lesson.
inline std::string section(int n, std::string_view doc = text()) {
    const std::string head = "## Lesson " + std::to_string(n) + " ";
    const auto at = doc.find(head);
    if (at == std::string_view::npos) return {};
    const auto end = doc.find("\n---", at);
    auto s = doc.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.remove_suffix(1);
    return std::string(s);
}

// Markdown to plain text for the Notes tab: emphasis marks (* and **) and backticks go, "\*" stays a star,
// "## " headings lose their marks, code-fence lines (```) are dropped.
inline std::string plain(std::string_view md) {
    std::string out;
    std::size_t lineStart = 0;
    while (lineStart <= md.size()) {
        auto lineEnd = md.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) lineEnd = md.size();
        auto line = md.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;
        const auto first = line.find_first_not_of(' ');
        if (first != std::string_view::npos && line.substr(first, 3) == "```") continue;
        if (line.substr(0, 3) == "## ") line.remove_prefix(3);
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '*') { out += '*'; ++i; continue; }
            if (line[i] == '*' || line[i] == '`') continue;
            out += line[i];
        }
        if (lineEnd < md.size()) out += '\n';
    }
    return out;
}

// The notes a lesson preset brings (user 2026-11-21: the one exception that replaces the notes).
inline std::string notes(int n) {
    return "Tutorial lesson " + std::to_string(n) + " — from docs/tutorial/Tutorial.md\n\n" + plain(section(n)) + "\n";
}

// The lesson sounds (the table at the end of the document).
inline const std::vector<Lesson>& lessons() {
    static const std::vector<Lesson> list{
        {1, "Default", {}, {}},
        {2, "Default", {{"env1.sustain", "1"}}, {}},
        {3, "Default", {{"vcf1.cutoff", "1000"}, {"vcf1.res", "0.2"}}, {}},
        {4, "Default", {{"vcf1.cutoff", "600"}, {"vcf1.env", "3"}}, {}},
        {5, "Default", {{"vco1.wave", "Sine"}, {"env1.sustain", "1"}}, {}},
        {6, "Default", {{"vcf1.cutoff", "800"}, {"vcf1.res", "0.2"}}, {}},
        {7, "Default", {{"vco1.wave", "Pulse"}, {"vco2.wave", "Saw"}, {"vco2.tune", "0.1"}, {"mix.vco2", "0.7"},
                        {"formant.mix", "1"}, {"formant.voice", "Tenor"}}, {}},
        {8, "Default", {{"vco1.wave", "Sine"}, {"vco2.wave", "Sine"}, {"mix.vco2", "0"}, {"env1.sustain", "1"},
                        {"vco1.pm", "0"}}, "vco1.pm=vco2.out"},
        {9, "Flute", {{"comb.mix", "0"}, {"reverb.mix", "0"}, {"delay.mix", "0"}}, {}},
        {10, "Clarinet", {}, {}},
        {11, "", {}, {}},
    };
    return list;
}

} // namespace lmw::tutorial
