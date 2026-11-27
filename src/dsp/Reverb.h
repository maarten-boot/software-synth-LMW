// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.15 (Phase 43): the Hall reverb — a feedback delay network (FDN) instead of Freeverb's fixed combs.
//   input (per side) -> 4 allpass diffusers -> 8 delay lines, mixed by an 8 x 8 Householder matrix; each line's
//   length moves slowly (its own rate, a few samples) so no mode stays put (Freeverb's metallic ring); per line a
//   one-pole lowpass and a gain set the decay: RT60 at low frequencies from Size, the highs faster from Damping.
//   Early reflections: a few taps of the undiffused input per side. Output: left and right from the lines with
//   different signs (decorrelated), Width blends to mono. Wet only (the caller mixes dry and wet).
// Header-only, allocation in prepare() only (RT-safe process()).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace lmw::dsp {

class HallReverb {
public:
    static constexpr int numLines = 8;

    // Size 0..1 -> RT60 at low frequencies, 0.3 s .. 12 s (exponential).
    static double rt60(double size) noexcept { return 0.3 * std::pow(40.0, std::clamp(size, 0.0, 1.0)); }
    // Damping 0..1 -> the decay time at the top (Nyquist) as a share of the low one: 1 .. 1/13 (8 kHz: about 1/3).
    static double hfRatio(double damping) noexcept { return 1.0 / (1.0 + 12.0 * std::clamp(damping, 0.0, 1.0)); }

