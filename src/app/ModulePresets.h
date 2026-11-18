// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.5: named presets per module (user 2026-11-18) — starting points for one ENV, VCF, VCO or the
// REVERB, next to whole-patch files (S7.1). Built-in presets follow classical instruments (flute, recorder,
// oboe, clarinet, ...); the user's own are small text files, one per preset:
//
//   # LMW-Synth module preset
//   format = 1
//   module = env
//   name = My flute
//   attack = 0.08
//   ...
//
// Values are texts as in patch files and 9P (S7.1, S10.10): numbers in the parameter's own unit (seconds,
// Hz, octaves, semitones, 0-1 for %), choices by name. Keys are the parameter names after the module
// ("attack" for env1.attack). Pure C++, no JUCE (T-S7.5).
#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::presets {

enum class Kind { Env, Vcf, Vco, Reverb };

struct Preset {
    std::string name;
    std::vector<std::pair<std::string, std::string>> values;   // key ("attack") -> value text ("0.08")
};

// The module kind of a parameter prefix: env1/env2, vcf1/vcf2, vco1-vco3, reverb.
inline std::optional<Kind> kindOf(std::string_view module) {
    if (module == "env1" || module == "env2") return Kind::Env;
    if (module == "vcf1" || module == "vcf2") return Kind::Vcf;
    if (module == "vco1" || module == "vco2" || module == "vco3") return Kind::Vco;
    if (module == "reverb") return Kind::Reverb;
    return std::nullopt;
}

inline std::string_view kindName(Kind k) {          // "module = ..." in the file
    switch (k) {
        case Kind::Env: return "env";
        case Kind::Vcf: return "vcf";
        case Kind::Vco: return "vco";
        default:        return "reverb";
    }
}

inline std::string_view folderName(Kind k) {        // Documents/LMW-Synth Presets/<folder>
    switch (k) {
        case Kind::Env: return "ENV";
        case Kind::Vcf: return "VCF";
        case Kind::Vco: return "VCO";
        default:        return "REVERB";
    }
}

// The parameters a preset of this kind sets, in panel order.
inline const std::vector<std::string_view>& keys(Kind k) {
    static const std::vector<std::string_view> env{"attack", "decay", "sustain", "release"};
    static const std::vector<std::string_view> vcf{"cutoff", "res", "env", "key", "slope", "type"};
    static const std::vector<std::string_view> vco{"wave", "octave", "tune", "fm"};
    static const std::vector<std::string_view> reverb{"mix", "size", "damping", "width"};
    switch (k) {
        case Kind::Env: return env;
        case Kind::Vcf: return vcf;
        case Kind::Vco: return vco;
        default:        return reverb;
    }
}

namespace detail {
inline Preset make(std::string name, Kind k, std::vector<std::string> values) {
    Preset p{std::move(name), {}};
    const auto& ks = keys(k);
    for (std::size_t i = 0; i < ks.size() && i < values.size(); ++i) p.values.emplace_back(std::string(ks[i]), values[i]);
    return p;
}
} // namespace detail

