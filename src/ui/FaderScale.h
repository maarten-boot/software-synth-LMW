// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.17 (Phase 24c): how a fader's travel maps to its value, shown as ticks beside the track, a tag
// under the name and a hover line; reported by /info. Independent of JUCE so it can be unit-tested.
// Kinds: lin (equal steps), pm (linear around 0, shown "±"), log (frequencies and rates, skewed so equal
// travel is about an equal pitch step), curve (times and depths: more travel at small values). The kind
// follows from the parameter: skewed with unit Hz = log, skewed otherwise = curve, a range through 0 = pm.
// Spec S5.31: unit dB (OUT Volume, a gain) = db: marks in decibels (-inf, -12, -6, 0, +6, +12).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::ui::scale {

enum class Kind { Lin, Pm, Log, Curve, Db };

inline Kind kindOf(std::string_view unit, double lo, double hi, std::optional<double> centre) {
    if (unit == "dB") return Kind::Db;                 // Spec S5.31
    if (centre) return unit == "Hz" ? Kind::Log : Kind::Curve;
    return lo < 0.0 && hi > 0.0 ? Kind::Pm : Kind::Lin;
}
inline std::string_view word(Kind k) {          // /info "scale"
    switch (k) { case Kind::Pm: return "pm"; case Kind::Log: return "log"; case Kind::Curve: return "curve"; case Kind::Db: return "db"; default: return "lin"; }
}
inline std::string_view tag(Kind k) {           // under the fader name (UTF-8)
    switch (k) { case Kind::Pm: return "\xc2\xb1"; case Kind::Log: return "log"; case Kind::Curve: return "curve"; case Kind::Db: return "dB"; default: return "lin"; }
}
// The hover line; `half` is the value text at half travel (skewed kinds).
inline std::string describe(Kind k, const std::string& half) {
    switch (k) {
        case Kind::Pm:    return "Scale: linear around 0";
        case Kind::Log:   return "Scale: logarithmic (half way = " + half + ")";
        case Kind::Curve: return "Scale: curved, more travel at small values (half way = " + half + ")";
        case Kind::Db:    return "Scale: decibels, 0 dB at three quarters (half way = " + half + ")";
        default:          return "Scale: linear";
    }
}

// Proportion of travel (0..1) of a value: JUCE's skew for a centre (NormalisableRange::setSkewForCentre).
inline double proportion(double v, double lo, double hi, std::optional<double> centre) {
    const double p = std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
    if (!centre) return p;
    const double skew = std::log(0.5) / std::log((*centre - lo) / (hi - lo));
    return std::pow(p, skew);
}

// A short value text for a tick (8 px): 1k, 0.1, 50, +5, A; `sign`: "+" before positive values (pm).
inline std::string label(double v, std::string_view unit, bool sign = false) {
    char b[24];
    const auto g = [&](double x, const char* fmt) { std::snprintf(b, sizeof b, fmt, x); return std::string(b); };
    if (unit == "Hz") return v >= 1000.0 ? g(v / 1000.0, "%gk") : g(v, "%g");
    if (unit == "%") return g(std::round(v * 100.0), sign && v > 0.0 ? "%+g" : "%g");
    if (unit == "dB") {                                // -inf, -6, 0, +6 (S5.31: a sign above 0)
        if (v <= 0.0) return "-inf";
        const double db = std::round(20.0 * std::log10(v));
        return db == 0.0 ? std::string("0") : g(db, "%+g");
    }
    if (unit == "vowel") { static const char* names[] = {"A", "E", "I", "O", "U"}; return names[std::clamp(static_cast<int>(std::lround(v)), 0, 4)]; }
    return g(v, sign && v > 0.0 ? "%+g" : "%g");
}

struct Tick {
    double value, pos;       // value and its proportion of travel (0 = bottom)
    bool major;              // longer; carries a label where there is room
    std::string text;
};

// The ticks of a fader. lin: quarters (majors 0, 1/2, 1); pm: ends, half ways and 0 (majors ends and 0);
// log: 1-2-5 per decade in range (majors the powers of ten); curve: 0 and 1-2-5 from a tenth of the centre's
// decade (majors 0, powers of ten, the top). Ticks closer than 6 % of the travel to a kept one are left out;
// the top and a major replace a minor that is too close.
inline std::vector<Tick> ticks(Kind k, std::string_view unit, double lo, double hi, std::optional<double> centre) {
    std::vector<std::pair<double, bool>> v;
    const auto isDecade = [](double x) { const double l = std::log10(x); return std::fabs(l - std::round(l)) < 1e-9; };
    if (k == Kind::Db) {                               // Spec S5.31: -inf, -12, -6, 0, +6, +12 dB (majors -inf, 0, the top)
        v.push_back({lo, true});
        for (const double db : {-12.0, -6.0, 0.0, 6.0, 12.0}) {
            const double x = std::pow(10.0, db / 20.0);
            if (x > lo && x <= hi * (1 + 1e-6)) v.push_back({x, db == 0.0 || db == 12.0});
        }
    } else if (k == Kind::Lin || k == Kind::Pm) {
        for (int i = 0; i <= 4; ++i) v.push_back({lo + (hi - lo) * i / 4.0, i % 2 == 0});
        if (k == Kind::Pm) v[1].second = v[3].second = false, v[2].second = true;
    } else {
        double start;
        if (k == Kind::Log) start = std::pow(10.0, std::floor(std::log10(lo) + 1e-9));
        else { v.push_back({lo, true}); start = std::pow(10.0, std::floor(std::log10(*centre) + 1e-9)) / 10.0; }
        for (double d = start; d <= hi * (1 + 1e-9); d *= 10.0)
            for (double m : {1.0, 2.0, 5.0}) {
                const double x = d * m;
                if (x >= lo * (1 - 1e-9) && x > (k == Kind::Curve ? lo : 0.0) && x <= hi * (1 + 1e-9)) v.push_back({x, isDecade(x)});
            }
        if (k == Kind::Curve && (v.empty() || std::fabs(v.back().first - hi) > 1e-9 * hi)) v.push_back({hi, true});
        if (k == Kind::Curve) v.back().second = true;
    }
    std::vector<Tick> out;
    for (const auto& [x, major] : v) {
        const double p = proportion(x, lo, hi, centre);
        const bool top = std::fabs(x - hi) < 1e-9 * std::max(1.0, std::fabs(hi));
        bool keep = true;
        while (keep && !out.empty() && p - out.back().pos < 0.06) {
            if (top || (major && !out.back().major)) out.pop_back();   // the top and majors win over a minor
            else keep = false;
        }
        if (keep) out.push_back({x, p, major, label(x, unit, k == Kind::Pm)});
    }
    return out;
}

} // namespace lmw::ui::scale
