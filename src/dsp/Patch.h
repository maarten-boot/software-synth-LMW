// SPDX-License-Identifier: AGPL-3.0-or-later
// Semi-modular patch (Spec S5.3, S5.4): fixed set of module outputs and inputs; every input has
// a normalled (default) source, and at most one cable that replaces it. Pure data, RT-safe.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace lmw::dsp {

// Order = order in menus: roughly signal flow. Saved patches use the names, not the numbers.
enum class Out : std::uint8_t { KeysPitch, KeysGate, KeysVel, Vco, Noise, Lfo, Env1, Env2, Mix, Vcf, Vca, Count };
enum class In  : std::uint8_t { VcoPitch, VcoFm, Env1Gate, Env2Gate, MixIn1, MixIn2, VcfIn, VcfCutoff, VcaIn, VcaCv,
                                OutIn, Count };

inline constexpr int numOutputs = static_cast<int>(Out::Count);
inline constexpr int numInputs  = static_cast<int>(In::Count);

// Port names = future 9P file names (Spec S10).
inline constexpr std::array<std::string_view, numOutputs> outputNames{
    "keys.pitch", "keys.gate", "keys.vel",
    "vco.out", "noise.out", "lfo.out", "env1.out", "env2.out", "mix.out", "vcf.out", "vca.out"};
inline constexpr std::array<std::string_view, numInputs> inputNames{
    "vco.pitch", "vco.fm", "env1.gate", "env2.gate",
    "mix.in1", "mix.in2", "vcf.in", "vcf.cutoff", "vca.in", "vca.cv", "out.in"};

// Spec S5.3, S5.8 normalled sources.
inline constexpr std::array<Out, numInputs> normalled{
    Out::KeysPitch, Out::Lfo, Out::KeysGate, Out::KeysGate,
    Out::Vco, Out::Noise, Out::Mix, Out::Env2, Out::Vcf, Out::Env1, Out::Vca};

inline constexpr bool isGlobal(Out o) noexcept { return o == Out::Lfo || o == Out::Noise; }

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
    In input = In::VcoFm;
    std::int8_t output = Patch::none;   // none = remove the cable
};

} // namespace lmw::dsp
