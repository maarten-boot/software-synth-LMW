// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.7: whole-instrument presets (Phase 17, user 2026-11-20: option B, the same names as the module
// presets, voice count included). A preset is a whole sound: values for the parameters it names (every
// other parameter goes to its default) and its cables (none: everything normalled). Most are built from
// the module presets of the same name (ENV 1 and ENV 2, VCF 1, VCO 1; S7.5), so the panels then show those
// names, plus what makes the instrument: voices, mixer, vibrato, breath noise, comb body, reverb.
// Pure C++ (no JUCE).
#pragma once
#include "app/ModulePresets.h"
#include <string>
#include <utility>
#include <vector>

namespace lmw::instruments {

struct Instrument {
    std::string name;
    std::vector<std::pair<std::string, std::string>> values;   // parameter id -> value text (as in patch files)
    std::string cables;                                         // cable text (PatchText.h); empty = none
};

namespace detail {
// The values of module preset `preset` of `kind`, for the module `prefix` ("env1", "vcf1", ...).
inline void addModule(Instrument& i, std::string_view prefix, presets::Kind kind, std::string_view preset) {
    for (const auto& p : presets::builtIn(kind))
        if (p.name == preset)
            for (const auto& [key, value] : p.values) i.values.emplace_back(std::string(prefix) + "." + key, value);
}
// An instrument from the module presets of its name (ENV 1 and ENV 2, VCF 1, VCO 1), then `extra` values.
inline Instrument fromModules(const std::string& name, std::vector<std::pair<std::string, std::string>> extra,
                              std::string cables = {}) {
    Instrument i{name, {}, std::move(cables)};
    addModule(i, "env1", presets::Kind::Env, name);
    addModule(i, "env2", presets::Kind::Env, name);
    addModule(i, "vcf1", presets::Kind::Vcf, name);
    addModule(i, "vco1", presets::Kind::Vco, name);
    for (auto& v : extra) i.values.push_back(std::move(v));
    return i;
}
// The comb preset of that name (S7.5, S7.6): the wind and guitar bodies.
inline std::vector<std::pair<std::string, std::string>> comb(std::string_view preset) {
    Instrument i;
    addModule(i, "comb", presets::Kind::Comb, preset);
    return i.values;
}
// The ensemble preset of that name (S7.11): the string-machine chorus.
inline std::vector<std::pair<std::string, std::string>> ensemble(std::string_view preset) {
    Instrument i;
    addModule(i, "ensemble", presets::Kind::Ensemble, preset);
    return i.values;
}
// The reverb preset of that name (S7.5).
inline std::vector<std::pair<std::string, std::string>> reverb(std::string_view preset) {
    Instrument i;
    addModule(i, "reverb", presets::Kind::Reverb, preset);
    return i.values;
}
inline std::vector<std::pair<std::string, std::string>> join(std::vector<std::pair<std::string, std::string>> a,
                                                             const std::vector<std::pair<std::string, std::string>>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
} // namespace detail

// Built-in instruments, in menu order. Starting points, not models of the real instruments.
inline const std::vector<Instrument>& builtIn() {
    using detail::fromModules;
    using detail::join;
    using detail::reverb;
    using detail::ensemble;
    using detail::comb;
    static const std::vector<Instrument> list = [] {
        const auto solo = std::pair<std::string, std::string>{"voice.count", "1"};
        // S5.25: one voice stays centred; with more voices (a "choral" flute, two hands) the notes spread.
        const auto soloSpread = std::pair<std::string, std::string>{"out.spread", "0.4"};
        const auto byVoices = std::pair<std::string, std::string>{"out.spreadmode", "Voices"};
        const auto byKeys = std::pair<std::string, std::string>{"out.spreadmode", "Keys"};
        const auto vibrato = std::pair<std::string, std::string>{"lfo1.rate", "5.5"};
        const auto delayed = std::pair<std::string, std::string>{"lfo1.delay", "0.4"};   // S5.24: vibrato after the attack
        std::vector<Instrument> v;
        // Winds: one voice; breath noise; a comb body that follows the notes (the COMB preset of the name: Key 100 %, from C4).
        v.push_back(fromModules("Flute", join({solo, soloSpread, byVoices, vibrato, delayed, {"mix.noise", "0.06"}, {"noise.colour", "Pink"}},
                                              join(comb("Flute"), reverb("Chamber")))));
        v.push_back(fromModules("Recorder", join({solo, soloSpread, byVoices, {"mix.noise", "0.04"}, {"noise.colour", "Pink"}}, reverb("Small room"))));
        v.push_back(fromModules("Oboe", join({solo, soloSpread, byVoices, vibrato, delayed}, join(comb("Oboe"), reverb("Chamber")))));
        v.push_back(fromModules("Clarinet", join({solo, soloSpread, byVoices, vibrato, delayed}, join(comb("Clarinet"), reverb("Chamber")))));
        v.push_back(fromModules("Bassoon", join({solo, soloSpread, byVoices, vibrato, delayed}, join(comb("Bassoon"), reverb("Chamber")))));
        // Brass: one voice; VCO 2 slightly detuned for width.
        v.push_back(fromModules("Trumpet", join({solo, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.tune", "0.06"}, {"mix.vco2", "0.5"}},
                                                reverb("Concert hall"))));
        v.push_back(fromModules("French horn", join({solo, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.octave", "-1"},
                                                     {"vco2.tune", "0.04"}, {"mix.vco2", "0.4"}}, reverb("Concert hall"))));
        // Strings.
        v.push_back(fromModules("Violin", join({solo, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.tune", "0.05"}, {"mix.vco2", "0.4"}},
                                               reverb("Concert hall"))));
        v.push_back(fromModules("String section", join({{"voice.count", "12"}, {"out.spread", "0.5"}, byVoices, {"lfo1.rate", "5"}, {"vco1.fm", "0.12"},
                                                        {"vco2.wave", "Saw"}, {"vco2.tune", "0.1"}, {"vco2.fm", "0.12"}, {"mix.vco2", "0.8"},
                                                        {"vco3.wave", "Saw"}, {"vco3.tune", "-0.08"}, {"vco3.fm", "0.12"}, {"mix.vco3", "0.6"}},
                                                       join(ensemble("Solina"), reverb("Concert hall")))));   // S7.11
        // Keyboards and plucked.
        v.push_back(fromModules("Organ", join({{"voice.count", "16"}, {"vco2.wave", "Sine"}, {"vco2.octave", "+1"}, {"mix.vco2", "0.5"},
                                               {"vco3.wave", "Sine"}, {"vco3.octave", "-1"}, {"mix.vco3", "0.5"},
                                               {"lfo2.rate", "6"}, {"vca.tremolo", "0.15"}}, reverb("Cathedral"))));
        v.push_back(fromModules("Piano", join({{"voice.count", "16"}, {"out.spread", "0.4"}, byKeys, {"vco2.wave", "Triangle"}, {"vco2.tune", "0.03"},
                                               {"mix.vco2", "0.5"}}, reverb("Small room"))));
        v.push_back(fromModules("Harpsichord", join({{"voice.count", "12"}, {"out.spread", "0.4"}, byKeys, {"vco2.wave", "Square"}, {"vco2.octave", "+1"},
                                                     {"mix.vco2", "0.3"}}, reverb("Chamber"))));
        v.push_back(fromModules("Guitar (plucked)", join({{"voice.count", "6"}}, join(comb("Guitar (plucked)"), reverb("Ambience")))));
        v.push_back(fromModules("Marimba", join({{"voice.count", "8"}, {"out.spread", "0.4"}, byKeys}, reverb("Small room"))));
        v.push_back(fromModules("Pad (slow)", join({{"voice.count", "12"}, {"out.spread", "0.5"}, byVoices, {"lfo1.rate", "0.3"}, {"vco1.fm", "0.1"},
                                                    {"vco2.wave", "Saw"}, {"vco2.tune", "0.12"}, {"mix.vco2", "0.8"},
                                                    {"lfo2.rate", "0.2"}, {"vca.tremolo", "0.1"}},
                                                   join(ensemble("Solina"), reverb("Concert hall")))));   // S7.11
        // Beyond the module-preset names (user 2026-11-20/21): voices (FORMANT, S5.16) and FM (PM, S5.20).
        v.push_back({"Choir (aah)", join({{"voice.count", "12"}, {"out.spread", "0.5"}, byVoices,
                                          {"vco1.wave", "Pulse"}, {"vco1.fm", "0.1"}, {"vco2.wave", "Saw"}, {"vco2.tune", "0.1"},
                                          {"vco2.fm", "0.1"}, {"mix.vco2", "0.7"}, {"lfo1.rate", "5"},
                                          {"env1.attack", "0.35"}, {"env1.decay", "0.3"}, {"env1.sustain", "0.9"}, {"env1.release", "0.9"},
                                          {"formant.vowel", "0"}, {"formant.voice", "Alto"}, {"formant.res", "0.6"}, {"formant.mix", "1"}},
                                         reverb("Concert hall")), {}});
        v.push_back({"Male voice (ooo)", join({solo, soloSpread, byVoices, {"vco1.wave", "Pulse"}, {"vco1.octave", "-1"}, {"vco1.fm", "0.12"}, {"lfo1.rate", "5.2"}, {"lfo1.delay", "0.4"},
                                               {"env1.attack", "0.15"}, {"env1.decay", "0.2"}, {"env1.sustain", "0.9"}, {"env1.release", "0.4"},
                                               {"formant.vowel", "4"}, {"formant.voice", "Baritone"}, {"formant.res", "0.6"},
                                               {"formant.mix", "1"}}, reverb("Chamber")), {}});
        v.push_back({"Violin (PM vibrato)", join({solo, soloSpread, byVoices, {"vco1.wave", "Saw"}, {"vco1.pm", "0.6"}, {"vco2.wave", "Saw"},
                                                  {"vco2.tune", "0.05"}, {"vco2.pm", "0.6"}, {"mix.vco2", "0.5"}, {"lfo1.rate", "5.5"},
                                                  {"env1.attack", "0.15"}, {"env1.decay", "0.3"}, {"env1.sustain", "0.9"}, {"env1.release", "0.3"},
                                                  {"vcf1.cutoff", "3000"}, {"vcf1.res", "0.2"}, {"vcf1.slope", "12 dB"}, {"vcf1.key", "0.5"}},
                                                 reverb("Concert hall")),
                     "vco1.pm=lfo1.out;vco2.pm=lfo1.out;vco3.pm=lfo1.out"});
        v.push_back({"Electric piano (FM)", join({{"voice.count", "12"}, {"vco1.wave", "Sine"}, {"vco1.pm", "1.5"},
                                                  {"vco2.wave", "Sine"}, {"mix.vco2", "0"},
                                                  {"env1.attack", "0.002"}, {"env1.decay", "2.5"}, {"env1.sustain", "0"}, {"env1.release", "0.4"}},
                                                 reverb("Small room")),
                     "vco1.pm=vco2.out"});
        // Spec S5.26 (user 2026-11-26: "add choral flute"; the user's way: 6 voices, two hands, three VCOs with
        // different PM): the Flute modules, VCO 1 through VCF 1 to the left, VCO 2 through VCF 2 to the right and VCO 3
        // unfiltered in the centre (MIX 2), each VCO with its own PM from LFO 1; the voices spread too. By ear.
        {
            auto i = fromModules("Choral flute", join({{"voice.count", "6"}, soloSpread, byVoices, vibrato, delayed,
                                                       {"vco1.pm", "0.3"}, {"vco2.wave", "Triangle"}, {"vco2.tune", "0.07"},
                                                       {"vco2.fm", "0.1"}, {"vco2.pm", "0.6"}, {"vco3.wave", "Sine"},
                                                       {"vco3.tune", "-0.05"}, {"vco3.fm", "0.1"}, {"vcf2.cutoff", "2500"},
                                                       {"mix2.level1", "1"}, {"mix2.pan1", "-0.7"}, {"mix2.level2", "0.9"},
                                                       {"mix2.pan2", "0.7"}, {"mix2.level3", "0.6"}, {"mix2.pan3", "0"}},
                                                      join(ensemble("Subtle chorus"), reverb("Concert hall"))));
            i.cables = "vco1.pm=lfo1.out;vco2.pm=lfo1.out;vcf1.in=vco1.out;vcf2.in=vco2.out;"
                       "mix2.in1=vcf1.out;mix2.in2=vcf2.out;mix2.in3=vco3.out";
            v.push_back(std::move(i));
        }
        return v;
    }();
    return list;
}

// Spec S7.7: own instrument presets are patch files in Documents/LMW-Synth Presets/INSTRUMENT.
inline constexpr std::string_view folderName = "INSTRUMENT";

} // namespace lmw::instruments
