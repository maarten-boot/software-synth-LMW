// SPDX-License-Identifier: AGPL-3.0-or-later
// Semi-modular patch (Spec S5.3, S5.4, S5.14): fixed set of module outputs and inputs; every input has
// a normalled (default) source (or none), and up to four pins (cables with a level) that replace it.
// Pure data, RT-safe.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::dsp {

// Order = order in menus: roughly signal flow. Saved patches use the names, not the numbers.
enum class Out : std::uint8_t { KeysPitch, KeysGate, KeysVel, KeysMod, Vco1, Vco2, Vco3, Noise, Rand, Lfo1, Lfo2, Env1, Env2, Mix,   // KeysMod: the mod wheel (S6.24)
                                Vcf1, Vcf2, Formant, Mix2, Vca,           // Mix2: Spec S5.26
                                OutOut, CombOut, EnsembleOut, DelayOut, ReverbOut,   // effects (Spec S7.6, S7.8, S7.11): after OUT
                                Count };
enum class In  : std::uint8_t { Vco1Pitch, Vco1Fm, Vco1Pm, Vco2Pitch, Vco2Fm, Vco2Pm, Vco3Pitch, Vco3Fm, Vco3Pm,   // PM: S5.20
                                Env1Gate, Env2Gate,
                                MixIn1, MixIn2, MixIn3, MixIn4, Vcf1In, Vcf1Cutoff, Vcf1Res, Vcf2In, Vcf2Cutoff, Vcf2Res,
                                FormantIn, FormantVowel, Mix2In1, Mix2In2, Mix2In3, VcaIn, VcaCv,   // MIX 2: Spec S5.26
                                VcaMod, VcaVel,                          // velocity input: Spec S5.22
                                OutIn, Lfo1Rate, Lfo2Rate,               // LFO rate inputs: Spec S5.19
                                OutPan,                                  // pan per voice: Spec S5.25
                                CombIn, EnsembleIn, DelayIn, ReverbIn, MainIn,   // effects (Spec S7.6, S7.8, S7.11)
                                Count };

// Spec S5.10: three VCOs per voice.
inline constexpr int numVcos = 3;
inline constexpr Out vcoOut(int n) noexcept { return static_cast<Out>(static_cast<int>(Out::Vco1) + n); }
inline constexpr In vcoPitchIn(int n) noexcept { return static_cast<In>(static_cast<int>(In::Vco1Pitch) + 3 * n); }
inline constexpr In vcoFmIn(int n) noexcept { return static_cast<In>(static_cast<int>(In::Vco1Fm) + 3 * n); }
inline constexpr In vcoPmIn(int n) noexcept { return static_cast<In>(static_cast<int>(In::Vco1Pm) + 3 * n); }   // S5.20

inline constexpr int numOutputs = static_cast<int>(Out::Count);
inline constexpr int numInputs  = static_cast<int>(In::Count);

// Port names, as used in cable text, patch files and the 9P /cables file (Spec S5.3, S10.10).
inline constexpr std::array<std::string_view, numOutputs> outputNames{
    "keys.pitch", "keys.gate", "keys.vel", "keys.mod", "vco1.out", "vco2.out", "vco3.out",
    "noise.out", "noise.rand", "lfo1.out", "lfo2.out", "env1.out", "env2.out", "mix.out", "vcf1.out", "vcf2.out", "formant.out", "mix2.out", "vca.out",
    "out.out", "comb.out", "ensemble.out", "delay.out", "reverb.out"};
inline constexpr std::array<std::string_view, numInputs> inputNames{
    "vco1.pitch", "vco1.fm", "vco1.pm", "vco2.pitch", "vco2.fm", "vco2.pm", "vco3.pitch", "vco3.fm", "vco3.pm",
    "env1.gate", "env2.gate",
    "mix.in1", "mix.in2", "mix.in3", "mix.in4", "vcf1.in", "vcf1.cutoff", "vcf1.res", "vcf2.in", "vcf2.cutoff", "vcf2.res",
    "formant.in", "formant.vowel", "mix2.in1", "mix2.in2", "mix2.in3", "vca.in", "vca.cv",
    "vca.mod", "vca.vel", "out.in", "lfo1.rate", "lfo2.rate", "out.pan", "comb.in", "ensemble.in", "delay.in", "reverb.in", "main.in"};

// An input without a normalled source reads 0 until a cable is plugged in (Spec S5.13: vcfN.res).
inline constexpr Out noSource = Out::Count;

