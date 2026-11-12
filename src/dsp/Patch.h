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
                                Vcf1, Vcf2, Vca, Count };
enum class In  : std::uint8_t { Vco1Pitch, Vco1Fm, Vco2Pitch, Vco2Fm, Vco3Pitch, Vco3Fm, Env1Gate, Env2Gate,
                                MixIn1, MixIn2, MixIn3, MixIn4, Vcf1In, Vcf1Cutoff, Vcf1Res, Vcf2In, Vcf2Cutoff, Vcf2Res, VcaIn, VcaCv,
                                VcaMod, OutIn,
                                Count };

// Spec S5.10: three VCOs per voice.
inline constexpr int numVcos = 3;
inline constexpr Out vcoOut(int n) noexcept { return static_cast<Out>(static_cast<int>(Out::Vco1) + n); }
inline constexpr In vcoPitchIn(int n) noexcept { return static_cast<In>(static_cast<int>(In::Vco1Pitch) + 2 * n); }
inline constexpr In vcoFmIn(int n) noexcept { return static_cast<In>(static_cast<int>(In::Vco1Fm) + 2 * n); }

inline constexpr int numOutputs = static_cast<int>(Out::Count);
inline constexpr int numInputs  = static_cast<int>(In::Count);

// Port names, as used in cable text, patch files and the 9P /cables file (Spec S5.3, S10.10).
inline constexpr std::array<std::string_view, numOutputs> outputNames{
    "keys.pitch", "keys.gate", "keys.vel", "vco1.out", "vco2.out", "vco3.out",
    "noise.out", "noise.rand", "lfo1.out", "lfo2.out", "env1.out", "env2.out", "mix.out", "vcf1.out", "vcf2.out", "vca.out"};
inline constexpr std::array<std::string_view, numInputs> inputNames{
    "vco1.pitch", "vco1.fm", "vco2.pitch", "vco2.fm", "vco3.pitch", "vco3.fm", "env1.gate", "env2.gate",
    "mix.in1", "mix.in2", "mix.in3", "mix.in4", "vcf1.in", "vcf1.cutoff", "vcf1.res", "vcf2.in", "vcf2.cutoff", "vcf2.res",
    "vca.in", "vca.cv",
    "vca.mod", "out.in"};

// An input without a normalled source reads 0 until a cable is plugged in (Spec S5.13: vcfN.res).
inline constexpr Out noSource = Out::Count;

// Spec S5.3, S5.8 normalled sources; S5.12: LFO 1 -> VCO FM (vibrato), LFO 2 -> vca.mod (tremolo).
inline constexpr std::array<Out, numInputs> normalled{
    Out::KeysPitch, Out::Lfo1, Out::KeysPitch, Out::Lfo1, Out::KeysPitch, Out::Lfo1, Out::KeysGate, Out::KeysGate,
    Out::Vco1, Out::Vco2, Out::Vco3, Out::Noise, Out::Mix, Out::Env2, noSource, Out::Vcf1, Out::Env2, noSource, Out::Vcf2, Out::Env1, Out::Lfo2,
    Out::Vca};

// Name of an output, or "none" for noSource (hover texts, 9P /ports).
inline constexpr std::string_view sourceName(Out o) noexcept {
    return o == noSource ? std::string_view("none") : outputNames[static_cast<std::size_t>(o)];
}

inline constexpr bool isGlobal(Out o) noexcept { return o == Out::Lfo1 || o == Out::Lfo2 || o == Out::Noise || o == Out::Rand; }

// Spec S5.14 (Phase 10): a patch is a pin matrix. Every input has up to maxPins pins, each an output
// with a level 0..1 (a cable is a pin at 1); the input reads the sum of level x output over its pins,
// or its normalled source while it has no pins.
class Patch {
public:
    static constexpr std::int8_t none = -1;
    static constexpr int maxPins = 4;
    struct Pin { std::int8_t source = none; float level = 1.0f; };

    // Adds a pin, or sets the level of the existing pin from `from`; a level <= 0 removes it. False
    // when the input already has maxPins other pins.
    bool connect(In in, Out from, float level = 1.0f) noexcept {
        auto& ps = pins[idx(in)];
        auto& n = count[idx(in)];
        const auto src = static_cast<std::int8_t>(from);
        for (int k = 0; k < n; ++k)
            if (ps[static_cast<std::size_t>(k)].source == src) {
                if (level <= 0.0f) { removeAt(in, k); return true; }
                ps[static_cast<std::size_t>(k)].level = level > 1.0f ? 1.0f : level;
                return true;
            }
        if (level <= 0.0f) return true;
        if (n >= maxPins) return false;
        ps[static_cast<std::size_t>(n++)] = {src, level > 1.0f ? 1.0f : level};
        return true;
    }
    void disconnect(In in, Out from) noexcept { connect(in, from, 0.0f); }
    void disconnect(In in) noexcept { count[idx(in)] = 0; }                     // all pins of the input
    void clear() noexcept { count.fill(0); }

    bool isPatched(In in) const noexcept { return count[idx(in)] > 0; }
    int pinCount(In in) const noexcept { return count[idx(in)]; }
    const Pin& pin(In in, int k) const noexcept { return pins[idx(in)][static_cast<std::size_t>(k)]; }
    float level(In in, Out from) const noexcept {                              // 0 = no pin
        for (int k = 0; k < count[idx(in)]; ++k)
            if (pin(in, k).source == static_cast<std::int8_t>(from)) return pin(in, k).level;
        return 0.0f;
    }
    // The first pin's output, else the normalled source (for code that shows one source).
    Out source(In in) const noexcept {
        return isPatched(in) ? static_cast<Out>(pin(in, 0).source) : normalled[idx(in)];
    }
    // True when the input gets exactly `o` at full level: normalled (no pins) or one pin at 1.
    bool onlySource(In in, Out o) const noexcept {
        const int n = count[idx(in)];
        if (n == 0) return normalled[idx(in)] == o;
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
};

// A pin change sent from the GUI / 9P thread to the audio thread (Spec S5.4, S10.2): output none =
// remove all pins of the input; level <= 0 = remove that pin; else add it or set its level.
struct PatchCommand {
    In input = In::Vco1Fm;
    std::int8_t output = Patch::none;
    float level = 1.0f;
};

} // namespace lmw::dsp
