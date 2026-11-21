// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.5-presets: module presets (built-in list, file format). The values themselves are checked against
// the real parameters by the host test (section 4f, over 9P).
#include "app/ModulePresets.h"
#include <cstdio>
#include <set>

using namespace lmw::presets;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    CHECK(kindOf("env1") == Kind::Env && kindOf("env2") == Kind::Env);
    CHECK(kindOf("vcf2") == Kind::Vcf && kindOf("vco3") == Kind::Vco && kindOf("reverb") == Kind::Reverb);
    CHECK(kindOf("formant") == Kind::Formant && kindName(Kind::Formant) == "formant" && folderName(Kind::Formant) == "FORMANT");
    CHECK(kindOf("comb") == Kind::Comb && kindOf("delay") == Kind::Delay && folderName(Kind::Delay) == "DELAY");
    CHECK(!kindOf("lfo1") && !kindOf("vco4") && !kindOf("env") && !kindOf(""));

    // Every built-in preset sets every parameter of its module, in panel order; names are unique; the
    // instruments the user asked for are there (flute, recorder, oboe, clarinet).
    int count = 0;
    for (Kind k : {Kind::Env, Kind::Vcf, Kind::Vco, Kind::Reverb, Kind::Formant, Kind::Comb, Kind::Delay}) {
        std::set<std::string> names;
        for (const auto& p : builtIn(k)) {
            ++count;
            CHECK(!p.name.empty() && names.insert(p.name).second);
            CHECK(p.values.size() == keys(k).size());
            for (std::size_t i = 0; i < p.values.size() && i < keys(k).size(); ++i) {
                CHECK(p.values[i].first == keys(k)[i]);
                CHECK(!p.values[i].second.empty());
            }
            // write -> parse gives the same preset
            const auto back = parse(k, write(k, p));
            CHECK(back && back->name == p.name && back->values == p.values);
        }
        if (k == Kind::Comb)
            for (const char* n : {"Flute", "Oboe", "Clarinet", "Bassoon", "Guitar (plucked)"}) CHECK(names.count(n) == 1);
        if (k != Kind::Reverb && k != Kind::Formant && k != Kind::Delay && k != Kind::Comb)
            for (const char* n : {"Flute", "Recorder", "Oboe", "Clarinet"}) CHECK(names.count(n) == 1);
        CHECK(names.size() >= 6);
    }
    std::printf("%d built-in presets\n", count);
    CHECK(count == 99);                                        // 53 + 30 FORMANT (S5.16) + 9 COMB + 7 DELAY
    CHECK(builtIn(Kind::Formant).size() == 30 && builtIn(Kind::Formant).front().name == "Bass A"
          && builtIn(Kind::Formant).back().name == "Child U");

    // File format.
    CHECK(write(Kind::Env, {"Soft", {{"attack", "0.5"}, {"release", "2"}}})
          == "# LMW-Synth module preset (hand-editable; values as in patch files)\nformat = 1\nmodule = env\nname = Soft\n"
             "attack = 0.5\nrelease = 2\n");
    std::string error;
    auto p = parse(Kind::Env, "# mine\r\nformat = 1\r\nmodule = env\r\n\r\nattack=0.2\nsustain = 50%\nwhatever = 3\nattack = 0.3\n", &error);
    CHECK(p && p->name.empty() && p->values.size() == 2);             // unknown key ignored, attack once
    CHECK(p && p->values[0] == std::make_pair(std::string("attack"), std::string("0.3")) && p->values[1].second == "50%");
    CHECK(!parse(Kind::Vcf, "format = 1\nmodule = env\nattack = 1\n", &error) && error.find("another kind") != std::string::npos);
    CHECK(!parse(Kind::Env, "module = env\nattack = 1\n", &error) && error.find("format") != std::string::npos);
    CHECK(!parse(Kind::Env, "format = 2\nmodule = env\n", &error) && error.find("newer") != std::string::npos);
    CHECK(!parse(Kind::Env, "format = 1\nattack = 1\n", &error) && error.find("module") != std::string::npos);

    // File names.
    CHECK(fileName("My flute") == "My flute");
    CHECK(fileName("  a/b:c*?  ") == "a_b_c__");
    CHECK(fileName("   ") == "Preset" && fileName("") == "Preset");
    CHECK(fileName("Organ 16'") == "Organ 16'");

    if (failures == 0) std::printf("test_presets: all passed\n");
    return failures == 0 ? 0 : 1;
}