// Spec S5.3, S5.8 normalled sources; S5.12: LFO 1 -> VCO FM (vibrato), LFO 2 -> vca.mod (tremolo);
// S5.16: VCF 2 -> FORMANT -> VCA (formant.vowel has no normalled source).
inline constexpr std::array<Out, numInputs> normalled{
    Out::KeysPitch, Out::Lfo1, noSource, Out::KeysPitch, Out::Lfo1, noSource, Out::KeysPitch, Out::Lfo1, noSource,
    Out::KeysGate, Out::KeysGate,
    Out::Vco1, Out::Vco2, Out::Vco3, Out::Noise, Out::Mix, Out::Env2, noSource, Out::Vcf1, Out::Env2, noSource,
    Out::Vcf2, noSource, Out::Formant, noSource, noSource,       // S5.26: FORMANT -> MIX 2 in 1
    Out::Mix2, Out::Env1, Out::Lfo2,                              // S5.26: MIX 2 -> VCA (was FORMANT -> VCA)
    Out::KeysVel,                                                 // S5.22: velocity -> VCA, as before but patchable
    Out::Vca, noSource, noSource,
    noSource,                                                     // S5.25: out.pan, no normalled source
    Out::OutOut, Out::CombOut, Out::EnsembleOut, Out::DelayOut, Out::ReverbOut};   // S7.6, S7.8, S7.11: out.out -> COMB -> ENSEMBLE -> DELAY -> REVERB -> main.in

// Name of an output, or "none" for noSource (hover texts, 9P /ports).
inline constexpr std::string_view sourceName(Out o) noexcept {
    return o == noSource ? std::string_view("none") : outputNames[static_cast<std::size_t>(o)];
}

// Spec S5.17: VCF routing (vcf2.routing). Serial (default): mix.out -> VCF 1 -> VCF 2 -> FORMANT. Parallel:
// mix.out is normalled to vcf2.in too, and formant.in gets the average of the two VCFs (each at 50 %).
// Only those two inputs change; a cable on them overrides it as usual.
enum class VcfRouting : std::uint8_t { Serial, Parallel };

// Spec S7.14 (Phase 42): the order of the four effects (0 COMB, 1 ENSEMBLE, 2 DELAY, 3 REVERB), one of the 24
// orders numbered in lexicographic order: 0 = COMB, ENSEMBLE, DELAY, REVERB (the order before S7.14), 23 = reversed.
inline constexpr int numEffects = 4, numEffectOrders = 24;
using EffectOrder = std::array<int, numEffects>;
inline constexpr std::array<std::string_view, numEffects> effectNames{"comb", "ensemble", "delay", "reverb"};
inline constexpr EffectOrder effectOrder(int index) noexcept {
    index = index < 0 ? 0 : index >= numEffectOrders ? numEffectOrders - 1 : index;
    std::array<int, numEffects> pool{0, 1, 2, 3};
    EffectOrder r{};
    int left = numEffects, f = 6;                          // (left - 1)!
    for (int i = 0; i < numEffects; ++i) {
        const int k = index / f;
        index %= f;
        r[static_cast<std::size_t>(i)] = pool[static_cast<std::size_t>(k)];
        for (int j = k; j + 1 < left; ++j) pool[static_cast<std::size_t>(j)] = pool[static_cast<std::size_t>(j + 1)];
        --left;
        if (left > 0) f /= left;
    }
    return r;
}
inline constexpr int effectOrderIndex(const EffectOrder& o) noexcept {   // -1: not an order of the four
    for (int i = 0; i < numEffectOrders; ++i) if (effectOrder(i) == o) return i;
    return -1;
}
// "comb ensemble delay reverb" (spaces and / or commas, any case) -> the order's index; -1 unless each effect once.
inline int parseEffectOrder(std::string_view text) noexcept {
    EffectOrder o{};
    int n = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == ',' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')) ++i;
        std::size_t j = i;
        while (j < text.size() && !(text[j] == ' ' || text[j] == ',' || text[j] == '\t' || text[j] == '\n' || text[j] == '\r')) ++j;
        if (j == i) break;
        int found = -1;
        for (int e = 0; e < numEffects; ++e) {
            const auto name = effectNames[static_cast<std::size_t>(e)];
            if (j - i != name.size()) continue;
            bool same = true;
            for (std::size_t k = 0; k < name.size(); ++k) {
                char c = text[i + k];
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                same = same && c == name[k];
            }
            if (same) found = e;
        }
        if (found < 0 || n >= numEffects) return -1;
        o[static_cast<std::size_t>(n++)] = found;
        i = j;
    }
    return n == numEffects ? effectOrderIndex(o) : -1;
}
inline constexpr In effectIn(int e) noexcept { return static_cast<In>(static_cast<int>(In::CombIn) + e); }
inline constexpr Out effectOut(int e) noexcept { return static_cast<Out>(static_cast<int>(Out::CombOut) + e); }
// The normalled source of an effect input or main.in: the effect left of it in the order (out.out for the first).
inline constexpr Out effectNormal(In in, int orderIndex) noexcept {
    const auto o = effectOrder(orderIndex);
    if (in == In::MainIn) return effectOut(o[numEffects - 1]);
    for (int p = 0; p < numEffects; ++p)
        if (effectIn(o[static_cast<std::size_t>(p)]) == in) return p == 0 ? Out::OutOut : effectOut(o[static_cast<std::size_t>(p - 1)]);
    return noSource;
}

