// SPDX-License-Identifier: AGPL-3.0-or-later
// Saved-state translation (Spec S5.10, S5.11). The state carries "stateVersion"; older states are
// translated step by step on load:
//   1 -> 2 (VCO 1-3):  vco.* -> vco1.*, mix.vco -> mix.vco1, noise input mix.in2 -> mix.in4
//   2 -> 3 (VCF 1-2):  vcf.* -> vcf1.*
// A state without "stateVersion" is version 1. Pure C++ so it can be unit-tested.
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace lmw::migration {

inline constexpr int currentStateVersion = 3;

namespace detail {
template <std::size_t N>
std::string_view lookup(const std::array<std::pair<std::string_view, std::string_view>, N>& map, std::string_view key) noexcept {
    for (const auto& [from, to] : map)
        if (key == from) return to;
    return key;
}
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 3> params1{{
    {"vco.wave", "vco1.wave"}, {"vco.fm", "vco1.fm"}, {"mix.vco", "mix.vco1"}}};
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 5> params2{{
    {"vcf.cutoff", "vcf1.cutoff"}, {"vcf.res", "vcf1.res"}, {"vcf.env", "vcf1.env"},
    {"vcf.slope", "vcf1.slope"}, {"vcf.type", "vcf1.type"}}};
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 4> ports1{{
    {"vco.out", "vco1.out"}, {"vco.pitch", "vco1.pitch"}, {"vco.fm", "vco1.fm"}, {"mix.in2", "mix.in4"}}};
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 3> ports2{{
    {"vcf.out", "vcf1.out"}, {"vcf.in", "vcf1.in"}, {"vcf.cutoff", "vcf1.cutoff"}}};
}

// Parameter ID saved by state version `from` -> current ID (unchanged IDs are returned as they are).
inline std::string parameterId(int from, std::string_view id) {
    std::string s(id);
    if (from < 2) s = std::string(detail::lookup(detail::params1, s));
    if (from < 3) s = std::string(detail::lookup(detail::params2, s));
    return s;
}

// Port name saved by state version `from` -> current name.
inline std::string portName(int from, std::string_view name) {
    std::string s(name);
    if (from < 2) s = std::string(detail::lookup(detail::ports1, s));
    if (from < 3) s = std::string(detail::lookup(detail::ports2, s));
    return s;
}

// Translates every port name in a cable list ("in=out;in=out") saved by state version `from`;
// the result is parsed by patchFromText as usual (which rejects anything malformed).
inline std::string cables(int from, std::string_view text) {
    std::string result;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find_first_of("=;", start);
        if (end == std::string_view::npos) end = text.size();
        result += portName(from, text.substr(start, end - start));
        if (end < text.size()) result += text[end];
        start = end + 1;
    }
    return result;
}

} // namespace lmw::migration
