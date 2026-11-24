// SPDX-License-Identifier: AGPL-3.0-or-later
// Semi-modular patch (Spec S5.3, S5.4, S5.14): fixed set of module outputs and inputs; every input has
// a normalled (default) source (or none), and up to four pins (cables with a level) that replace it.
// Pure data, RT-safe.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace lmw::dsp {

// Order = order in menus: roughly signal flow. Saved patches use the names, not the numbers.
enum class Out : std::uint8_t { KeysPitch, KeysGate, KeysVel, Vco1, Vco2, Vco3, Noise, Rand, Lfo1, Lfo2, Env1, Env2, Mix,
                                Vcf1, Vcf2, Formant, Vca,
                                OutOut, CombOut, DelayOut, ReverbOut,    // effects (Spec S7.6, S7.8): after OUT
                                Count };
enum class In  : std::uint8_t { Vco1Pitch, Vco1Fm, Vco1Pm, Vco2Pitch, Vco2Fm, Vco2Pm, Vco3Pitch, Vco3Fm, Vco3Pm,   // PM: S5.20
                                Env1Gate, Env2Gate,
                                MixIn1, MixIn2, MixIn3, MixIn4, Vcf1In, Vcf1Cutoff, Vcf1Res, Vcf2In, Vcf2Cutoff, Vcf2Res,
                                FormantIn, FormantVowel, VcaIn, VcaCv,
                                VcaMod, OutIn, Lfo1Rate, Lfo2Rate,      // LFO rate inputs: Spec S5.19
                                CombIn, DelayIn, ReverbIn, MainIn,       // effects (Spec S7.6, S7.8)
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
    "keys.pitch", "keys.gate", "keys.vel", "vco1.out", "vco2.out", "vco3.out",
    "noise.out", "noise.rand", "lfo1.out", "lfo2.out", "env1.out", "env2.out", "mix.out", "vcf1.out", "vcf2.out", "formant.out", "vca.out",
    "out.out", "comb.out", "delay.out", "reverb.out"};
inline constexpr std::array<std::string_view, numInputs> inputNames{
    "vco1.pitch", "vco1.fm", "vco1.pm", "vco2.pitch", "vco2.fm", "vco2.pm", "vco3.pitch", "vco3.fm", "vco3.pm",
    "env1.gate", "env2.gate",
    "mix.in1", "mix.in2", "mix.in3", "mix.in4", "vcf1.in", "vcf1.cutoff", "vcf1.res", "vcf2.in", "vcf2.cutoff", "vcf2.res",
    "formant.in", "formant.vowel", "vca.in", "vca.cv",
    "vca.mod", "out.in", "lfo1.rate", "lfo2.rate", "comb.in", "delay.in", "reverb.in", "main.in"};

// An input without a normalled source reads 0 until a cable is plugged in (Spec S5.13: vcfN.res).
inline constexpr Out noSource = Out::Count;

// Spec S5.3, S5.8 normalled sources; S5.12: LFO 1 -> VCO FM (vibrato), LFO 2 -> vca.mod (tremolo);
// S5.16: VCF 2 -> FORMANT -> VCA (formant.vowel has no normalled source).
inline constexpr std::array<Out, numInputs> normalled{
    Out::KeysPitch, Out::Lfo1, noSource, Out::KeysPitch, Out::Lfo1, noSource, Out::KeysPitch, Out::Lfo1, noSource,
    Out::KeysGate, Out::KeysGate,
    Out::Vco1, Out::Vco2, Out::Vco3, Out::Noise, Out::Mix, Out::Env2, noSource, Out::Vcf1, Out::Env2, noSource,
    Out::Vcf2, noSource, Out::Formant, Out::Env1, Out::Lfo2,
    Out::Vca, noSource, noSource,
    Out::OutOut, Out::CombOut, Out::DelayOut, Out::ReverbOut};   // S7.6, S7.8: out.out -> COMB -> DELAY -> REVERB -> main.in

// Name of an output, or "none" for noSource (hover texts, 9P /ports).
inline constexpr std::string_view sourceName(Out o) noexcept {
    return o == noSource ? std::string_view("none") : outputNames[static_cast<std::size_t>(o)];
}