struct Normal { Out source = noSource; float level = 1.0f; };
struct Normals { std::array<Normal, 2> s{}; int n = 0; };   // the normalled sources of an input (0-2)
inline Normals normals(In in, VcfRouting r, int effects = 0) noexcept {
    if (in >= In::CombIn && in < In::Count) return {{{{effectNormal(in, effects), 1.0f}, {}}}, 1};   // S7.14
    if (r == VcfRouting::Parallel) {
        if (in == In::Vcf2In) return {{{{Out::Mix, 1.0f}, {}}}, 1};
        if (in == In::FormantIn) return {{{{Out::Vcf1, 0.5f}, {Out::Vcf2, 0.5f}}}, 2};
    }
    const Out o = normalled[static_cast<std::size_t>(in)];
    if (o == noSource) return {};
    return {{{{o, 1.0f}, {}}}, 1};
}
// The level at which `o` is normalled to `in` (0 = not normalled from it).
inline float normalLevel(In in, Out o, VcfRouting r, int effects = 0) noexcept {
    const auto ns = normals(in, r, effects);
    for (int k = 0; k < ns.n; ++k) if (ns.s[static_cast<std::size_t>(k)].source == o) return ns.s[static_cast<std::size_t>(k)].level;
    return 0.0f;
}

// Spec S7.6: two domains. Effect ports (out.out, the effects' ins and outs, main.in) carry the summed
// stereo signal after OUT; they connect only to each other, synth ports only to synth ports.
inline constexpr bool isEffectOut(Out o) noexcept { return o >= Out::OutOut && o < Out::Count; }
inline constexpr bool isEffectIn(In i) noexcept { return i >= In::CombIn && i < In::Count; }
inline constexpr bool canConnect(In i, Out o) noexcept { return isEffectIn(i) == isEffectOut(o); }

// Spec S6.25: the rows (inputs) and columns (outputs) of the two matrices. The Matrix tab: the synth's ports in
// their order; the effects matrix: the effects' inputs in chain order then main.in, out.out then the effects'
// outputs in chain order (the normalled chain is the diagonal next to the main one).
inline std::vector<int> synthMatrixInputs() {
    std::vector<int> v;
    for (int i = 0; i < numInputs; ++i) if (!isEffectIn(static_cast<In>(i))) v.push_back(i);
    return v;
}
inline std::vector<int> synthMatrixOutputs() {
    std::vector<int> v;
    for (int o = 0; o < numOutputs; ++o) if (!isEffectOut(static_cast<Out>(o))) v.push_back(o);
    return v;
}
inline std::vector<int> effectsMatrixInputs(int orderIndex) {
    std::vector<int> v;
    for (const int e : effectOrder(orderIndex)) v.push_back(static_cast<int>(effectIn(e)));
    v.push_back(static_cast<int>(In::MainIn));
    return v;
}
inline std::vector<int> effectsMatrixOutputs(int orderIndex) {
    std::vector<int> v{static_cast<int>(Out::OutOut)};
    for (const int e : effectOrder(orderIndex)) v.push_back(static_cast<int>(effectOut(e)));
    return v;
}

