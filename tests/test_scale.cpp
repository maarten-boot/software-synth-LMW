// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.17-scale: fader scale kinds and ticks (src/ui/FaderScale.h, Spec S6.17).
#include "ui/FaderScale.h"
#include <cstdio>
#include <string>

namespace sc = lmw::ui::scale;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static std::string texts(const std::vector<sc::Tick>& t, bool majorsOnly = false) {
    std::string s;
    for (const auto& k : t) if (!majorsOnly || k.major) s += (s.empty() ? "" : " ") + k.text;
    return s;
}
static bool ordered(const std::vector<sc::Tick>& t) {
    for (std::size_t i = 1; i < t.size(); ++i) if (t[i].pos - t[i - 1].pos < 0.06 - 1e-9 || t[i].value <= t[i - 1].value) return false;
    return !t.empty() && t.front().pos >= 0.0 && t.back().pos <= 1.0;
}

int main() {
    // Kinds from unit, range and skew.
    CHECK(sc::kindOf("Hz", 20, 18000, 600.0) == sc::Kind::Log);
    CHECK(sc::kindOf("s", 0, 5, 0.5) == sc::Kind::Curve && sc::kindOf("st", 0, 12, 1.0) == sc::Kind::Curve);
    CHECK(sc::kindOf("st", -7, 7, std::nullopt) == sc::Kind::Pm && sc::kindOf("%", -0.95, 0.95, std::nullopt) == sc::Kind::Pm);
    CHECK(sc::kindOf("%", 0, 1, std::nullopt) == sc::Kind::Lin && sc::kindOf("dB", 0, 1, std::nullopt) == sc::Kind::Db);   // S5.31: dB = db
    CHECK(sc::word(sc::Kind::Pm) == "pm" && sc::tag(sc::Kind::Pm) == "\xc2\xb1" && sc::tag(sc::Kind::Curve) == "curve");
    CHECK(sc::describe(sc::Kind::Log, "600 Hz") == "Scale: logarithmic (half way = 600 Hz)");
    // JUCE's skew: the centre sits at half travel.
    CHECK(std::fabs(sc::proportion(600, 20, 18000, 600.0) - 0.5) < 1e-9 && std::fabs(sc::proportion(0.5, 0, 5, 0.5) - 0.5) < 1e-9);
    // Cutoff: 1-2-5 per decade (200 too close to 100 at this skew), majors at the powers of ten.
    const auto cut = sc::ticks(sc::Kind::Log, "Hz", 20, 18000, 600.0);
    CHECK(texts(cut) == "20 50 100 500 1k 2k 5k 10k" && texts(cut, true) == "100 1k 10k" && ordered(cut));
    const auto rate = sc::ticks(sc::Kind::Log, "Hz", 0.05, 20, 2.0);
    CHECK(texts(rate, true) == "0.1 1 10" && ordered(rate));
    // Times: 0, then 1-2-5 from 0.01 (0.02, 0.05 too close; the major 0.1 wins); the top always kept.
    const auto att = sc::ticks(sc::Kind::Curve, "s", 0, 5, 0.5);
    CHECK(texts(att) == "0 0.01 0.1 0.2 0.5 1 2 5" && texts(att, true) == "0 0.01 0.1 1 5" && ordered(att));
    const auto fm = sc::ticks(sc::Kind::Curve, "st", 0, 12, 1.0);   // 10 too close to the top 12: left out
    CHECK(texts(fm).find("10") == std::string::npos && fm.back().text == "12" && fm.back().major && ordered(fm));
    // Linear and around 0.
    CHECK(texts(sc::ticks(sc::Kind::Lin, "%", 0, 1, std::nullopt), true) == "0 50 100");
    CHECK(texts(sc::ticks(sc::Kind::Pm, "st", -7, 7, std::nullopt)) == "-7 -3.5 0 +3.5 +7");
    CHECK(texts(sc::ticks(sc::Kind::Pm, "oct", -5, 5, std::nullopt), true) == "-5 0 +5");
    CHECK(texts(sc::ticks(sc::Kind::Pm, "%", -0.95, 0.95, std::nullopt), true) == "-95 0 +95");
    CHECK(texts(sc::ticks(sc::Kind::Lin, "dB", 0, 1, std::nullopt), true) == "-inf -6 0");
    CHECK(texts(sc::ticks(sc::Kind::Lin, "vowel", 0, 4, std::nullopt)) == "A E I O U");
    if (failures == 0) std::printf("ok\n");
    else std::printf("%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