// Spec S5.17: VCF routing (vcf2.routing). Serial (default): mix.out -> VCF 1 -> VCF 2 -> FORMANT. Parallel:
// mix.out is normalled to vcf2.in too, and formant.in gets the average of the two VCFs (each at 50 %).
// Only those two inputs change; a cable on them overrides it as usual.
enum class VcfRouting : std::uint8_t { Serial, Parallel };
struct Normal { Out source = noSource; float level = 1.0f; };
struct Normals { std::array<Normal, 2> s{}; int n = 0; };   // the normalled sources of an input (0-2)
inline Normals normals(In in, VcfRouting r) noexcept {
    if (r == VcfRouting::Parallel) {
        if (in == In::Vcf2In) return {{{{Out::Mix, 1.0f}, {}}}, 1};
        if (in == In::FormantIn) return {{{{Out::Vcf1, 0.5f}, {Out::Vcf2, 0.5f}}}, 2};
    }
    const Out o = normalled[static_cast<std::size_t>(in)];
    if (o == noSource) return {};
    return {{{{o, 1.0f}, {}}}, 1};
}
// The level at which `o` is normalled to `in` (0 = not normalled from it).
inline float normalLevel(In in, Out o, VcfRouting r) noexcept {
    const auto ns = normals(in, r);
    for (int k = 0; k < ns.n; ++k) if (ns.s[static_cast<std::size_t>(k)].source == o) return ns.s[static_cast<std::size_t>(k)].level;
    return 0.0f;
}

// Spec S7.6: two domains. Effect ports (out.out, the effects' ins and outs, main.in) carry the summed
// stereo signal after OUT; they connect only to each other, synth ports only to synth ports.
inline constexpr bool isEffectOut(Out o) noexcept { return o >= Out::OutOut && o < Out::Count; }
inline constexpr bool isEffectIn(In i) noexcept { return i >= In::CombIn && i < In::Count; }
inline constexpr bool canConnect(In i, Out o) noexcept { return isEffectIn(i) == isEffectOut(o); }

// Spec S5.21 (Phase 25a): modulation inputs take a cable level from -1 to +1 (a negative level subtracts:
// the inverted source); audio and gate inputs 0 to 1.
inline constexpr bool bipolar(In i) noexcept {
    switch (i) {
        case In::Vco1Pitch: case In::Vco1Fm: case In::Vco1Pm: case In::Vco2Pitch: case In::Vco2Fm: case In::Vco2Pm:
        case In::Vco3Pitch: case In::Vco3Fm: case In::Vco3Pm: case In::Vcf1Cutoff: case In::Vcf1Res:
        case In::Vcf2Cutoff: case In::Vcf2Res: case In::FormantVowel: case In::VcaCv: case In::VcaMod:
        case In::Lfo1Rate: case In::Lfo2Rate:
            return true;
        default:
            return false;
    }
}
inline constexpr float minLevel(In i) noexcept { return bipolar(i) ? -1.0f : 0.0f; }
// A level below -1 (this one) removes a pin; a level of 0 keeps it, silent (S5.21: a fader passes through 0).
inline constexpr float removePin = -2.0f;
inline constexpr bool removes(float level) noexcept { return level < -1.0f; }

inline constexpr bool isGlobal(Out o) noexcept { return o == Out::Lfo1 || o == Out::Lfo2 || o == Out::Noise || o == Out::Rand; }

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
    Normals normals(In in) const noexcept { return lmw::dsp::normals(in, route); }
    float normalLevel(In in, Out o) const noexcept { return lmw::dsp::normalLevel(in, o, route); }

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
};

// A pin change sent from the GUI / 9P thread to the audio thread (Spec S5.4, S10.2): output none =
// remove all pins of the input; removePin (a level below -1) = remove that pin; else add it or set its level.
struct PatchCommand {
    In input = In::Vco1Fm;
    std::int8_t output = Patch::none;
    float level = 1.0f;
};

} // namespace lmw::dsp