// Spec S5.21 (Phase 25a): modulation inputs take a cable level from -1 to +1 (a negative level subtracts:
// the inverted source); audio and gate inputs 0 to 1.
inline constexpr bool bipolar(In i) noexcept {
    switch (i) {
        case In::Vco1Pitch: case In::Vco1Fm: case In::Vco1Pm: case In::Vco2Pitch: case In::Vco2Fm: case In::Vco2Pm:
        case In::Vco3Pitch: case In::Vco3Fm: case In::Vco3Pm: case In::Vcf1Cutoff: case In::Vcf1Res:
        case In::Vcf2Cutoff: case In::Vcf2Res: case In::FormantVowel: case In::VcaCv: case In::VcaMod: case In::VcaVel:
        case In::Lfo1Rate: case In::Lfo2Rate: case In::OutPan:   // S5.25
            return true;
        default:
            return false;
    }
}
inline constexpr float minLevel(In i) noexcept { return bipolar(i) ? -1.0f : 0.0f; }

// Spec S6.19 (Phase 32): what a socket carries or is made for — audio, control voltage (modulation) or a gate.
// Patching stays free within a domain (S7.6); the type is shown as a ring and named in the hover text and /ports.
enum class SignalType : std::uint8_t { Audio, Cv, Gate };
inline constexpr SignalType typeOf(In i) noexcept {
    if (i == In::Env1Gate || i == In::Env2Gate) return SignalType::Gate;
    return bipolar(i) ? SignalType::Cv : SignalType::Audio;
}
inline constexpr SignalType typeOf(Out o) noexcept {
    switch (o) {
        case Out::KeysGate: return SignalType::Gate;
        case Out::KeysPitch: case Out::KeysVel: case Out::KeysMod: case Out::Rand: case Out::Lfo1: case Out::Lfo2: case Out::Env1: case Out::Env2:
            return SignalType::Cv;
        default: return SignalType::Audio;
    }
}
// FM and PM: CV inputs that also take audio (audio-rate FM / PM, S5.20).
inline constexpr bool alsoTakesAudio(In i) noexcept {
    for (int n = 0; n < numVcos; ++n) if (i == vcoFmIn(n) || i == vcoPmIn(n)) return true;
    return false;
}
inline constexpr std::string_view typeName(SignalType t) noexcept {
    return t == SignalType::Audio ? "audio" : t == SignalType::Cv ? "cv" : "gate";
}
// A level below -1 (this one) removes a pin; a level of 0 keeps it, silent (S5.21: a fader passes through 0).
inline constexpr float removePin = -2.0f;
inline constexpr bool removes(float level) noexcept { return level < -1.0f; }

inline constexpr bool isGlobal(Out o) noexcept { return o == Out::Lfo1 || o == Out::Lfo2 || o == Out::Noise || o == Out::Rand || o == Out::KeysMod; }

// Spec S5.14 (Phase 10): a patch is a pin matrix. Every input has up to maxPins pins, each an output
// with a level 0..1 (a cable is a pin at 1; -1..1 on modulation inputs, S5.21); the input reads the sum of
// level x output over its pins, or its normalled source while it has no pins.
class Patch {
public:
    static constexpr std::int8_t none = -1;
    static constexpr int maxPins = 4;
    struct Pin { std::int8_t source = none; float level = 1.0f; };

    // Adds a pin, or sets the level of the existing pin from `from` (up to 1; a level of 0 keeps it);
    // removePin (any level below -1) removes it. False when the input already has maxPins other pins,
    // for a negative level on an audio or gate input (S5.21), or across the domains (S7.6).
    bool connect(In in, Out from, float level = 1.0f) noexcept {
        auto& ps = pins[idx(in)];
        auto& n = count[idx(in)];
        const auto src = static_cast<std::int8_t>(from);
        const bool remove = removes(level);
        if (!canConnect(in, from)) return remove;                    // across the domains (S7.6): refused
        if (!remove && level < minLevel(in)) return false;           // S5.21: not inverted on this input
        if (level > 1.0f) level = 1.0f;
        for (int k = 0; k < n; ++k)
            if (ps[static_cast<std::size_t>(k)].source == src) {
                if (remove) { removeAt(in, k); return true; }
                ps[static_cast<std::size_t>(k)].level = level;
                return true;
            }
        if (remove) return true;
        if (n >= maxPins) return false;
        ps[static_cast<std::size_t>(n++)] = {src, level};
        return true;
    }
    void disconnect(In in, Out from) noexcept { connect(in, from, removePin); }
    void disconnect(In in) noexcept { count[idx(in)] = 0; }                     // all pins of the input
    void clear() noexcept { count.fill(0); }

