// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S5.4 / S6.6: cables as text, for the saved state, patch files (S7.1) and 9P (S10.10):
// "vco1.fm=env2.out;vcf1.in=vcf1.out" — input=output pairs in input order, ';'-separated; "" = no cables.
// Port names are the 9P-style names from Patch.h. Not used on the audio thread.
#pragma once
#include "dsp/Patch.h"
#include <optional>
#include <string>
#include <string_view>

namespace lmw::dsp {

inline std::optional<In> inputByName(std::string_view name) {
    for (std::size_t i = 0; i < inputNames.size(); ++i)
        if (inputNames[i] == name) return static_cast<In>(i);
    return std::nullopt;
}

inline std::optional<Out> outputByName(std::string_view name) {
    for (std::size_t i = 0; i < outputNames.size(); ++i)
        if (outputNames[i] == name) return static_cast<Out>(i);
    return std::nullopt;
}

inline std::string patchToText(const Patch& p) {
    std::string s;
    for (int i = 0; i < numInputs; ++i) {
        const auto in = static_cast<In>(i);
        if (!p.isPatched(in)) continue;
        if (!s.empty()) s += ';';
        s += inputNames[static_cast<std::size_t>(i)];
        s += '=';
        s += outputNames[static_cast<std::size_t>(p.source(in))];
    }
    return s;
}

// Whole text or nothing: on any error (unknown port, missing '=', an input twice) returns nullopt.
inline std::optional<Patch> patchFromText(std::string_view text) {
    Patch p;
    while (!text.empty()) {
        const auto end = text.find(';');
        const auto item = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        const auto eq = item.find('=');
        if (eq == std::string_view::npos) return std::nullopt;
        const auto in = inputByName(item.substr(0, eq));
        const auto out = outputByName(item.substr(eq + 1));
        if (!in || !out || p.isPatched(*in)) return std::nullopt;
        p.connect(*in, *out);
        if (end != std::string_view::npos && text.empty()) return std::nullopt;   // trailing ';'
    }
    return p;
}

} // namespace lmw::dsp
