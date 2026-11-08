// SPDX-License-Identifier: AGPL-3.0-or-later
// Saved-state translation (Spec S5.10): state written before VCO 1–3 (no "stateVersion" property,
// version 1) used single-VCO names. Pure C++ so it can be unit-tested.
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace lmw::migration {

inline constexpr int currentStateVersion = 2;

// Version 1 parameter ID -> current ID (unchanged IDs are returned as they are).
inline std::string_view parameterIdFromV1(std::string_view id) noexcept {
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> map{{
        {"vco.wave", "vco1.wave"}, {"vco.fm", "vco1.fm"}, {"mix.vco", "mix.vco1"}}};
    for (const auto& [from, to] : map)
        if (id == from) return to;
    return id;
}

// Version 1 port name -> current name. mix.in2 was the noise input; it is mix.in4 now.
inline std::string_view portNameFromV1(std::string_view name) noexcept {
    constexpr std::array<std::pair<std::string_view, std::string_view>, 4> map{{
        {"vco.out", "vco1.out"}, {"vco.pitch", "vco1.pitch"}, {"vco.fm", "vco1.fm"}, {"mix.in2", "mix.in4"}}};
    for (const auto& [from, to] : map)
        if (name == from) return to;
    return name;
}

// Translates every port name in a version 1 cable list ("in=out;in=out"); the result is parsed by
// patchFromText as usual (which rejects anything malformed).
inline std::string cablesFromV1(std::string_view text) {
    std::string result;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find_first_of("=;", start);
        if (end == std::string_view::npos) end = text.size();
        result += portNameFromV1(text.substr(start, end - start));
        if (end < text.size()) result += text[end];
        start = end + 1;
    }
    return result;
}

} // namespace lmw::migration
