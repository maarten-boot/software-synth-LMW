// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.7: whole-instrument presets (Phase 17, user 2026-11-20: option B, the same names as the module
// presets, voice count included). A preset is a whole sound: values for the parameters it names (every
// other parameter goes to its default) and its cables (none: everything normalled). Most are built from
// the module presets of the same name (ENV 1 and ENV 2, VCF 1, VCO 1; S7.5), so the panels then show those
// names, plus what makes the instrument: voices, mixer, vibrato, breath noise, comb body, reverb.
// Pure C++ (no JUCE).
#pragma once
#include "app/ModulePresets.h"
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::instruments {

struct Instrument {
    std::string name;
    std::vector<std::pair<std::string, std::string>> values;   // parameter id -> value text (as in patch files)
    std::string cables;                                         // cable text (PatchText.h); empty = none
    std::string group = {};                                     // "" = the main list, "World" = Presets > World (S7.13)
    std::string family = {};                                    // S7.16: Presets > <family> (world ones there too)
};
// Spec S7.16: the families of the Presets menu, in menu order (World is its own submenu after them).
inline constexpr std::array<std::string_view, 7> families{"Woodwinds", "Brass", "Strings", "Keyboards", "Plucked & mallets",
                                                          "Voices", "Synth"};

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
// Spec S7.13: an instrument from the module presets of another one (`base`: ENV 1 and ENV 2, VCF 1, VCO 1).
inline Instrument like(const std::string& name, std::string_view base, std::vector<std::pair<std::string, std::string>> extra,
                       std::string cables = {}, std::string group = "World") {
    Instrument i{name, {}, std::move(cables), std::move(group)};
    addModule(i, "env1", presets::Kind::Env, base);
    addModule(i, "env2", presets::Kind::Env, base);
    addModule(i, "vcf1", presets::Kind::Vcf, base);
    addModule(i, "vco1", presets::Kind::Vco, base);
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
// The delay preset of that name (S7.8).
inline std::vector<std::pair<std::string, std::string>> delay(std::string_view preset) {
    Instrument i;
    addModule(i, "delay", presets::Kind::Delay, preset);
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
    using detail::delay;
    static const std::vector<Instrument> list = [] {
        const auto solo = std::pair<std::string, std::string>{"voice.count", "1"};
        // S5.25: one voice stays centred; with more voices (a "choral" flute, two hands) the notes spread.
        const auto soloSpread = std::pair<std::string, std::string>{"out.spread", "0.4"};
        const auto byVoices = std::pair<std::string, std::string>{"out.spreadmode", "Voices"};
        const auto byKeys = std::pair<std::string, std::string>{"out.spreadmode", "Keys"};
        const auto vibrato = std::pair<std::string, std::string>{"lfo1.rate", "5.5"};
        const auto delayed = std::pair<std::string, std::string>{"lfo1.delay", "0.4"};   // S5.24: vibrato after the attack
        // S6.24 (S5.27): a short legato glide on the solo winds and brass, a little longer on the violin (by ear).
        const auto glideShort = std::pair<std::string, std::string>{"keys.glide", "0.04"};
        const auto glideViolin = std::pair<std::string, std::string>{"keys.glide", "0.06"};
        const auto legato = std::pair<std::string, std::string>{"keys.glidemode", "Legato"};
        // S7.16: a little breath under the reeds; velocity brightens (keys.vel added to ENV 2 on the cutoff, scaled by
        // the VCF's Env amount — the cable keeps ENV 2, which it would otherwise replace).
        const auto reedBreath = std::vector<std::pair<std::string, std::string>>{{"mix.noise", "0.03"}, {"noise.colour", "Pink"}};
        const std::string velocityBright = "vcf1.cutoff=env2.out+keys.vel@0.4";
        std::vector<Instrument> v;
        // Winds: one voice; breath noise; a comb body that follows the notes (the COMB preset of the name: Key 100 %, from C4).
        v.push_back(fromModules("Flute", join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed, {"mix.noise", "0.06"}, {"noise.colour", "Pink"}},
                                              join(comb("Flute"), reverb("Chamber")))));
        v.push_back(fromModules("Recorder", join({solo, glideShort, legato, soloSpread, byVoices, {"mix.noise", "0.04"}, {"noise.colour", "Pink"}}, reverb("Small room"))));
        v.push_back(fromModules("Oboe", join(join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed, {"vco1.shape", "0.1"}}, reedBreath),
                                             join(comb("Oboe"), reverb("Chamber")))));   // S7.16: Shape 10 % (a little more nasal), breath
        v.push_back(fromModules("Clarinet", join(join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed, {"vco1.shape", "0.1"}}, reedBreath),
                                                 join(comb("Clarinet"), reverb("Chamber")))));
        v.push_back(fromModules("Bassoon", join(join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed}, reedBreath),
                                                join(comb("Bassoon"), reverb("Chamber")))));
        // Brass: one voice; VCO 2 slightly detuned for width.
        v.push_back(fromModules("Trumpet", join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.tune", "0.06"}, {"mix.vco2", "0.5"}},
                                                reverb("Concert hall")), velocityBright));   // S7.16: harder = brighter
        v.push_back(fromModules("French horn", join({solo, glideShort, legato, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.octave", "-1"},
                                                     {"vco2.tune", "0.04"}, {"mix.vco2", "0.4"}}, reverb("Concert hall")), velocityBright));
        // Strings.
        v.push_back(fromModules("Violin", join({solo, glideViolin, legato, soloSpread, byVoices, vibrato, delayed, {"vco2.wave", "Saw"}, {"vco2.tune", "0.05"}, {"mix.vco2", "0.4"}},
                                               reverb("Concert hall"))));
        v.push_back(fromModules("String section", join({{"voice.count", "12"}, {"out.spread", "0.5"}, byVoices, {"lfo1.rate", "5"}, {"vco1.fm", "0.12"},
                                                        {"vco2.wave", "Saw"}, {"vco2.tune", "0.1"}, {"vco2.fm", "0.12"}, {"mix.vco2", "0.8"},
                                                        {"vco3.wave", "Saw"}, {"vco3.tune", "-0.08"}, {"vco3.fm", "0.12"}, {"mix.vco3", "0.6"},
                                                        {"vco1.shape", "0.15"}, {"vco2.shape", "0.15"}, {"vco3.shape", "0.15"}},   // S7.16: fuller
                                                       join(ensemble("Solina"), reverb("Concert hall")))));   // S7.11
        // Keyboards and plucked.
        v.push_back(fromModules("Organ", join({{"voice.count", "16"}, {"out.spread", "0.4"}, byKeys, {"vco2.wave", "Sine"}, {"vco2.octave", "+1"}, {"mix.vco2", "0.5"},
                                               {"vco3.wave", "Sine"}, {"vco3.octave", "-1"}, {"mix.vco3", "0.5"},
                                               {"lfo2.rate", "6"}, {"vca.tremolo", "0.15"}}, join(ensemble("Subtle chorus"), reverb("Cathedral")))));   // S7.16: chorus
        v.push_back(fromModules("Piano", join({{"voice.count", "16"}, {"out.spread", "0.4"}, byKeys, {"vco2.wave", "Triangle"}, {"vco2.tune", "0.03"},
                                               {"mix.vco2", "0.5"}}, reverb("Small room")), velocityBright));   // S7.16
        v.push_back(fromModules("Harpsichord", join({{"voice.count", "12"}, {"out.spread", "0.4"}, byKeys, {"vco2.wave", "Square"}, {"vco2.octave", "+1"},
                                                     {"mix.vco2", "0.3"}}, reverb("Chamber")), velocityBright));
        v.push_back(fromModules("Guitar (plucked)", join({{"voice.count", "6"}}, join(join(comb("Guitar (plucked)"), delay("Slapback")), reverb("Ambience")))));
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
        // Spec S5.28: pulse-width modulation — two squares at Shape 40 %, LFO 2 moving their widths in opposite
        // directions (the classic PWM string pad).
        v.push_back({"PWM strings", join({{"voice.count", "12"}, {"out.spread", "0.5"}, byVoices, {"vco1.wave", "Square"}, {"vco1.shape", "0.4"},
                                          {"vco2.wave", "Square"}, {"vco2.tune", "0.08"}, {"vco2.shape", "0.4"}, {"mix.vco2", "0.8"},
                                          {"lfo2.rate", "0.35"}, {"lfo1.rate", "5"}, {"vco1.fm", "0.08"}, {"vco2.fm", "0.08"},
                                          {"env1.attack", "0.4"}, {"env1.decay", "0.5"}, {"env1.sustain", "0.85"}, {"env1.release", "1.2"},
                                          {"vcf1.cutoff", "3500"}, {"vcf1.res", "0.15"}},
                                         join(ensemble("Solina"), reverb("Concert hall"))),
                     "vco1.shape=lfo2.out@0.35;vco2.shape=lfo2.out@-0.35"});
        v.push_back({"Electric piano (FM)", join({{"voice.count", "12"}, {"out.spread", "0.4"}, byKeys, {"vco1.wave", "Sine"}, {"vco1.pm", "1.5"},
                                                  {"vco2.wave", "Sine"}, {"mix.vco2", "0"},
                                                  {"env1.attack", "0.002"}, {"env1.decay", "2.5"}, {"env1.sustain", "0"}, {"env1.release", "0.4"},
                                                  {"vcf1.cutoff", "2500"}, {"vcf1.env", "1.5"}, {"vcf1.slope", "12 dB"}},   // S7.16: room for velocity
                                                 join(delay("Slapback"), reverb("Small room"))),
                     "vco1.pm=vco2.out;" + velocityBright});
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
        // Spec S7.16 (Phase 50): Synth presets that show the newer features (by ear, user).
        v.push_back({"Acid bass", join({solo, {"keys.glide", "0.08"}, legato, {"vco1.wave", "Saw"}, {"vco1.octave", "-1"},
                                        {"vcf1.cutoff", "300"}, {"vcf1.res", "0.85"}, {"vcf1.env", "3"}, {"vcf1.slope", "24 dB"},
                                        {"env1.attack", "0.001"}, {"env1.decay", "0.3"}, {"env1.sustain", "0.6"}, {"env1.release", "0.1"},
                                        {"env2.attack", "0.001"}, {"env2.decay", "0.25"}, {"env2.sustain", "0"}, {"env2.release", "0.1"}},
                                       reverb("Ambience")), velocityBright});
        v.push_back({"Filter sine", join({{"voice.count", "8"}, {"mix.vco1", "0"}, {"vcf1.cutoff", "262"}, {"vcf1.res", "1"},
                                          {"vcf1.key", "1"}, {"env1.attack", "0.01"}, {"env1.sustain", "1"}, {"env1.release", "0.4"}},
                                         reverb("Chamber")), {}});   // S6.27 C: the VCF alone sings, at the key's pitch
        v.push_back({"Hollow lead", join({solo, glideShort, legato, vibrato, delayed, {"vco1.wave", "Saw"}, {"vco1.shape", "1"},
                                          {"vco2.wave", "Saw"}, {"vco2.tune", "0.07"}, {"vco2.shape", "0.6"}, {"mix.vco2", "0.5"},
                                          {"vco1.fm", "0.1"}, {"vco2.fm", "0.1"}, {"vcf1.cutoff", "2500"}, {"vcf1.res", "0.2"}},
                                         join(delay("Tape echo"), reverb("Chamber"))), {}});   // S5.28: saw Shape 100 % = hollow
        v.push_back({"Ping-pong pluck", join({{"voice.count", "8"}, {"vco1.wave", "Square"}, {"vco1.shape", "0.3"},
                                              {"env1.attack", "0.001"}, {"env1.decay", "0.4"}, {"env1.sustain", "0"}, {"env1.release", "0.3"},
                                              {"env2.attack", "0.001"}, {"env2.decay", "0.2"}, {"env2.sustain", "0"},
                                              {"vcf1.cutoff", "800"}, {"vcf1.env", "3"}, {"vcf1.res", "0.3"}},
                                             join(delay("Ping-pong"), reverb("Ambience"))), {}});
        v.push_back({"Wobble pad", join({{"voice.count", "8"}, {"out.spread", "0.5"}, byVoices, {"vco1.wave", "Saw"}, {"vco2.wave", "Saw"},
                                         {"vco2.tune", "0.1"}, {"mix.vco2", "0.8"}, {"lfo2.wave", "Triangle"}, {"lfo2.shape", "0.9"},
                                         {"lfo2.rate", "0.5"}, {"vcf1.cutoff", "900"}, {"vcf1.res", "0.5"}, {"vcf1.env", "2"},
                                         {"env1.attack", "0.3"}, {"env1.sustain", "0.9"}, {"env1.release", "1"}},
                                        join(ensemble("Subtle chorus"), reverb("Concert hall"))),
                     "vcf1.cutoff=env2.out+lfo2.out@0.6"});   // S5.28: the LFO on a ramp (Shape 90 %) opens the filter
        v.push_back({"Shimmer space", join({{"voice.count", "8"}, {"out.spread", "0.5"}, byVoices, {"vco1.wave", "Triangle"},
                                            {"vco2.wave", "Sine"}, {"vco2.octave", "+1"}, {"mix.vco2", "0.4"},
                                            {"env1.attack", "0.6"}, {"env1.sustain", "0.8"}, {"env1.release", "2"},
                                            {"effects.order", "reverb, comb, ensemble, delay"},   // S7.14: the comb rings on the reverb
                                            {"comb.freq", "261.6"}, {"comb.key", "1"}, {"comb.feedback", "0.85"}, {"comb.damping", "0.3"},
                                            {"comb.mix", "0.3"}}, join(delay("Long ambient"), reverb("Cathedral"))), {}});
        // Spec S7.13 (Phase 37, user 2026-11-26): world instruments, Presets > World. Starting points from the
        // existing modules (a flute, reed, bowed or plucked base); values to be set by ear. Each has its voices
        // and a reverb as the others; one-voice ones spread only when given more voices (S5.25).
        using detail::like;
        using P = std::vector<std::pair<std::string, std::string>>;
        const P breath{{"noise.colour", "Pink"}};
        const P keysSpread{{"out.spread", "0.4"}, byKeys};
        v.push_back(like("Shakuhachi", "Flute", join(P{solo, soloSpread, byVoices, {"vco1.wave", "Sine"}, {"vco2.wave", "Triangle"},
            {"vco2.tune", "0.05"}, {"mix.vco2", "0.3"}, {"mix.noise", "0.18"}, {"env1.attack", "0.12"}, {"env1.release", "0.25"},
            {"lfo1.rate", "5"}, {"vco1.fm", "0.15"}, {"vco2.fm", "0.15"}, {"lfo1.delay", "0.5"},
            {"formant.voice", "Tenor"}, {"formant.vowel", "3"}, {"formant.mix", "0.35"}}, join(breath, reverb("Concert hall")))));
        v.push_back(like("Bansuri", "Flute", join(P{solo, soloSpread, byVoices, {"vco1.wave", "Sine"}, {"mix.noise", "0.1"},
            {"lfo1.rate", "6"}, {"vco1.fm", "0.12"}, {"lfo1.delay", "0.3"}}, join(breath, reverb("Chamber")))));
        v.push_back(like("Pan flute", "Recorder", join(P{{"voice.count", "4"}, {"vco1.wave", "Sine"}, {"mix.noise", "0.08"},
            {"env1.attack", "0.03"}, {"env1.release", "0.4"}, {"env2.attack", "0.001"}, {"env2.decay", "0.08"}, {"env2.sustain", "0"},
            {"vcf1.env", "2"}}, join(breath, reverb("Concert hall")))));
        v.push_back(like("Duduk", "Oboe", join(P{solo, soloSpread, byVoices, {"vco1.wave", "Square"}, {"vco1.shape", "0.4"},   // S7.16: a 32 % pulse
            {"vcf1.cutoff", "4000"},   // (was a 12.5 % pulse through 1800 Hz bandpass, formant 0.6: 16 dB too quiet), still nasal
            {"vcf1.type", "Lowpass"}, {"formant.voice", "Tenor"}, {"formant.vowel", "3"}, {"formant.mix", "0.3"}, {"env1.attack", "0.15"},
            {"lfo1.rate", "4.5"}, {"vco1.fm", "0.1"}, {"lfo1.delay", "0.6"}}, reverb("Chamber"))));
        v.push_back(like("Erhu", "Violin", join(P{solo, soloSpread, byVoices, {"vco1.wave", "Saw"}, {"formant.voice", "Alto"},
            {"formant.vowel", "1"}, {"formant.mix", "0.5"}, {"env1.attack", "0.08"}, {"lfo1.rate", "6.5"}, {"vco1.fm", "0.25"},
            {"lfo1.delay", "0.25"}}, reverb("Concert hall"))));
        v.push_back(like("Shamisen", "Guitar (plucked)", join(P{{"voice.count", "3"}, {"vco1.wave", "Saw"}, {"env1.attack", "0.001"},
            {"env1.decay", "0.9"}, {"env1.sustain", "0"}, {"env1.release", "0.25"}, {"env2.decay", "0.15"}, {"env2.sustain", "0"},
            {"vcf1.cutoff", "1500"}, {"vcf1.env", "3"}, {"vcf1.res", "0.3"}, {"comb.freq", "261.6"}, {"comb.key", "1"},
            {"comb.feedback", "0.8"}, {"comb.damping", "0.1"}, {"comb.mix", "0.35"}}, reverb("Small room"))));
        v.push_back(like("Koto", "Guitar (plucked)", join(join(P{{"voice.count", "8"}, {"vco1.wave", "Triangle"}, {"vco2.wave", "Saw"},
            {"mix.vco2", "0.3"}, {"env1.decay", "1.8"}, {"env1.sustain", "0"}, {"env1.release", "0.6"}}, keysSpread),
            join(comb("Guitar (plucked)"), reverb("Chamber")))));
        v.push_back(like("Sitar", "Guitar (plucked)", join(P{{"voice.count", "6"}, {"vco1.wave", "Saw"}, {"vco3.wave", "Saw"},
            {"vco3.octave", "-1"}, {"mix.vco3", "0.25"}, {"env1.decay", "3"}, {"env1.sustain", "0"}, {"env1.release", "1"},
            {"vcf1.cutoff", "3000"}, {"vcf1.res", "0.3"}, {"comb.freq", "261.6"}, {"comb.key", "1"}, {"comb.feedback", "0.9"},
            {"comb.damping", "0.05"}, {"comb.mix", "0.4"}}, reverb("Chamber"))));
        v.push_back(like("Oud", "Guitar (plucked)", join(P{{"voice.count", "4"}, {"vco1.wave", "Saw"}, {"vcf1.cutoff", "1200"},
            {"env1.decay", "1.2"}, {"env1.sustain", "0"}}, join(comb("Guitar (plucked)"), reverb("Small room")))));
        v.push_back(like("Kalimba", "Marimba", join(join(P{{"voice.count", "8"}, {"vco1.wave", "Sine"}, {"vco1.pm", "1.2"},
            {"vco2.wave", "Sine"}, {"vco2.octave", "+2"}, {"vco2.tune", "0.84"}, {"mix.vco2", "0"}, {"env1.attack", "0.001"},
            {"env1.decay", "0.6"}, {"env1.sustain", "0"}, {"env1.release", "0.3"}}, keysSpread), reverb("Small room")),
            "vco1.pm=vco2.out"));
        v.push_back(like("Gamelan", "Marimba", join(join(P{{"voice.count", "12"}, {"vco1.wave", "Sine"}, {"vco1.pm", "1"},
            {"vco2.wave", "Sine"}, {"vco2.octave", "+2"}, {"vco2.tune", "-2.6"}, {"mix.vco2", "0.3"}, {"env1.attack", "0.001"},
            {"env1.decay", "4"}, {"env1.sustain", "0"}, {"env1.release", "2"}, {"out.spread", "0.5"}, byKeys}, ensemble("Subtle chorus")),
            reverb("Concert hall")), "vco1.pm=vco2.out"));
        v.push_back(like("Steel pan", "Marimba", join(join(P{{"voice.count", "8"}, {"vco1.wave", "Sine"}, {"vco1.pm", "0.8"},
            {"vco2.wave", "Sine"}, {"vco2.octave", "+1"}, {"mix.vco2", "0"}, {"env1.attack", "0.002"}, {"env1.decay", "1.2"},
            {"env1.sustain", "0.1"}, {"env1.release", "0.5"}}, keysSpread), reverb("Chamber")), "vco1.pm=vco2.out"));
        v.push_back(like("Didgeridoo", "Default", join(P{solo, {"vco1.wave", "Pulse"}, {"vco1.octave", "-1"}, {"vcf1.cutoff", "600"},
            {"vcf1.res", "0.4"}, {"formant.voice", "Bass"}, {"formant.vowel", "3"}, {"formant.mix", "0.8"}, {"lfo2.rate", "1.5"},
            {"mix.noise", "0.05"}, {"noise.colour", "Brown"}, {"env1.attack", "0.08"}, {"env1.sustain", "1"}, {"env1.release", "0.3"}},
            reverb("Small room")), "formant.vowel=lfo2.out@0.3"));
        // Spec S7.16: every instrument's family (world ones by how they are played).
        const std::vector<std::pair<std::string_view, std::vector<std::string_view>>> byFamily{
            {"Woodwinds", {"Flute", "Recorder", "Oboe", "Clarinet", "Bassoon", "Choral flute", "Shakuhachi", "Bansuri", "Pan flute", "Duduk"}},
            {"Brass", {"Trumpet", "French horn", "Didgeridoo"}},
            {"Strings", {"Violin", "Violin (PM vibrato)", "String section", "Erhu"}},
            {"Keyboards", {"Organ", "Piano", "Harpsichord", "Electric piano (FM)"}},
            {"Plucked & mallets", {"Guitar (plucked)", "Marimba", "Shamisen", "Koto", "Sitar", "Oud", "Kalimba", "Gamelan", "Steel pan"}},
            {"Voices", {"Choir (aah)", "Male voice (ooo)"}},
            {"Synth", {"Pad (slow)", "PWM strings", "Acid bass", "Filter sine", "Hollow lead", "Ping-pong pluck", "Wobble pad", "Shimmer space"}}};
        // Spec S7.16 (choice B a, user 2026-11-27: "a is ok"): OUT Volume per instrument so each sounds as loud as the
        // others — the loudest 50 ms of a held C4 measured through the whole plugin (host test 4h-2), every one set to
        // about 9 dB below what String section was (the synth cannot raise a quiet one above Volume 100 %).
        const std::vector<std::pair<std::string_view, std::string_view>> volume{
            {"Flute", "0.318"},
            {"Recorder", "0.349"},
            {"Oboe", "0.962"},
            {"Clarinet", "0.410"},
            {"Bassoon", "0.747"},
            {"Trumpet", "0.529"},
            {"French horn", "0.425"},
            {"Violin", "0.553"},
            {"String section", "0.284"},
            {"Organ", "0.374"},
            {"Piano", "0.322"},
            {"Harpsichord", "0.819"},
            {"Guitar (plucked)", "0.547"},
            {"Marimba", "0.857"},
            {"Pad (slow)", "0.560"},
            {"Choir (aah)", "0.210"},
            {"Male voice (ooo)", "0.681"},
            {"Violin (PM vibrato)", "0.488"},
            {"PWM strings", "0.341"},
            {"Electric piano (FM)", "0.493"},
            {"Choral flute", "0.401"},
            {"Acid bass", "1.000"},
            {"Filter sine", "0.415"},
            {"Hollow lead", "0.996"},
            {"Ping-pong pluck", "0.973"},
            {"Wobble pad", "0.940"},
            {"Shimmer space", "0.379"},
            {"Shakuhachi", "0.460"},
            {"Bansuri", "0.349"},
            {"Pan flute", "0.333"},
            {"Duduk", "1.000"},
            {"Erhu", "1.000"},
            {"Shamisen", "0.499"},
            {"Koto", "0.410"},
            {"Sitar", "0.341"},
            {"Oud", "0.401"},
            {"Kalimba", "0.643"},
            {"Gamelan", "0.962"},
            {"Steel pan", "0.580"},
            {"Didgeridoo", "0.593"}};
        for (auto& i : v)
            for (const auto& [name, gain] : volume) if (i.name == name) i.values.emplace_back("out.gain", std::string(gain));
        for (auto& i : v)
            for (const auto& [family, names] : byFamily)
                for (const auto n : names) if (i.name == n) i.family = std::string(family);
        return v;
    }();
    return list;
}

// Spec S7.7: own instrument presets are patch files in Documents/LMW-Synth Presets/INSTRUMENT.
inline constexpr std::string_view folderName = "INSTRUMENT";

} // namespace lmw::instruments