    void prepare(double sampleRate) {
        sr = sampleRate;
        const double scale = sr / 1000.0;
        for (int i = 0; i < numLines; ++i) {
            // longest length: base x room 1.5 + modulation depth + interpolation margin
            const auto n = static_cast<std::size_t>(std::ceil(lineMs[static_cast<std::size_t>(i)] * 1.5 * scale + modDepthMs * scale + 8));
            lines[static_cast<std::size_t>(i)].assign(n, 0.0f);
        }
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < 4; ++k)
                diff[static_cast<std::size_t>(s)][static_cast<std::size_t>(k)].buf.assign(
                    static_cast<std::size_t>(std::ceil(diffMs[static_cast<std::size_t>(s)][static_cast<std::size_t>(k)] * scale)) + 1, 0.0f);
        early.assign(static_cast<std::size_t>(std::ceil(60.0 * 1.5 * scale)) + 2, {0.0f, 0.0f});
        for (int i = 0; i < numLines; ++i) {
            const double hz = modHz[static_cast<std::size_t>(i)];
            modStep[static_cast<std::size_t>(i)] = 2.0 * 3.141592653589793 * hz / sr;
        }
        reset();
        setParameters(size, damping, width);
        len = lenTarget;
    }

    void reset() noexcept {
        for (auto& l : lines) std::fill(l.begin(), l.end(), 0.0f);
        for (auto& side : diff) for (auto& d : side) { std::fill(d.buf.begin(), d.buf.end(), 0.0f); d.pos = 0; }
        std::fill(early.begin(), early.end(), std::array<float, 2>{0.0f, 0.0f});
        lp.fill(0.0f);
        tone = {};
        wp.fill(0);
        earlyPos = 0;
        for (int i = 0; i < numLines; ++i) modPhase[static_cast<std::size_t>(i)] = 0.7 * i;   // spread, deterministic
    }

    void setParameters(float newSize, float newDamping, float newWidth) noexcept {
        size = std::clamp(newSize, 0.0f, 1.0f);
        damping = std::clamp(newDamping, 0.0f, 1.0f);
        width = std::clamp(newWidth, 0.0f, 1.0f);
        if (sr <= 0.0) return;
        room = 0.6 + 0.9 * size;                                 // delay lengths x0.6 .. x1.5
        const double t60 = rt60(size), ratio = hfRatio(damping);
        for (int i = 0; i < numLines; ++i) {
            const auto u = static_cast<std::size_t>(i);
            lenTarget[u] = lineMs[u] * room * sr / 1000.0;
            const double seconds = lenTarget[u] / sr;
            const double gLow = std::pow(10.0, -3.0 * seconds / t60);
            const double gHigh = std::pow(10.0, -3.0 * seconds / (t60 * ratio));
            // One pole y = (1 - a) x + a y': DC gain 1, Nyquist (1 - a) / (1 + a) = gHigh / gLow.
            const double r = gHigh / gLow;
            lpCoeff[u] = static_cast<float>((1.0 - r) / (1.0 + r));
            gain[u] = static_cast<float>(gLow);
        }
        // About the same loudness at every Size: the level without this gain (steady noise, measured 2026-11-27:
        // -19.6 dB at Size 0, -10.3 dB at 50 %, -3.3 dB at 100 %) follows rawLevelDb(); scaled to Size 50 %.
        outGain = static_cast<float>(levelTrim * std::pow(10.0, (rawLevelDb(0.5) - rawLevelDb(size)) / 20.0));
        earlyRoom = room;
        // Damping also darkens the whole wet sound (the first reflections too): a lowpass, 20 kHz at 0 down
        // to 2 kHz at 100 % (20 kHz x 0.1^(Damping^2)), two poles; the decay alone left the start of the tail bright.
        const double fc = std::min(20000.0 * std::pow(0.1, double(damping) * damping), 0.45 * sr);
        toneCoeff = static_cast<float>(std::exp(-2.0 * 3.141592653589793 * fc / sr));
    }

    // One stereo sample in, wet stereo out.
    void process(float inL, float inR, float& outL, float& outR) noexcept {
        // Early reflections from the undiffused input.
        early[earlyPos] = {inL, inR};
        float erL = 0.0f, erR = 0.0f;
        const double scale = sr / 1000.0 * earlyRoom;
        for (std::size_t k = 0; k < erTaps; ++k) {
            erL += erGain[k] * tapAt(static_cast<std::size_t>(erMsL[k] * scale), 0);
            erR += erGain[k] * tapAt(static_cast<std::size_t>(erMsR[k] * scale), 1);
        }
        earlyPos = earlyPos + 1 == early.size() ? 0 : earlyPos + 1;

        // Diffusion: 4 allpasses per side.
        float dL = inL, dR = inR;
        for (std::size_t k = 0; k < 4; ++k) {
            dL = diff[0][k].process(dL, diffGain[k]);
            dR = diff[1][k].process(dR, diffGain[k]);
        }

        // Read the lines (modulated, linear interpolation), damp, decay.
        std::array<float, numLines> y{};
        float sum = 0.0f;
        for (int i = 0; i < numLines; ++i) {
            const auto u = static_cast<std::size_t>(i);
            len[u] += (lenTarget[u] - len[u]) * 0.0005;          // a Size change glides (no zipper, no click)
            modPhase[u] += modStep[u];
            if (modPhase[u] > 6.283185307179586) modPhase[u] -= 6.283185307179586;
            const double d = len[u] + modDepthMs * 0.5 * sr / 1000.0 * (1.0 + std::sin(modPhase[u]));
            const auto& buf = lines[u];
            const auto n = buf.size();
            double rp = static_cast<double>(wp[u]) - d;
            while (rp < 0) rp += static_cast<double>(n);
            const auto i0 = static_cast<std::size_t>(rp);
            const float frac = static_cast<float>(rp - static_cast<double>(i0));
            const float a = buf[i0], b = buf[i0 + 1 == n ? 0 : i0 + 1];
            const float x = a + (b - a) * frac;
            lp[u] = x * (1.0f - lpCoeff[u]) + lp[u] * lpCoeff[u];
            y[u] = lp[u] * gain[u];
            sum += y[u];
        }
        // Householder: every line feeds every line; the input goes to half the lines per side.
        const float h = sum * (2.0f / numLines);
        for (int i = 0; i < numLines; ++i) {
            const auto u = static_cast<std::size_t>(i);
            const float in = (i < numLines / 2 ? dL : dR) * inSign[u] * 0.5f;
            auto& buf = lines[u];
            buf[wp[u]] = y[u] - h + in;
            wp[u] = wp[u] + 1 == buf.size() ? 0 : wp[u] + 1;
        }

        float L = 0.0f, R = 0.0f;
        for (std::size_t u = 0; u < static_cast<std::size_t>(numLines); ++u) { L += outSignL[u] * y[u]; R += outSignR[u] * y[u]; }
        L = (L + erL * erLevel) * outGain;
        R = (R + erR * erLevel) * outGain;
        for (std::size_t k = 0; k < 2; ++k) {                    // two poles: 12 dB per octave
            tone[k][0] = L * (1.0f - toneCoeff) + tone[k][0] * toneCoeff;
            tone[k][1] = R * (1.0f - toneCoeff) + tone[k][1] * toneCoeff;
            L = tone[k][0];
            R = tone[k][1];
        }
        const float mid = 0.5f * (L + R), side = 0.5f * (L - R) * width;
        outL = mid + side;
        outR = mid - side;
    }

    // The wet level (dB, steady white noise) before the level gain, fitted to measurements at Size 0 .. 100 %.
    static double rawLevelDb(double sz) noexcept { return -4.78 * sz * sz + 21.13 * sz - 19.63; }

    double tailSeconds() const noexcept { return rt60(size) + 1.0; }

