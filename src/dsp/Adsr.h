// SPDX-License-Identifier: AGPL-3.0-or-later
// Linear ADSR envelope (Spec S5.1). Times in seconds, sustain 0..1. RT-safe.
#pragma once
#include <algorithm>

namespace lmw::dsp {

class Adsr {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr) noexcept { sampleRate = sr; }
    void setParameters(float a, float d, float s, float r) noexcept {
        attack = a; decay = d; sustain = std::clamp(s, 0.0f, 1.0f); release = r;
        // A sustain change on a held note glides in next() (Spec S5a.3) instead of jumping.
    }
    // Retriggers from the current level (no jump to 0 -> no click).
    void noteOn() noexcept { stage = Stage::Attack; }
    void noteOff() noexcept {
        if (stage == Stage::Idle) return;
        stage = Stage::Release;
        releaseStep = release > 0.0f ? level / static_cast<float>(release * sampleRate) : level;
    }
    void reset() noexcept { stage = Stage::Idle; level = 0.0f; }

    float next() noexcept {
        switch (stage) {
            case Stage::Idle: return 0.0f;
            case Stage::Attack:
                level += attack > 0.0f ? 1.0f / static_cast<float>(attack * sampleRate) : 1.0f;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level -= decay > 0.0f ? (1.0f - sustain) / static_cast<float>(decay * sampleRate) : 1.0f;
                if (level <= sustain) { level = sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain: {                     // glide to a changed sustain level over 20 ms
                const float step = 1.0f / static_cast<float>(0.02 * sampleRate);
                level += std::clamp(sustain - level, -step, step);
                break;
            }
            case Stage::Release:
                level -= releaseStep;
                if (level <= 0.0f) { level = 0.0f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

private:
    double sampleRate = 48000.0;
    float attack = 0.01f, decay = 0.1f, sustain = 0.8f, release = 0.2f;
    float level = 0.0f, releaseStep = 0.0f;
    Stage stage = Stage::Idle;
};

} // namespace lmw::dsp
