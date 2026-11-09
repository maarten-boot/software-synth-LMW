// SPDX-License-Identifier: AGPL-3.0-or-later
// Human-readable parameter values (Spec S6.1): shown under the faders and by plugin hosts.
// Pure C++ so it is unit-tested without JUCE (tests/test_paramtext.cpp).
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>

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

// Frequency: "5.00 Hz", "50.0 Hz", "500 Hz", "5.00 kHz", "18.0 kHz"; accepts "2k", "2 kHz", "440".
inline std::string hertz(float hz) {
    if (hz < 10.0f)    return printf1("%.2f Hz", hz);
    if (hz < 100.0f)   return printf1("%.1f Hz", hz);
    if (hz < 999.5f)   return printf1("%.0f Hz", hz);
    if (hz < 9995.0f)  return printf1("%.2f kHz", hz / 1000.0f);
    return printf1("%.1f kHz", hz / 1000.0f);
}
inline float parseHertz(const std::string& s) {
    const double v = std::strtod(s.c_str(), nullptr);
    return static_cast<float>(s.find_first_of("kK") != std::string::npos ? v * 1000.0 : v);
}

// Signed octaves "+2.0 oct", "-1.5 oct", "0.0 oct"; semitones "1.0 st".
inline std::string octaves(float v) {
    if (std::fabs(v) < 0.05f) return "0.0 oct";
    return printf1("%+.1f oct", v);
}
inline std::string semitones(float v) { return printf1("%.1f st", v); }
inline float parseNumber(const std::string& s) { return static_cast<float>(std::strtod(s.c_str(), nullptr)); }

// Spec S10.10, S7.1: a choice given as text -> its index, or -1. In order: the exact name (any
// case); the name with " dB" and a leading "+" ignored ("24" -> "24 dB", "1" -> "+1"); a unique
// prefix ("low" -> "Lowpass"); finally a plain index ("2" for the third choice).
inline int matchChoice(const std::vector<std::string>& names, std::string_view text) {
    auto lower = [](std::string_view v) {
        std::string r;
        for (char c : v) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        while (!r.empty() && std::isspace(static_cast<unsigned char>(r.back()))) r.pop_back();
        while (!r.empty() && std::isspace(static_cast<unsigned char>(r.front()))) r.erase(r.begin());
        return r;
    };
    auto plain = [&](std::string_view v) {
        std::string r = lower(v);
        if (r.size() > 3 && r.compare(r.size() - 3, 3, " db") == 0) r.resize(r.size() - 3);
        if (r.size() > 2 && r.compare(r.size() - 2, 2, "db") == 0) r.resize(r.size() - 2);
        if (!r.empty() && r.front() == '+') r.erase(r.begin());
        return r;
    };
    const std::string t = lower(text), tp = plain(text);
    if (t.empty()) return -1;
    for (std::size_t i = 0; i < names.size(); ++i) if (lower(names[i]) == t) return static_cast<int>(i);
    for (std::size_t i = 0; i < names.size(); ++i) if (plain(names[i]) == tp) return static_cast<int>(i);
    int found = -1;
    for (std::size_t i = 0; i < names.size(); ++i)
        if (lower(names[i]).rfind(t, 0) == 0) { if (found >= 0) { found = -2; break; } found = static_cast<int>(i); }
    if (found >= 0) return found;
    if (t.find_first_not_of("0123456789") == std::string::npos && t.size() < 4) {
        const int index = std::stoi(t);
        if (index < static_cast<int>(names.size())) return index;
    }
    return -1;
}

} // namespace lmw::ui::text
