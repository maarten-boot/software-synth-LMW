// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared noise source (Spec S5.1, S5.5, S5.15): white (uniform ±1, xorshift32), pink (−3 dB/octave,
// Paul Kellet's filter on the white noise) or brown (−6 dB/octave, leaky integrator). RT-safe,
// deterministic. White is the plain generator output, exactly as before colours existed.
#pragma once
#include <algorithm>
#include <cstdint>

namespace lmw::dsp {

enum class NoiseColour { White = 0, Pink, Brown };

class Noise {
public:
    void setColour(NoiseColour c) noexcept { colour = c; }

    float next() noexcept {
        const float white = nextWhite();
        switch (colour) {
            case NoiseColour::White: return white;
            case NoiseColour::Pink: {
                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
                b6 = white * 0.115926f;
                return std::clamp(pink * 0.2f, -1.0f, 1.0f);          // about the loudness of white
            }
            case NoiseColour::Brown:
                brown = (brown + 0.02f * white) / 1.02f;
                return std::clamp(brown * 5.0f, -1.0f, 1.0f);
        }
        return white;
    }

    void reset() noexcept { state = 0x9E3779B9u; b0 = b1 = b2 = b3 = b4 = b5 = b6 = brown = 0.0f; }

private:
    float nextWhite() noexcept {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        // Top 24 bits -> [0, 1) -> [-1, 1)
        return static_cast<float>(state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    std::uint32_t state = 0x9E3779B9u;
    NoiseColour colour = NoiseColour::White;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0;
};

} // namespace lmw::dsp
