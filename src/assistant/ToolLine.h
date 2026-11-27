// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.5: one line per tool call in the Assistant tab, in plain words ("set vcf1.cutoff 800",
// "applied preset Male voice (ooo)", "played C3 E3 G3 for 2 s"); a refused call adds the first line of why.
#pragma once
#include "mcp/Json.h"
#include <cstdio>
#include <string>

namespace lmw::assistant {

inline std::string noteName(int n) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (n < 0 || n > 127) return std::to_string(n);
    return std::string(names[n % 12]) + std::to_string(n / 12 - 1);       // 60 = C4
}

inline std::string toolLine(const std::string& tool, const json::Value& in, const std::string& result, bool isError) {
    const auto str = [&](const char* k) {
        const auto* v = in.get(k);
        return v != nullptr && v->isString() ? v->s : std::string();
    };
    const auto num = [](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%g", v);
        return std::string(b);
    };
    std::string line;
    if (tool == "set_parameters") {
        line = "set";
        if (const auto* v = in.get("values"); v != nullptr && v->isObject()) {
            int n = 0;
            for (const auto& [id, val] : v->o) {
                if (n++ == 6) { line += " and " + std::to_string(v->o.size() - 6) + " more"; break; }
                line += (n > 1 ? ", " : " ") + id + " " + (val.isNumber() ? num(val.n) : val.isString() ? val.s : val.dump());
            }
        }
    } else if (tool == "set_cables") {
        const auto* c = in.get("cables");
        const auto* clear = in.get("clear_first");
        line = clear != nullptr && clear->isBool() && clear->b ? "removed all cables" : "";
        if (c != nullptr && c->isArray() && !c->a.empty()) {
            line += line.empty() ? "cables:" : "; cables:";
            for (std::size_t i = 0; i < c->a.size(); ++i) line += (i ? ", " : " ") + (c->a[i].isString() ? c->a[i].s : c->a[i].dump());
        }
        if (line.empty()) line = "looked at the cables";
    } else if (tool == "apply_instrument_preset") line = "applied preset " + str("name");
    else if (tool == "apply_module_preset") line = "applied " + str("module") + " preset " + str("name");
    else if (tool == "play") {
        line = "played";
        if (const auto* n = in.get("notes"); n != nullptr && n->isArray())
            for (const auto& x : n->a) if (x.isNumber()) line += " " + noteName(static_cast<int>(x.n));
        const auto* s = in.get("seconds");
        line += " for " + (s != nullptr && s->isNumber() ? num(s->n) : std::string("1")) + " s";
    } else if (tool == "name_sound") line = "named the sound " + str("name");
    else if (tool == "save_instrument_preset") line = (result.rfind("Not saved", 0) == 0 ? "did not save " : "saved preset ") + str("name");
    else if (tool == "get_sound") line = "looked at the sound";
    else if (tool == "get_load") line = "checked the load";      // Spec S6.28
    else if (tool == "describe_synth") line = "read the parameter list";
    else if (tool == "list_presets") line = "read the preset list";
    else if (tool == "read_tutorial") line = "read the tutorial";
    else line = tool;
    if (isError) {
        std::string why = result;                                   // the first refused item, else the first line
        if (const auto r = why.find("Refused:\n"); r != std::string::npos) why = why.substr(r + 9);
        while (!why.empty() && why.front() == ' ') why.erase(0, 1);
        why = why.substr(0, why.find('\n'));
        line += " (refused: " + why + ")";
    }
    return line;
}

} // namespace lmw::assistant