// Built-in presets: approximate starting points, not models of the real instruments. The panel menu also
// offers "Default" (the parameters' defaults), which is not in this list.
inline const std::vector<Preset>& builtIn(Kind k) {
    using detail::make;
    // attack s, decay s, sustain 0-1, release s
    static const std::vector<Preset> env{
        make("Flute", Kind::Env, {"0.08", "0.3", "0.85", "0.15"}),
        make("Recorder", Kind::Env, {"0.03", "0.15", "0.9", "0.08"}),
        make("Oboe", Kind::Env, {"0.04", "0.2", "0.85", "0.1"}),
        make("Clarinet", Kind::Env, {"0.05", "0.15", "0.9", "0.12"}),
        make("Bassoon", Kind::Env, {"0.06", "0.2", "0.85", "0.12"}),
        make("Trumpet", Kind::Env, {"0.04", "0.3", "0.75", "0.15"}),
        make("French horn", Kind::Env, {"0.1", "0.4", "0.8", "0.3"}),
        make("Violin", Kind::Env, {"0.15", "0.3", "0.9", "0.3"}),
        make("String section", Kind::Env, {"0.4", "0.5", "0.9", "0.8"}),
        make("Organ", Kind::Env, {"0.005", "0.01", "1", "0.02"}),
        make("Piano", Kind::Env, {"0.002", "3", "0", "0.5"}),
        make("Harpsichord", Kind::Env, {"0.001", "1.5", "0", "0.2"}),
        make("Guitar (plucked)", Kind::Env, {"0.002", "2", "0", "0.3"}),
        make("Marimba", Kind::Env, {"0.001", "0.6", "0", "0.4"}),
        make("Pad (slow)", Kind::Env, {"1.5", "1", "0.8", "3"}),
    };
    // cutoff Hz, resonance 0-1, env octaves, key tracking 0-1, slope, type
    static const std::vector<Preset> vcf{
        make("Flute", Kind::Vcf, {"2500", "0.05", "0.5", "0.5", "12 dB", "Lowpass"}),
        make("Recorder", Kind::Vcf, {"1800", "0", "0.3", "0.5", "12 dB", "Lowpass"}),
        make("Oboe", Kind::Vcf, {"1300", "0.5", "0.5", "0.3", "12 dB", "Bandpass"}),
        make("Clarinet", Kind::Vcf, {"1600", "0.15", "0.5", "0.5", "24 dB", "Lowpass"}),
        make("Bassoon", Kind::Vcf, {"700", "0.3", "0.5", "0.3", "12 dB", "Bandpass"}),
        make("Trumpet", Kind::Vcf, {"900", "0.2", "2.5", "0.5", "24 dB", "Lowpass"}),
        make("French horn", Kind::Vcf, {"500", "0.1", "1.5", "0.5", "24 dB", "Lowpass"}),
        make("Violin", Kind::Vcf, {"3000", "0.2", "0.5", "0.5", "12 dB", "Lowpass"}),
        make("String section", Kind::Vcf, {"4000", "0.1", "0.3", "0.3", "12 dB", "Lowpass"}),
        make("Organ", Kind::Vcf, {"6000", "0", "0", "0.5", "12 dB", "Lowpass"}),
        make("Piano", Kind::Vcf, {"1500", "0.05", "2", "0.6", "12 dB", "Lowpass"}),
        make("Harpsichord", Kind::Vcf, {"2500", "0.2", "2.5", "0.5", "24 dB", "Lowpass"}),
        make("Guitar (plucked)", Kind::Vcf, {"1200", "0.1", "2.5", "0.5", "24 dB", "Lowpass"}),
        make("Marimba", Kind::Vcf, {"800", "0.3", "2", "0.6", "24 dB", "Lowpass"}),
        make("Pad (slow)", Kind::Vcf, {"1200", "0.3", "1", "0.3", "24 dB", "Lowpass"}),
        make("Acid bass", Kind::Vcf, {"300", "0.75", "3.5", "0.3", "24 dB", "Lowpass"}),
        make("Telephone", Kind::Vcf, {"1000", "0.2", "0", "0", "12 dB", "Bandpass"}),
    };
    // wave, octave, tune semitones, FM (vibrato depth from LFO 1) semitones
    static const std::vector<Preset> vco{
        make("Flute", Kind::Vco, {"Sine", "0", "0", "0.15"}),
        make("Piccolo", Kind::Vco, {"Sine", "+1", "0", "0.15"}),
        make("Recorder", Kind::Vco, {"Triangle", "0", "0", "0"}),
        make("Oboe", Kind::Vco, {"Saw", "0", "0", "0.1"}),
        make("Clarinet", Kind::Vco, {"Square", "0", "0", "0.05"}),
        make("Bassoon", Kind::Vco, {"Saw", "-1", "0", "0.05"}),
        make("Trumpet", Kind::Vco, {"Saw", "0", "0", "0.08"}),
        make("French horn", Kind::Vco, {"Saw", "-1", "0", "0.05"}),
        make("Violin", Kind::Vco, {"Saw", "0", "0", "0.25"}),
        make("Cello", Kind::Vco, {"Saw", "-1", "0", "0.2"}),
        make("Organ", Kind::Vco, {"Sine", "0", "0", "0"}),
        make("Organ 16'", Kind::Vco, {"Sine", "-1", "0", "0"}),
        make("Detuned +8 cents", Kind::Vco, {"Saw", "0", "0.08", "0"}),
        make("Fifth above", Kind::Vco, {"Saw", "0", "7", "0"}),
        make("Sub bass", Kind::Vco, {"Square", "-2", "0", "0"}),
    };
    // mix 0-1, size 0-1, damping 0-1, width 0-1
    static const std::vector<Preset> reverb{
        make("Small room", Kind::Reverb, {"0.15", "0.3", "0.6", "0.7"}),
        make("Ambience", Kind::Reverb, {"0.1", "0.4", "0.7", "0.5"}),
        make("Chamber", Kind::Reverb, {"0.2", "0.5", "0.5", "1"}),
        make("Concert hall", Kind::Reverb, {"0.25", "0.8", "0.4", "1"}),
        make("Cathedral", Kind::Reverb, {"0.35", "0.95", "0.3", "1"}),
        make("Plate", Kind::Reverb, {"0.25", "0.6", "0.1", "1"}),
    };
    switch (k) {
        case Kind::Env: return env;
        case Kind::Vcf: return vcf;
        case Kind::Vco: return vco;
        default:        return reverb;
    }
}

