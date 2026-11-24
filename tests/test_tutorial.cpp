// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.9-tutorial: the lesson presets. The document text is built in (TutorialText.h) and starts as the
// file does; lessons 1-12 each have a title and a section that starts at its heading, ends before the
// next ruler and holds its steps; every lesson sound names a known base and parseable cables.
#include "app/Tutorial.h"
#include "app/InstrumentPresets.h"
#include "dsp/PatchText.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    using namespace lmw;
    const auto& doc = tutorial::text();
    CHECK(doc.rfind("# LMW-Synth tutorial", 0) == 0);
    CHECK(doc.find("## Lesson sounds") != std::string::npos);            // the last part arrived too
    CHECK(tutorial::lessons().size() == 12);
    for (const auto& l : tutorial::lessons()) {
        const auto t = tutorial::title(l.number), s = tutorial::section(l.number);
        CHECK(!t.empty() && t.find("\xe2\x80\x94") == std::string::npos);
        CHECK(s.rfind("## Lesson " + std::to_string(l.number) + " ", 0) == 0);
        CHECK(s.find("\n---") == std::string::npos && s.find("1. ") != std::string::npos);
        CHECK(tutorial::notes(l.number).rfind("Tutorial lesson " + std::to_string(l.number), 0) == 0);
        bool baseKnown = l.base.empty() || l.base == "Default";
        for (const auto& ins : instruments::builtIn()) baseKnown = baseKnown || ins.name == l.base;
        CHECK(baseKnown);
        if (!l.cables.empty()) CHECK(dsp::patchFromText(l.cables).has_value());
    }
    CHECK(tutorial::title(1) == "First sound" && tutorial::title(9) == "Effects: comb, ensemble, delay and reverb");
    CHECK(tutorial::section(13).empty() && tutorial::title(13).empty());
    CHECK(tutorial::title(12) == "The assistant: describe a sound in words" && tutorial::notes(12).find("Test connection") != std::string::npos);
    // Notes are plain text: no emphasis marks, headings without "##", code fences dropped, "\*" a star.
    CHECK(tutorial::plain("## Lesson 1 — X\n**bold** and *it*, `code`, a \\* star\n```sh\nls\n```") ==
          "Lesson 1 — X\nbold and it, code, a * star\nls\n");
    // Image lines (screenshots) and the blank line after them stay out of the notes.
    CHECK(tutorial::plain("a\n\n![alt](../images/x.png)\n\nb") == "a\n\nb");
    CHECK(tutorial::plain("a\n   ![alt](x.png)\nb") == "a\nb");
    for (const auto& l : tutorial::lessons()) {
        const auto n = tutorial::notes(l.number);
        CHECK(n.find("**") == std::string::npos && n.find("## ") == std::string::npos && n.find("```") == std::string::npos && n.find("![") == std::string::npos);
    }
    CHECK(tutorial::notes(11).find("lmw9p unix!") != std::string::npos);   // the commands survive
    std::printf(failures ? "%d FAILED\n" : "T-S7.9-tutorial: all passed\n", failures);
    return failures ? 1 : 0;
}
