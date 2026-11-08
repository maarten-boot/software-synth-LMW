// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.9: VCF modes — slope 6/12/24 dB and type LP/HP/BP/Notch from the ladder's stage outputs.
#include "dsp/Ladder.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace lmw::dsp;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECKV(cond, v) do { if (!(cond)) { std::printf("FAIL %s:%d: %s (value %.2f)\n", __FILE__, __LINE__, #cond, double(v)); ++failures; } } while (0)
static constexpr double SR = 96000.0;   // high rate: the analog-like slopes hold far from Nyquist
static constexpr double pi = 3.14159265358979323846;

// Steady-state gain in dB for a sine of `hz`.
static double gainDb(FilterSlope sl, FilterType ty, double cutoff, double hz, float res = 0.0f) {
    Ladder f; f.setSampleRate(SR); f.setCutoff(cutoff); f.setResonance(res); f.setMode(sl, ty); f.reset();
    const int n = 96000, settle = 48000;
    double in2 = 0, out2 = 0;
    for (int i = 0; i < n; ++i) {
        const double x = 0.05 * std::sin(2 * pi * hz * i / SR);
        const double y = f.process(static_cast<float>(x));
        if (i >= settle) { in2 += x * x; out2 += y * y; }
    }
    return 10 * std::log10(out2 / in2);
}

int main() {
    const double fc = 1000.0;
    const FilterSlope slopes[] = {FilterSlope::Db6, FilterSlope::Db12, FilterSlope::Db24};
    const double perOctave[] = {6.0, 12.0, 24.0};
    // T-S5.9a: lowpass / highpass slopes, one octave apart well beyond the cutoff.
    for (int i = 0; i < 3; ++i) {
        // 4 -> 8 kHz: far enough above the cutoff, and low enough that frequency warping near Nyquist
        // (8 -> 16 kHz is 1.1 "analog" octaves at 96 kHz) does not steepen the slope.
        const double lp = gainDb(slopes[i], FilterType::Lowpass, fc, 4000) - gainDb(slopes[i], FilterType::Lowpass, fc, 8000);
        CHECKV(std::fabs(lp - perOctave[i]) < 2.0, lp);
        const double hp = gainDb(slopes[i], FilterType::Highpass, fc, 125) - gainDb(slopes[i], FilterType::Highpass, fc, 62.5);
        CHECKV(std::fabs(hp - perOctave[i]) < 2.0, hp);
        CHECKV(std::fabs(gainDb(slopes[i], FilterType::Lowpass, fc, 50)) < 1.0, gainDb(slopes[i], FilterType::Lowpass, fc, 50));   // passbands flat
        CHECKV(std::fabs(gainDb(slopes[i], FilterType::Highpass, fc, 20000)) < 1.0, gainDb(slopes[i], FilterType::Highpass, fc, 20000));
    }
    // Bandpass: peak (about 0 dB) at the cutoff, falling off both sides; notch: deep dip at the cutoff.
    for (auto sl : {FilterSlope::Db12, FilterSlope::Db24}) {
        const double peak = gainDb(sl, FilterType::Bandpass, fc, fc);
        CHECKV(std::fabs(peak) < 1.5, peak);
        CHECK(gainDb(sl, FilterType::Bandpass, fc, fc / 4) < peak - 6.0);
        CHECK(gainDb(sl, FilterType::Bandpass, fc, fc * 4) < peak - 6.0);
        const double dip = gainDb(sl, FilterType::Notch, fc, fc);
        CHECKV(dip < -30.0, dip);
        CHECK(std::fabs(gainDb(sl, FilterType::Notch, fc, fc / 8)) < 1.0);
        CHECK(std::fabs(gainDb(sl, FilterType::Notch, fc, fc * 8)) < 1.0);
    }
    // 6 dB bandpass / notch act as 12 dB.
    CHECK(filterMix(FilterSlope::Db6, FilterType::Bandpass) == filterMix(FilterSlope::Db12, FilterType::Bandpass));
    CHECK(filterMix(FilterSlope::Db6, FilterType::Notch) == filterMix(FilterSlope::Db12, FilterType::Notch));

    // T-S5.9b: top resonance self-oscillates, bounded, in every mode.
    for (auto sl : slopes)
        for (auto ty : {FilterType::Lowpass, FilterType::Highpass, FilterType::Bandpass, FilterType::Notch}) {
            Ladder f; f.setSampleRate(SR); f.setCutoff(500); f.setResonance(1.0f); f.setMode(sl, ty); f.reset();
            f.process(1.0f);
            float late = 0, peak = 0;
            for (int i = 0; i < 192000; ++i) {
                const float y = f.process(0.0f);
                peak = std::fmax(peak, std::fabs(y));
                if (i > 180000) late = std::fmax(late, std::fabs(y));
            }
            CHECKV(std::isfinite(peak) && peak < 8.0f, peak);
            CHECKV(late > 0.05f, late);
        }
    // T-S5.9b: switching mode on a sounding signal crossfades: no jump.
    {
        Ladder f; f.setSampleRate(SR); f.setCutoff(1000); f.reset();
        float prev = 0, maxStep = 0;
        for (int i = 0; i < 48000; ++i) {
            if (i == 24000) f.setMode(FilterSlope::Db24, FilterType::Highpass);   // 100 Hz: passes LP, not HP
            const float y = f.process(static_cast<float>(0.5 * std::sin(2 * pi * 100 * i / SR)));
            if (i > 12000) maxStep = std::fmax(maxStep, std::fabs(y - prev));
            prev = y;
        }
        CHECKV(maxStep < 0.01f, maxStep);         // the sine alone changes up to ~0.0033 per sample
        CHECK(std::fabs(prev) < 0.01f);           // and it really switched to highpass
    }
    if (failures == 0) std::printf("T-S5.9-vcfmodes: all passed\n");
    return failures == 0 ? 0 : 1;
}