// A preset's file name (without extension): letters, digits, space, '-', '_', '(', ')', '\'' and '+'
// kept, anything else '_'; trimmed; "Preset" when nothing is left.
inline std::string fileName(std::string_view name) {
    std::string out;
    for (char c : name) {
        const auto u = static_cast<unsigned char>(c);
        out += (std::isalnum(u) != 0 || u >= 0x80 || std::string_view(" -_()'+").find(c) != std::string_view::npos) ? c : '_';
    }
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return "Preset";
    out = out.substr(first, out.find_last_not_of(' ') - first + 1);
    return out;
}

inline std::string write(Kind k, const Preset& p) {
    std::string out = "# LMW-Synth module preset (hand-editable; values as in patch files)\nformat = 1\nmodule = ";
    out += kindName(k);
    out += "\nname = " + p.name + "\n";
    for (const auto& [key, value] : p.values) out += key + " = " + value + "\n";
    return out;
}

// Reads a preset file of kind k. Refused (nullopt, *error set): no "format = 1", or another module kind.
// Ignored: '#' comments, blank lines, CR, unknown keys. Keys missing from the file leave those parameters
// as they are. Without a name line the name is empty (the caller uses the file name).
inline std::optional<Preset> parse(Kind k, std::string_view text, std::string* error = nullptr) {
    auto fail = [error](const char* why) { if (error != nullptr) *error = why; return std::optional<Preset>(); };
    auto trim = [](std::string_view s) {
        const auto a = s.find_first_not_of(" \t\r");
        if (a == std::string_view::npos) return std::string_view{};
        return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    };
    Preset p;
    bool format = false, module = false;
    const auto& ks = keys(k);
    while (!text.empty()) {
        const auto nl = text.find('\n');
        const auto line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (line.empty() || line.front() == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const auto key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        if (key == "format") {
            if (value != "1") return fail("This preset was made by a newer LMW-Synth (format is not 1).");
            format = true;
        } else if (key == "module") {
            if (value != kindName(k)) return fail("This preset is for another kind of module.");
            module = true;
        } else if (key == "name") {
            p.name = std::string(value);
        } else if (std::find(ks.begin(), ks.end(), key) != ks.end()) {
            const auto it = std::find_if(p.values.begin(), p.values.end(), [&](const auto& kv) { return kv.first == key; });
            if (it != p.values.end()) it->second = std::string(value);   // the last one counts
            else p.values.emplace_back(std::string(key), std::string(value));
        }
    }
    if (!format) return fail("Not an LMW-Synth module preset (no \"format = 1\" line).");
    if (!module) return fail("The preset does not say which module it is for (no \"module =\" line).");
    return p;
}

} // namespace lmw::presets