private:
    struct Allpass {
        std::vector<float> buf;
        std::size_t pos = 0;
        float process(float x, float g) noexcept {
            const float d = buf[pos];
            const float v = x + g * d;
            buf[pos] = v;
            pos = pos + 1 == buf.size() ? 0 : pos + 1;
            return d - g * v;
        }
    };
    float tapAt(std::size_t delay, int side) const noexcept {
        const auto n = early.size();
        delay = std::min(delay, n - 1);
        const std::size_t i = earlyPos >= delay ? earlyPos - delay : earlyPos + n - delay;
        return early[i][static_cast<std::size_t>(side)];
    }

    // Mutually prime-ish lengths (ms at room x1), the shortest and longest about 2.5 apart.
    static constexpr std::array<double, numLines> lineMs{31.7, 37.9, 41.3, 47.9, 53.1, 61.3, 67.9, 77.3};
    static constexpr std::array<double, numLines> modHz{0.11, 0.17, 0.23, 0.29, 0.37, 0.43, 0.53, 0.61};
    static constexpr double modDepthMs = 1.0;                    // 48 samples at 48 kHz peak to peak: at most about 3 cents
    static constexpr std::array<std::array<double, 4>, 2> diffMs{{{4.77, 3.59, 12.73, 9.31}, {5.03, 3.83, 13.19, 8.89}}};
    static constexpr std::array<float, 4> diffGain{0.70f, 0.70f, 0.60f, 0.60f};
    static constexpr std::size_t erTaps = 6;
    static constexpr std::array<double, erTaps> erMsL{7.3, 11.9, 17.3, 23.9, 31.1, 39.7};
    static constexpr std::array<double, erTaps> erMsR{8.9, 13.3, 19.1, 26.3, 29.9, 41.9};
    static constexpr std::array<float, erTaps> erGain{0.55f, -0.45f, 0.38f, -0.32f, 0.26f, -0.2f};
    static constexpr float erLevel = 0.35f;
    static constexpr std::array<float, numLines> inSign{1, -1, 1, -1, 1, -1, 1, -1};
    static constexpr std::array<float, numLines> outSignL{1, 1, 1, 1, -1, -1, -1, -1};
    static constexpr std::array<float, numLines> outSignR{1, -1, 1, -1, 1, -1, 1, -1};
    static constexpr double levelTrim = 0.35;                    // calibrated: about Classic's level at Size 50 %

    double sr = 0.0, room = 1.0, earlyRoom = 1.0;
    float size = 0.5f, damping = 0.5f, width = 1.0f, outGain = 1.0f, toneCoeff = 0.0f;
    std::array<std::array<float, 2>, 2> tone{};
    std::array<std::vector<float>, numLines> lines;
    std::array<std::array<Allpass, 4>, 2> diff;
    std::vector<std::array<float, 2>> early;
    std::array<std::size_t, numLines> wp{};                      // write positions
    std::size_t earlyPos = 0;
    std::array<double, numLines> len{}, lenTarget{}, modPhase{}, modStep{};
    std::array<float, numLines> lp{}, lpCoeff{}, gain{};
};

} // namespace lmw::dsp
