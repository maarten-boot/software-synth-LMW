// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S6.1-text: parameter value text (fader value boxes, host display) and parsing of typed values.
#include "ui/ParamText.h"
#include <cmath>
#include <cstdio>

using namespace lmw::ui::text;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
static bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }

int main() {
    CHECK(time(0.0f) == "0 ms");
    CHECK(time(0.01f) == "10 ms");
    CHECK(time(0.3115628f) == "312 ms");
    CHECK(time(0.9999f) == "1.00 s");
    CHECK(time(2.5f) == "2.50 s");
    CHECK(time(10.0f) == "10.0 s");
    CHECK(near(parseTime("250 ms"), 0.25f));
    CHECK(near(parseTime("0.25 s"), 0.25f));
    CHECK(near(parseTime("1.5s"), 1.5f));
    CHECK(near(parseTime("2"), 2.0f));
    CHECK(near(parseTime("250"), 0.25f));

    CHECK(percent(0.7f) == "70 %");
    CHECK(percent(0.0f) == "0 %");
    CHECK(percent(1.0f) == "100 %");
    CHECK(near(parsePercent("70 %"), 0.7f));
    CHECK(near(parsePercent("70"), 0.7f));
    CHECK(near(parsePercent("0.7"), 0.7f));

    CHECK(decibels(0.0f) == "-inf dB");
    CHECK(decibels(1.0f) == "0.0 dB");
    CHECK(decibels(0.5f) == "-6.0 dB");
    CHECK(decibels(0.8f) == "-1.9 dB");
    CHECK(near(parseDecibels("-6.0 dB"), 0.501187f));
    CHECK(near(parseDecibels("0"), 1.0f));
    CHECK(parseDecibels("-inf dB") == 0.0f);

    // Round trip: text shown for a value parses back to (nearly) the same value.
    for (float v : {0.005f, 0.2f, 0.75f, 3.3f, 9.0f}) CHECK(std::fabs(parseTime(time(v)) - v) <= 0.006f);
    for (float v : {0.1f, 0.5f, 0.8f, 1.0f})          CHECK(std::fabs(parseDecibels(decibels(v)) - v) <= 0.01f);

    std::printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