    // Spec S5.17: the VCF routing decides two inputs' normalled sources; not part of the pins or the cable text.
    void setRouting(VcfRouting r) noexcept { route = r; }
    VcfRouting routing() const noexcept { return route; }
    // Spec S7.14: the effects order decides the effect inputs' normalled sources.
    void setEffectOrder(int index) noexcept { fxOrder = index; }
    int effectOrderIndex() const noexcept { return fxOrder; }
    Normals normals(In in) const noexcept { return lmw::dsp::normals(in, route, fxOrder); }
    float normalLevel(In in, Out o) const noexcept { return lmw::dsp::normalLevel(in, o, route, fxOrder); }

    bool isPatched(In in) const noexcept { return count[idx(in)] > 0; }
    int pinCount(In in) const noexcept { return count[idx(in)]; }
    const Pin& pin(In in, int k) const noexcept { return pins[idx(in)][static_cast<std::size_t>(k)]; }
    float level(In in, Out from) const noexcept {                              // 0 = no pin (or a silent one)
        for (int k = 0; k < count[idx(in)]; ++k)
            if (pin(in, k).source == static_cast<std::int8_t>(from)) return pin(in, k).level;
        return 0.0f;
    }
    bool hasPin(In in, Out from) const noexcept {                              // S5.21: a pin may be at 0
        for (int k = 0; k < count[idx(in)]; ++k)
            if (pin(in, k).source == static_cast<std::int8_t>(from)) return true;
        return false;
    }
    // The first pin's output, else the normalled source (for code that shows one source).
    Out source(In in) const noexcept {
        return isPatched(in) ? static_cast<Out>(pin(in, 0).source) : normals(in).s[0].source;
    }
    // True when the input gets exactly `o` at full level: normalled (no pins) or one pin at 1.
    bool onlySource(In in, Out o) const noexcept {
        const int n = count[idx(in)];
        if (n == 0) { const auto ns = normals(in); return ns.n == 1 && ns.s[0].source == o; }
        return n == 1 && pin(in, 0).source == static_cast<std::int8_t>(o) && pin(in, 0).level >= 1.0f;
    }
    bool samePins(const Patch& other, In in) const noexcept {   // same pins and levels, any order
        if (pinCount(in) != other.pinCount(in)) return false;
        for (int k = 0; k < pinCount(in); ++k)
            if (other.level(in, static_cast<Out>(pin(in, k).source)) != pin(in, k).level) return false;
        return true;
    }

private:
    static constexpr std::size_t idx(In in) noexcept { return static_cast<std::size_t>(in); }
    void removeAt(In in, int k) noexcept {
        auto& ps = pins[idx(in)];
        auto& n = count[idx(in)];
        for (int m = k + 1; m < n; ++m) ps[static_cast<std::size_t>(m - 1)] = ps[static_cast<std::size_t>(m)];
        --n;
    }
    std::array<std::array<Pin, maxPins>, numInputs> pins{};
    std::array<std::uint8_t, numInputs> count{};
    VcfRouting route = VcfRouting::Serial;
    int fxOrder = 0;                                 // S7.14
};

// A pin change sent from the GUI / 9P thread to the audio thread (Spec S5.4, S10.2): output none =
// remove all pins of the input; removePin (a level below -1) = remove that pin; else add it or set its level.
struct PatchCommand {
    In input = In::Vco1Fm;
    std::int8_t output = Patch::none;
    float level = 1.0f;
};

// Spec S6.26 (Phase 45): which units have an effect on what is heard. Followed back from main.in: an input counts
// when it is reached and its gain (the fader that scales it, e.g. a MIXER level) is not 0; its sources count through
// connections (cables, or the normalled ones while it has none) whose level is not 0; a source output reached that
// way makes every input of its unit reached. KEYS always counts.
enum class Use : std::uint8_t { Used, Passing, Unused };
struct UseParams {
    std::array<float, numInputs> gain{};                  // 0 = this input changes nothing (set by the caller)
    std::array<bool, numOutputs> passing{};               // FORMANT or an effect at Mix 0: passes its input unchanged
    UseParams() { gain.fill(1.0f); }
};
// The unit of a port: its name before the dot ("vcf1.cutoff" -> "vcf1").
inline std::string_view portUnit(std::string_view port) noexcept { return port.substr(0, port.find('.')); }
inline std::string_view portUnit(Out o) noexcept { return portUnit(outputNames[static_cast<std::size_t>(o)]); }
inline std::string_view portUnit(In i) noexcept { return portUnit(inputNames[static_cast<std::size_t>(i)]); }

