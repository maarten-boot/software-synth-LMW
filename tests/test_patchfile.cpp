// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.1: patch file text format (write, read back, notes verbatim, robustness).
#include "app/PatchFile.h"
#include <cstdio>

using namespace lmw::patchfile;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    Patch p;
    p.stateVersion = 3;
    p.name = "Fat bass #2";
    p.parameters = {{"vcf1.cutoff", "1800", "1.80 kHz"}, {"vco1.wave", "Saw", ""}, {"voice.count", "4", ""}};
    p.cables = {{"vco1.fm", "env2.out"}, {"vcf2.in", "vcf2.out"}};
    p.notes = "Play low.\n# not a comment here\n[parameters]\nvco1.wave = Sine\n\n  indented, trailing spaces  \nno newline at end";

    const auto text = write(p);
    const auto q = parse(text);
    CHECK(q.has_value());
    if (q) {
        CHECK(q->format == 1 && q->stateVersion == 3);
        CHECK(q->name == "Fat bass #2");
        CHECK(q->parameters.size() == 3);
        CHECK(q->parameters[0].id == "vcf1.cutoff" && q->parameters[0].value == "1800");   // comment dropped
        CHECK(q->parameters[1].value == "Saw" && q->parameters[2].value == "4");
        CHECK(q->cables.size() == 2 && q->cables[1].first == "vcf2.in" && q->cables[1].second == "vcf2.out");
        CHECK(q->notes == p.notes);                                    // exactly, '#' and '[' included
        CHECK(write(*q).size() <= text.size());                          // only the comments are lost
    }

    {   // Spec S6.7: [schematic] written only when there are moved blocks, before [notes]; read back;
        // an unknown section (from a newer version) is skipped, not read as cables.
        Patch s = p;
        CHECK(write(s).find("[schematic]") == std::string::npos);
        s.schematic = {{"lfo2", "110, 56"}, {"vcf2", "88, 21"}};
        const auto st = write(s);
        CHECK(st.find("[schematic]\nlfo2 = 110, 56\nvcf2 = 88, 21\n") != std::string::npos);
        CHECK(st.find("[schematic]") < st.find("[notes]"));
        const auto sq = parse(st);
        CHECK(sq && sq->schematic == s.schematic && sq->notes == p.notes && sq->cables == p.cables);
        const auto u = parse("format = 1\n[cables]\nout.in = vca.out\n[future]\nwhatever = 3\nvca.in = x\n[schematic]\nkeys = 2, 30\n");
        CHECK(u && u->cables.size() == 1 && u->schematic.size() == 1 && u->schematic[0].second == "2, 30");
    }

    // Hand-edited file: CRLF line ends, blank lines, comments, unknown keys, spaces.
    const auto h = parse("# my patch\r\nformat=1\r\n\r\nname =  Pad  \r\n[parameters]\r\n  vcf1.res = 0.4   # 40 %\r\nnonsense\r\n= 3\r\n[cables]\r\nout.in = noise.out\r\n");
    CHECK(h.has_value());
    if (h) {
        CHECK(h->name == "Pad" && h->stateVersion == 0);
        CHECK(h->parameters.size() == 1 && h->parameters[0].id == "vcf1.res" && h->parameters[0].value == "0.4");
        CHECK(h->cables.size() == 1 && h->cables[0].second == "noise.out");
        CHECK(h->notes.empty());
    }
    // Not a patch / unsupported: no format line, a newer format, empty file.
    CHECK(!parse("vcf1.res = 0.4\n").has_value());
    CHECK(!parse("format = 2\n").has_value());
    CHECK(!parse("").has_value());
    CHECK(!parse("format = x\n").has_value());
    // Empty notes section and a name with a line break (written as one line).
    Patch e; e.name = "two\nlines";
    const auto er = parse(write(e));
    CHECK(er.has_value() && er->name == "two lines" && er->notes.empty() && er->parameters.empty());

    if (failures == 0) std::printf("T-S7.1-patchfile: all passed\n");
    return failures == 0 ? 0 : 1;
}
