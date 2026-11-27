// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.7-instruments: built-in whole-instrument presets. Unique names; every module-preset instrument name
// (the ENV list) is there, plus the voice and FM extras; each preset names voice.count and a reverb; the
// module-based ones carry their ENV 1/ENV 2/VCF 1/VCO 1 module presets; every id is module.key; cable texts
// parse. The values themselves are checked against the real parameters by the host test (4h, over 9P).
#include "app/InstrumentPresets.h"
#include "dsp/PatchText.h"
#include <cstdio>
#include <algorithm>
#include <map>
#include <set>
#include <cctype>

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
    for (const char* extra : {"Choir (aah)", "Male voice (ooo)", "Violin (PM vibrato)", "Electric piano (FM)", "Choral flute"})
        CHECK(names.count(extra) == 1);
    // Flute: its module presets on ENV 1, ENV 2, VCF 1 and VCO 1, one voice.
    for (const auto& i : list)
        if (i.name == "Flute") {
            int modules = 0;
            for (const auto& [id, value] : i.values)
                modules += (id.rfind("env1.", 0) == 0 || id.rfind("env2.", 0) == 0 || id.rfind("vcf1.", 0) == 0 || id.rfind("vco1.", 0) == 0) ? 1 : 0;
            CHECK(modules == 4 + 4 + 6 + 6);   // VCO: + shape (S5.28)
            bool solo = false; for (const auto& [id, value] : i.values) solo = solo || (id == "voice.count" && value == "1");
            CHECK(solo);
        }
    // S5.24: the solo instruments with vibrato start it after the attack (lfo1.delay 0.4 s); the others have none.
    {
        const std::set<std::string> delayed{"Flute", "Oboe", "Clarinet", "Bassoon", "Trumpet", "French horn", "Violin", "Male voice (ooo)", "Choral flute", "Hollow lead"};
        for (const auto& i : list) {
            std::string d;
            for (const auto& [id, value] : i.values) if (id == "lfo1.delay") d = value;
            if (i.group.empty()) CHECK(delayed.count(i.name) ? d == "0.4" : d.empty());   // (World: their own)
        }
    }
    // Spec S7.16: every instrument has a family; the main ones once in the menu, each world one also in World; the
    // families are the menu's; the new Synth presets use their feature.
    {
        std::map<std::string, int> perFamily;
        for (const auto& i : list) {
            CHECK(!i.family.empty());
            CHECK(std::find(lmw::instruments::families.begin(), lmw::instruments::families.end(), i.family) != lmw::instruments::families.end());
            ++perFamily[i.family];
        }
        for (const auto& [f, n] : perFamily) std::printf("  %s: %d\n", f.c_str(), n);
        // Presets > All (user 2026-11-27): every built-in once, A-Z ignoring case — names unique ignoring case
        {
            std::set<std::string> lower;
            for (const auto& i : list) {
                std::string l = i.name;
                for (auto& ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                lower.insert(l);
            }
            CHECK(lower.size() == list.size());
        }
        CHECK(perFamily["Woodwinds"] == 10 && perFamily["Brass"] == 3 && perFamily["Strings"] == 4 && perFamily["Keyboards"] == 4
              && perFamily["Plucked & mallets"] == 9 && perFamily["Voices"] == 2 && perFamily["Synth"] == 8);
        auto value = [&](const std::string& name, const std::string& id) {
            for (const auto& i : list) if (i.name == name) for (const auto& [k, v] : i.values) if (k == id) return v;
            return std::string();
        };
        auto cables = [&](const std::string& name) { for (const auto& i : list) if (i.name == name) return i.cables; return std::string(); };
        CHECK(value("Acid bass", "vcf1.res") == "0.85" && value("Acid bass", "keys.glidemode") == "Legato");
        CHECK(value("Filter sine", "mix.vco1") == "0" && value("Filter sine", "vcf1.res") == "1" && value("Filter sine", "vcf1.key") == "1");
        CHECK(value("Hollow lead", "vco1.shape") == "1" && value("Hollow lead", "delay.mix") != "");
        CHECK(value("Ping-pong pluck", "delay.spread") == "1");
        CHECK(value("Wobble pad", "lfo2.shape") == "0.9" && cables("Wobble pad").find("lfo2.out") != std::string::npos);
        CHECK(value("Shimmer space", "effects.order") == "reverb, comb, ensemble, delay");
        for (const char* n : {"Trumpet", "French horn", "Piano", "Harpsichord", "Electric piano (FM)"})
            CHECK(cables(n).find("vcf1.cutoff=env2.out+keys.vel@0.4") != std::string::npos);   // velocity brightens, ENV 2 kept
    }
    std::printf("%zu built-in instrument presets\n", list.size());
    CHECK(list.size() == 40);   // + 6 Synth (S7.16), + PWM strings (S5.28), + Choral flute (S5.26), + 13 world instruments (S7.13)
    {   // S7.13: the World group, 13 instruments
        int world = 0;
        for (const auto& i : list) world += i.group == "World" ? 1 : 0;
        CHECK(world == 13);
        for (const char* n : {"Shakuhachi", "Bansuri", "Pan flute", "Duduk", "Erhu", "Shamisen", "Koto", "Sitar", "Oud",
                              "Kalimba", "Gamelan", "Steel pan", "Didgeridoo"})
            CHECK(names.count(n) == 1);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S7.7-instruments: all passed\n", failures);
    return failures ? 1 : 0;
}
