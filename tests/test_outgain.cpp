// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S5.31-out (unit): OUT Volume up to +12 dB and the limiter lamps. The dB scale (kind, 0 dB at about three
// quarters of the travel, marks); the limiters' reduction (0 below the threshold, more above); the lamp meter
// (lit for 0.3 s, the most taken off held for 1 s); the engine's peak into OUT's limiter.
#include "dsp/Engine.h"
#include "dsp/SoftLimiter.h"
#include "ui/FaderScale.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace lmw;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    // The scale of OUT Volume: 0 .. 4 with centre 0.14169 (as PluginProcessor.cpp).
    {
        namespace sc = ui::scale;
        const double lo = 0.0, hi = 4.0, centre = 0.14169;
        CHECK(sc::kindOf("dB", lo, hi, centre) == sc::Kind::Db && sc::word(sc::Kind::Db) == "db");
        const double p0 = sc::proportion(1.0, lo, hi, centre), pDef = sc::proportion(0.8, lo, hi, centre);
        std::printf("0 dB at %.3f of the travel, the default -1.9 dB at %.3f\n", p0, pDef);
        CHECK(p0 > 0.70 && p0 < 0.80 && pDef > 0.65 && pDef < p0);
        std::string marks;
        for (const auto& t : sc::ticks(sc::Kind::Db, "dB", lo, hi, centre)) marks += t.text + (t.major ? "* " : " ");
        std::printf("marks: %s\n", marks.c_str());
        CHECK(marks.find("-inf*") != std::string::npos && marks.find("0*") != std::string::npos && marks.find("+12*") != std::string::npos);
        CHECK(marks.find("+6 ") != std::string::npos && marks.find("-6 ") != std::string::npos);
    }
    // Reductions.
    {
        using dsp::reductionDb;
        CHECK(reductionDb<dsp::SoftLimiter>(0.4f) == 0.0f && reductionDb<dsp::MainLimiter>(0.85f) == 0.0f);
        const float o = reductionDb<dsp::SoftLimiter>(1.0f), m = reductionDb<dsp::MainLimiter>(1.2f);
        std::printf("OUT limiter at 0 dBFS takes %.2f dB, MAIN at +1.6 dBFS %.2f dB\n", o, m);
        CHECK(o > 0.5f && o < 3.0f && m > 1.0f && m < 3.0f);
        CHECK(reductionDb<dsp::SoftLimiter>(4.0f) > reductionDb<dsp::SoftLimiter>(1.0f));
        CHECK(reductionDb<dsp::SoftLimiter>(std::nanf("")) == 0.0f);
    }
    // The lamp meter.
    {
        dsp::LimitMeter l;
        l.block(0.0f, 9.0);
        CHECK(!l.lit(9.0) && l.worst(9.0) == 0.0f);
        l.block(2.4f, 10.0);
        l.block(1.0f, 10.2);                                   // smaller: the 2.4 stays held
        CHECK(l.lit(10.25) && l.lit(10.45) && !l.lit(10.55));  // 0.3 s after the last working block
        CHECK(l.worst(10.5) == 2.4f && l.worst(11.05) == 0.0f);
        l.block(0.0f, 11.1);                                   // a new window
        CHECK(l.worst(11.2) == 0.0f && !l.lit(11.2));
    }
    // The engine's peak into OUT's limiter: a loud chord at x4 goes over the knee; the output itself is unchanged
    // by the measuring (the same samples with and without taking the peak).
    {
        auto play = [](float gain, bool take, float& peak) {
            dsp::Engine e; e.prepare(48000.0); e.setGain(gain);
            dsp::VoiceParams p; p.attack = 0.0f; p.decay = 0.0f; p.sustain = 1.0f; p.release = 0.01f; e.setParams(p);
            for (int n : {48, 52, 55, 60}) e.noteOn(n, 1.0f);
            std::vector<float> l(4800); e.render(l.data(), nullptr, 4800);
            peak = take ? e.takeOutPeak() : 0.0f;
            if (take) CHECK(e.takeOutPeak() == 0.0f);          // reset by taking it
            return l;
        };
        float loud = 0, quiet = 0, unused = 0;
        const auto a = play(4.0f, true, loud), b = play(4.0f, false, unused);
        play(0.1f, true, quiet);
        std::printf("peak into OUT's limiter: x4 %.3f (%.1f dB off), x0.1 %.3f\n", loud, dsp::reductionDb<dsp::SoftLimiter>(loud), quiet);
        CHECK(a == b);
        CHECK(loud > dsp::SoftLimiter::threshold && dsp::reductionDb<dsp::SoftLimiter>(loud) > 1.0f);
        CHECK(quiet < dsp::SoftLimiter::threshold && dsp::reductionDb<dsp::SoftLimiter>(quiet) == 0.0f);
    }
    if (failures == 0) std::printf("T-S5.31-out: all passed\n");
    return failures == 0 ? 0 : 1;
}
