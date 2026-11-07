// SPDX-License-Identifier: AGPL-3.0-or-later
// Human-readable parameter values (Spec S6.1): shown under the faders and by plugin hosts.
// Pure C++ so it is unit-tested without JUCE (tests/test_paramtext.cpp).
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace lmw::ui::text {

inline std::string printf1(const char* fmt, double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, fmt, v);
    return buf;
}

// Seconds: "0 ms" .. "999 ms", then "1.00 s" .. "10.0 s".
inline std::string time(float sec) {
    if (sec < 0.9995f) return printf1("%.0f ms", std::round(sec * 1000.0f));
    if (sec < 9.995f)  return printf1("%.2f s", sec);
    return printf1("%.1f s", sec);
}

// Accepts "250", "250 ms", "0.25 s", "1.5s". A bare number below 20 is seconds, otherwise ms.
inline float parseTime(const std::string& s) {
    const double v = std::strtod(s.c_str(), nullptr);
    if (s.find("ms") != std::string::npos) return static_cast<float>(v / 1000.0);
    if (s.find('s') != std::string::npos)  return static_cast<float>(v);
    return static_cast<float>(v < 20.0 ? v : v / 1000.0);
}

// 0..1 shown as "0 %" .. "100 %"; accepts "70", "70 %" or "0.7".
inline std::string percent(float v) { return printf1("%.0f %%", std::round(v * 100.0f)); }
inline float parsePercent(const std::string& s) {
    const double v = std::strtod(s.c_str(), nullptr);
    return static_cast<float>(s.find('%') == std::string::npos && v <= 1.0 ? v : v / 100.0);
}

// Linear gain 0..1 shown in dB ("-inf dB" for 0); accepts "-6", "-6 dB" or "-inf".
inline std::string decibels(float gain) {
    if (gain <= 0.0f) return "-inf dB";
    const double db = 20.0 * std::log10(gain);
    return printf1("%.1f dB", std::fabs(db) < 0.05 ? 0.0 : db);
}
inline float parseDecibels(const std::string& s) {
    if (s.find("inf") != std::string::npos) return 0.0f;
    return static_cast<float>(std::pow(10.0, std::strtod(s.c_str(), nullptr) / 20.0));
}

} // namespace lmw::ui::text
