// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.7-instruments: built-in whole-instrument presets. Unique names; every module-preset instrument name
// (the ENV list) is there, plus the voice and FM extras; each preset names voice.count and a reverb; the
// module-based ones carry their ENV 1/ENV 2/VCF 1/VCO 1 module presets; every id is module.key; cable texts
// parse. The values themselves are checked against the real parameters by the host test (4h, over 9P).
#include "app/InstrumentPresets.h"
#include "dsp/PatchText.h"
#include <cstdio>
#include <set>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    using namespace lmw;
    const auto& list = instruments::builtIn();
    std::set<std::string> names;
    for (const auto& i : list) {
        CHECK(!i.name.empty() && names.insert(i.name).second);
        bool voices = false, reverb = false;
        for (const auto& [id, value] : i.values) {
            CHECK(id.find('.') != std::string::npos && id.find('.') == id.rfind('.') && !value.empty());
            voices = voices || id == "voice.count";
            reverb = reverb || id == "reverb.mix";
        }
        CHECK(voices && reverb);
        if (!i.cables.empty()) CHECK(dsp::patchFromText(i.cables).has_value());
    }
    for (const auto& p : presets::builtIn(presets::Kind::Env)) CHECK(names.count(p.name) == 1);   // the same names (user)
    for (const char* extra : {"Choir (aah)", "Male voice (ooo)", "Violin (PM vibrato)", "Electric piano (FM)"})
        CHECK(names.count(extra) == 1);
    // Flute: its module presets on ENV 1, ENV 2, VCF 1 and VCO 1, one voice.
    for (const auto& i : list)
        if (i.name == "Flute") {
            int modules = 0;
            for (const auto& [id, value] : i.values)
                modules += (id.rfind("env1.", 0) == 0 || id.rfind("env2.", 0) == 0 || id.rfind("vcf1.", 0) == 0 || id.rfind("vco1.", 0) == 0) ? 1 : 0;
            CHECK(modules == 4 + 4 + 6 + 5);
            bool solo = false; for (const auto& [id, value] : i.values) solo = solo || (id == "voice.count" && value == "1");
            CHECK(solo);
        }
    std::printf("%zu built-in instrument presets\n", list.size());
    CHECK(list.size() == 19);
    std::printf(failures ? "%d FAILED\n" : "T-S7.7-instruments: all passed\n", failures);
    return failures ? 1 : 0;
}
