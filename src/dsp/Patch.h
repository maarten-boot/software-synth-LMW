// SPDX-License-Identifier: AGPL-3.0-or-later
// Semi-modular patch (Spec S5.3, S5.4): fixed set of module outputs and inputs; every input has
// a normalled (default) source, and at most one cable that replaces it. Pure data, RT-safe.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace lmw::dsp {

// Order = order in menus: roughly signal flow. Saved patches use the names, not the numbers.
enum class Out : std::uint8_t { KeysPitch, KeysGate, KeysVel, Vco1, Vco2, Vco3, Noise, Lfo1, Lfo2, Env1, Env2, Mix, Vcf1,
                                Vcf2, Vca, Count };
enum class In  : std::uint8_t { Vco1Pitch, Vco1Fm, Vco2Pitch, Vco2Fm, Vco3Pitch, Vco3Fm, Env1Gate, Env2Gate,
                                MixIn1, MixIn2, MixIn3, MixIn4, Vcf1In, Vcf1Cutoff, Vcf2In, Vcf2Cutoff, VcaIn, VcaCv, VcaMod, OutIn,
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
    "noise.out", "lfo1.out", "lfo2.out", "env1.out", "env2.out", "mix.out", "vcf1.out", "vcf2.out", "vca.out"};
inline constexpr std::array<std::string_view, numInputs> inputNames{
    "vco1.pitch", "vco1.fm", "vco2.pitch", "vco2.fm", "vco3.pitch", "vco3.fm", "env1.gate", "env2.gate",
    "mix.in1", "mix.in2", "mix.in3", "mix.in4", "vcf1.in", "vcf1.cutoff", "vcf2.in", "vcf2.cutoff", "vca.in", "vca.cv",
    "vca.mod", "out.in"};

// Spec S5.3, S5.8 normalled sources; S5.12: LFO 1 -> VCO FM (vibrato), LFO 2 -> vca.mod (tremolo).
inline constexpr std::array<Out, numInputs> normalled{
    Out::KeysPitch, Out::Lfo1, Out::KeysPitch, Out::Lfo1, Out::KeysPitch, Out::Lfo1, Out::KeysGate, Out::KeysGate,
    Out::Vco1, Out::Vco2, Out::Vco3, Out::Noise, Out::Mix, Out::Env2, Out::Vcf1, Out::Env2, Out::Vcf2, Out::Env1, Out::Lfo2,
    Out::Vca};

inline constexpr bool isGlobal(Out o) noexcept { return o == Out::Lfo1 || o == Out::Lfo2 || o == Out::Noise; }

class Patch {
public:
    static constexpr std::int8_t none = -1;

    void connect(In in, Out from) noexcept { cable[idx(in)] = static_cast<std::int8_t>(from); }
    void disconnect(In in) noexcept { cable[idx(in)] = none; }
    void clear() noexcept { cable.fill(none); }

    bool isPatched(In in) const noexcept { return cable[idx(in)] != none; }
    // Effective source: the cable if present, else the normalled source.
    Out source(In in) const noexcept {
        return isPatched(in) ? static_cast<Out>(cable[idx(in)]) : normalled[idx(in)];
    }

private:
    static constexpr std::size_t idx(In in) noexcept { return static_cast<std::size_t>(in); }
    std::array<std::int8_t, numInputs> cable = [] { std::array<std::int8_t, numInputs> a{}; a.fill(none); return a; }();
};

// A cable change sent from the GUI / 9P thread to the audio thread (Spec S5.4, S10.2).
struct PatchCommand {
    In input = In::Vco1Fm;
    std::int8_t output = Patch::none;   // none = remove the cable
};

} // namespace lmw::dsp
