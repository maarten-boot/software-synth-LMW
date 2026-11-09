// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared white noise source (Spec S5.1, S5.5): uniform ±1, xorshift32. RT-safe, deterministic.
#pragma once
#include <cstdint>

namespace lmw::dsp {

class Noise {
public:
    float next() noexcept {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        // Top 24 bits -> [0, 1) -> [-1, 1)
        return static_cast<float>(state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    void reset() noexcept { state = 0x9E3779B9u; }

private:
    std::uint32_t state = 0x9E3779B9u;
};

} // namespace lmw::dsp
