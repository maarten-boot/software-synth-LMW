// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S5.4 / S6.6: cables as text, for the saved state, patch files (S7.1) and 9P (S10.10):
// "vco1.fm=env2.out;vcf1.cutoff=env2.out@0.5+lfo1.out" — per patched input its pins (Spec S5.14), in
// input order, ';'-separated; "" = no cables.
// Port names are the 9P-style names from Patch.h. Not used on the audio thread.
#pragma once
#include "dsp/Patch.h"
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

// One input's pins: "env2.out", "env2.out@0.5+lfo1.out@0.25" (Spec S5.14). A level of 1 is written
// without "@"; levels with up to 4 significant digits.
inline std::string pinsToText(const Patch& p, In in) {
    std::string s;
    for (int k = 0; k < p.pinCount(in); ++k) {
        const auto& pin = p.pin(in, k);
        if (k > 0) s += '+';
        s += outputNames[static_cast<std::size_t>(pin.source)];
        if (pin.level < 1.0f) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "@%.4g", static_cast<double>(pin.level));
            s += buf;
        }
    }
    return s;
}

namespace detail {
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}
// A level "0.5", ".25", "1", "50%" (percent): > 0 and <= 1; locale-independent.
inline std::optional<float> parseLevel(std::string_view s) {
    bool percent = false;
    if (!s.empty() && s.back() == '%') { percent = true; s.remove_suffix(1); }
    s = trim(s);
    if (s.empty()) return std::nullopt;
    double v = 0.0, scale = 1.0;
    bool dot = false, digits = false;
    for (char c : s) {
        if (c == '.' && !dot) { dot = true; continue; }
        if (c < '0' || c > '9') return std::nullopt;
        digits = true;
        if (dot) { scale /= 10.0; v += (c - '0') * scale; } else { v = v * 10.0 + (c - '0'); }
    }
    if (!digits) return std::nullopt;
    if (percent) v /= 100.0;
    if (!(v > 0.0) || v > 1.0) return std::nullopt;
    return static_cast<float>(v);
}
}

// Parses one input's pins ("out[@level]+out[@level]..."; spaces allowed around parts). nullopt on an
// unknown output, a bad level, an output twice or more than Patch::maxPins pins. "" = no pins.
inline std::optional<std::vector<Patch::Pin>> pinsFromText(std::string_view text) {
    std::vector<Patch::Pin> pins;
    text = detail::trim(text);
    if (text.empty()) return pins;
    while (true) {
        const auto plus = text.find('+');
        auto part = detail::trim(text.substr(0, plus));
        float level = 1.0f;
        if (const auto at = part.find('@'); at != std::string_view::npos) {
            const auto l = detail::parseLevel(part.substr(at + 1));
            if (!l) return std::nullopt;
            level = *l;
            part = detail::trim(part.substr(0, at));
        }
        const auto out = outputByName(part);
        if (!out) return std::nullopt;
        for (const auto& q : pins) if (q.source == static_cast<std::int8_t>(*out)) return std::nullopt;
        if (static_cast<int>(pins.size()) >= Patch::maxPins) return std::nullopt;
        pins.push_back({static_cast<std::int8_t>(*out), level});
        if (plus == std::string_view::npos) break;
        text = text.substr(plus + 1);
    }
    return pins;
}

inline std::string patchToText(const Patch& p) {
    std::string s;
    for (int i = 0; i < numInputs; ++i) {
        const auto in = static_cast<In>(i);
        if (!p.isPatched(in)) continue;
        if (!s.empty()) s += ';';
        s += inputNames[static_cast<std::size_t>(i)];
        s += '=';
        s += pinsToText(p, in);
    }
    return s;
}

// Whole text or nothing: on any error (unknown port, missing '=', bad pins, an input twice) returns
// nullopt. "vcf1.cutoff=env2.out@0.5+lfo1.out;out.in=vcf2.out" (older texts have one output per input).
inline std::optional<Patch> patchFromText(std::string_view text) {
    Patch p;
    while (!text.empty()) {
        const auto end = text.find(';');
        const auto item = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        const auto eq = item.find('=');
        if (eq == std::string_view::npos) return std::nullopt;
        const auto in = inputByName(detail::trim(item.substr(0, eq)));
        const auto pins = pinsFromText(item.substr(eq + 1));
        if (!in || !pins || pins->empty() || p.isPatched(*in)) return std::nullopt;
        for (const auto& q : *pins) p.connect(*in, static_cast<Out>(q.source), q.level);
        if (end != std::string_view::npos && text.empty()) return std::nullopt;   // trailing ';'
    }
    return p;
}

} // namespace lmw::dsp