struct UseResult {
    std::array<bool, numOutputs> output{};               // reaches main.in with a level
    std::array<bool, numInputs> input{};                 // reached and its gain not 0
    std::array<bool, numOutputs> passing{};
    // A unit (any of its outputs): Used, Passing (reached, but passes its input unchanged) or Unused. MAIN (no
    // outputs) and KEYS are always Used.
    Use unit(std::string_view u) const noexcept {
        if (u == "keys" || u == "main") return Use::Used;
        bool any = false, pass = false, has = false;
        for (int o = 0; o < numOutputs; ++o)
            if (portUnit(static_cast<Out>(o)) == u) {
                has = true;
                if (output[static_cast<std::size_t>(o)]) { any = true; pass = pass || passing[static_cast<std::size_t>(o)]; }
            }
        return !has || (any && !pass) ? Use::Used : any ? Use::Passing : Use::Unused;
    }
    bool operator==(const UseResult&) const = default;
};

inline UseResult unitUse(const Patch& p, const UseParams& up) {
    UseResult r;
    r.passing = up.passing;
    std::array<bool, numInputs> reached{};
    reached[static_cast<std::size_t>(In::MainIn)] = true;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < numInputs; ++i) {
            const auto in = static_cast<In>(i);
            const auto ii = static_cast<std::size_t>(i);
            if (!reached[ii] || up.gain[ii] == 0.0f) continue;
            r.input[ii] = true;
            auto reach = [&](Out o) {
                const auto oi = static_cast<std::size_t>(o);
                if (r.output[oi]) return;
                r.output[oi] = true;
                changed = true;
                for (int k = 0; k < numInputs; ++k)
                    if (portUnit(static_cast<In>(k)) == portUnit(o)) reached[static_cast<std::size_t>(k)] = true;
            };
            if (p.isPatched(in)) {
                for (int k = 0; k < p.pinCount(in); ++k)
                    if (p.pin(in, k).level != 0.0f) reach(static_cast<Out>(p.pin(in, k).source));
            } else {
                const auto ns = p.normals(in);
                for (int k = 0; k < ns.n; ++k)
                    if (ns.s[static_cast<std::size_t>(k)].level != 0.0f) reach(ns.s[static_cast<std::size_t>(k)].source);
            }
        }
    }
    return r;
}

// Why a unit has no effect, for the hover text: where its outputs go and why that does not count, e.g.
// "goes to mix.in2 (MIXER level 0)", "vcf1.in (cable at 0 %)", "vco2.fm (vco2 has no effect)"; or "not connected
// to MAIN". `gainName(in)` names the fader of an input with gain 0.
inline std::string unusedReason(const Patch& p, const UseParams& up, const UseResult& r, std::string_view unit,
                                std::string (*gainName)(In)) {
    std::string out;
    int shown = 0;
    auto add = [&](const std::string& t) { if (shown++ < 3) out += (out.empty() ? "" : "; ") + t; else if (shown == 4) out += "; ..."; };
    for (int i = 0; i < numInputs; ++i) {
        const auto in = static_cast<In>(i);
        const std::string name(inputNames[static_cast<std::size_t>(i)]);
        auto check = [&](Out o, float level, bool cable) {
            if (portUnit(o) != unit) return;
            if (level == 0.0f) add(name + " (" + (cable ? "cable" : "connection") + " at 0 %)");
            else if (up.gain[static_cast<std::size_t>(i)] == 0.0f) add(name + " (" + gainName(in) + " 0)");
            else if (!r.input[static_cast<std::size_t>(i)]) add(name + " (" + std::string(portUnit(in)) + " has no effect)");
        };
        if (p.isPatched(in)) {
            for (int k = 0; k < p.pinCount(in); ++k) check(static_cast<Out>(p.pin(in, k).source), p.pin(in, k).level, true);
        } else {
            const auto ns = p.normals(in);
            for (int k = 0; k < ns.n; ++k) check(ns.s[static_cast<std::size_t>(k)].source, ns.s[static_cast<std::size_t>(k)].level, false);
        }
    }
    return out.empty() ? std::string("not connected to MAIN") : "goes to " + out;
}

} // namespace lmw::dsp
