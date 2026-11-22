// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S11: the Model Context Protocol server of lmw-mcp. handle() takes one JSON-RPC message (a line from
// stdin) and returns the answer line (none for a notification). The synth is reached through Link (9P in
// lmw-mcp, a fake in tests/test_mcp.cpp), so everything here is tested without a running synth.
#pragma once
#include "app/InstrumentPresets.h"
#include "app/ModulePresets.h"
#include "mcp/Json.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::mcp {

// The synth side: one connection at a time (9P files as in Spec S10.10).
struct Link {
    virtual ~Link() = default;
    virtual std::vector<std::string> running() = 0;                                    // newest first
    virtual bool connect(const std::string& address, std::string& error) = 0;
    virtual bool connected() const = 0;
    virtual std::string address() const = 0;
    virtual std::optional<std::string> read(const std::string& path, std::string& error) = 0;
    virtual bool write(const std::string& path, const std::string& text, std::string& error) = 0;
    virtual void sleepMs(int ms) = 0;
};

struct Options {
    std::string version;            // "1.2.1 abc1234"
    std::string documentsDir;       // where "LMW-Synth Presets" lives (Spec S7.5, S7.7)
    std::string guide;              // docs/ai/SoundDesign.md
    std::string tutorial;           // docs/tutorial/Tutorial.md
};

class Server {
public:
    static constexpr const char* protocolVersion = "2025-06-18";

    Server(Link& l, Options o) : link(l), opt(std::move(o)) {}

    // One JSON-RPC message in; the answer (or nothing for a notification) out.
    std::optional<std::string> handle(std::string_view line) {
        std::string perr;
        const auto msg = json::parse(line, &perr);
        if (!msg || !msg->isObject()) return error(json::Value::null(), -32700, "parse error: " + perr);
        const auto* id = msg->get("id");
        const auto* method = msg->get("method");
        if (method == nullptr || !method->isString()) {
            if (id == nullptr) return std::nullopt;                 // a response to us: ignore
            return error(*id, -32600, "invalid request");
        }
        static const json::Value noParams = json::Value::object();
        const auto* params = msg->get("params");
        if (params == nullptr) params = &noParams;
        if (id == nullptr) return std::nullopt;                      // notifications (initialized, cancelled)
        const auto& m = method->s;
        if (m == "initialize") return result(*id, initialize(*params));
        if (m == "ping") return result(*id, json::Value::object());
        if (m == "tools/list") return result(*id, toolsList());
        if (m == "tools/call") return result(*id, callTool(*params));
        if (m == "resources/list") return result(*id, resourcesList());
        if (m == "resources/read") {
            const auto* uri = params->get("uri");
            if (uri == nullptr || !uri->isString()) return error(*id, -32602, "uri missing");
            if (auto r = readResource(uri->s)) return result(*id, *r);
            return error(*id, -32002, "unknown resource: " + uri->s);
        }
        return error(*id, -32601, "method not found: " + m);
    }

private:
    using V = json::Value;

    static std::string error(const V& id, int code, const std::string& message) {
        return V::object().set("jsonrpc", V::string("2.0")).set("id", id)
            .set("error", V::object().set("code", V::number(code)).set("message", V::string(message))).dump();
    }
    static std::string result(const V& id, V r) {
        return V::object().set("jsonrpc", V::string("2.0")).set("id", id).set("result", std::move(r)).dump();
    }

    V initialize(const V& params) {
        std::string version = protocolVersion;
        if (const auto* v = params.get("protocolVersion"); v != nullptr && v->isString())
            for (const char* known : {"2025-06-18", "2025-03-26", "2024-11-05"})
                if (v->s == known) version = known;
        return V::object()
            .set("protocolVersion", V::string(version))
            .set("capabilities", V::object().set("tools", V::object()).set("resources", V::object()))
            .set("serverInfo", V::object().set("name", V::string("lmw-mcp")).set("title", V::string("LMW-Synth"))
                                   .set("version", V::string(opt.version)))
            .set("instructions", V::string(
                "LMW-Synth is a semi-modular software synthesizer. Read the resource lmw://guide/sound-design "
                "first: it explains the modules, the parameter names and recipes. To make a sound: start "
                "from the nearest preset (list_presets, apply_instrument_preset), change a few parameters "
                "(set_parameters, set_cables), then play a note or chord and adjust. describe_synth gives "
                "every parameter's range. When the user likes the sound, name_sound and offer "
                "save_instrument_preset."));
    }

    // ---- tools -----------------------------------------------------------------------------
    struct Tool { const char* name; const char* description; std::string schema; };
    static const std::vector<Tool>& tools() {
        static const std::vector<Tool> t{
            {"list_synths", "List the running LMW-Synth instances (address, version, patch name) and which one is in use.",
             R"({"type":"object","properties":{}})"},
            {"select_synth", "Use the LMW-Synth instance at this address (from list_synths).",
             R"({"type":"object","properties":{"address":{"type":"string"}},"required":["address"]})"},
            {"describe_synth", "Every parameter with its id, range, unit, default and choices (one JSON object per line), "
                               "and every socket with its normal (pre-wired) source.",
             R"({"type":"object","properties":{}})"},
            {"get_sound", "The current sound as patch text: all parameter values, the cables, the patch name and notes.",
             R"({"type":"object","properties":{}})"},
            {"set_parameters", "Set several parameters at once. values maps a parameter id (e.g. vcf1.cutoff) to its "
                               "value: a number in the parameter's unit (Hz, s, octaves, semitones, 0-1 for %) or a "
                               "choice name (e.g. Saw). Answers what was set and what was refused, with the reason.",
             R"({"type":"object","properties":{"values":{"type":"object","additionalProperties":{"type":["string","number"]}}},"required":["values"]})"},
            {"set_cables", "Patch cables. Each entry is input=output[@level][+output[@level]...], e.g. "
                           "vcf1.cutoff=lfo2.out@0.3; input= removes that input's cables. A cable replaces the "
                           "input's normal connection. clear_first removes all cables before.",
             R"({"type":"object","properties":{"cables":{"type":"array","items":{"type":"string"}},"clear_first":{"type":"boolean"}}})"},
            {"list_presets", "The built-in instrument presets and the module presets per module kind.",
             R"({"type":"object","properties":{}})"},
            {"apply_instrument_preset", "Set the whole sound to a built-in instrument preset (all parameters, cables "
                                        "and the patch name; the notes stay).",
             R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})"},
            {"apply_module_preset", "Set one module to a preset, e.g. module vcf1 name Oboe, module formant name "
                                    "Bass O, module delay name Ping-pong. Module: env1, env2, vcf1, vcf2, vco1-vco3, "
                                    "formant, comb, delay, reverb. Name Default sets the module's defaults.",
             R"({"type":"object","properties":{"module":{"type":"string"},"name":{"type":"string"}},"required":["module","name"]})"},
            {"play", "Play MIDI notes (0-127, 60 = middle C) together for some seconds, so the sound can be heard.",
             R"({"type":"object","properties":{"notes":{"type":"array","items":{"type":"integer"}},"velocity":{"type":"integer"},"seconds":{"type":"number"}},"required":["notes"]})"},
            {"name_sound", "Set the patch name, and the notes when given.",
             R"({"type":"object","properties":{"name":{"type":"string"},"notes":{"type":"string"}},"required":["name"]})"},
            {"save_instrument_preset", "Save the current sound as the user's own instrument preset (Presets menu, "
                                       "Your presets). Refuses an existing name unless overwrite is true.",
             R"({"type":"object","properties":{"name":{"type":"string"},"overwrite":{"type":"boolean"}},"required":["name"]})"},
        };
        return t;
    }
    V toolsList() const {
        V list = V::array();
        for (const auto& t : tools())
            list.push(V::object().set("name", V::string(t.name)).set("description", V::string(t.description))
                          .set("inputSchema", *json::parse(t.schema)));
        return V::object().set("tools", std::move(list));
    }
    static V text(const std::string& t, bool isError = false) {
        return V::object().set("content", V::array().push(V::object().set("type", V::string("text")).set("text", V::string(t))))
                   .set("isError", V::boolean(isError));
    }

    // Connected to a synth (the selected one, else the newest), or the reason why not.
    bool ensure(std::string& why) {
        if (link.connected()) return true;
        if (!chosen.empty() && link.connect(chosen, why)) return true;
        const auto all = link.running();
        if (all.empty()) { why = "no LMW-Synth is running: start LMW-Synth (or load the plugin in a DAW) first"; return false; }
        for (const auto& a : all) if (link.connect(a, why)) return true;
        return false;
    }
    // A read or write that connects first and tries once more after a lost connection.
    std::optional<std::string> rd(const std::string& path, std::string& why) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (!ensure(why)) return std::nullopt;
            if (auto r = link.read(path, why)) return r;
            if (link.connected()) return std::nullopt;
        }
        return std::nullopt;
    }
    bool wr(const std::string& path, const std::string& t, std::string& why) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (!ensure(why)) return false;
            if (link.write(path, t, why)) return true;
            if (link.connected()) return false;
        }
        return false;
    }

    static std::string lower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
    static std::string paramPath(const std::string& id) {      // vcf1.cutoff -> params/vcf1/cutoff
        auto p = id;
        if (const auto dot = p.find('.'); dot != std::string::npos) p[dot] = '/';
        return "params/" + p;
    }
    static std::string trimmed(std::string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        return s;
    }
    static std::string valueText(const V& v) {
        if (v.isString()) return v.s;
        if (v.isNumber()) { char b[40]; std::snprintf(b, sizeof b, "%.9g", v.n); return b; }
        if (v.isBool()) return v.b ? "1" : "0";
        return {};
    }
    // The patch text with its name (and notes, when given) replaced.
    static std::string withName(const std::string& patch, const std::string& name, const std::optional<std::string>& notes) {
        std::string out;
        std::istringstream in(patch);
        bool header = true, named = false;
        for (std::string l; std::getline(in, l);) {
            if (!l.empty() && l[0] == '[') {
                if (header && !named) out += "name = " + name + "\n";
                header = false;
                if (notes && l == "[notes]") break;
            }
            if (header && l.rfind("name =", 0) == 0) { out += "name = " + name + "\n"; named = true; continue; }
            out += l + "\n";
        }
        if (header && !named) out += "name = " + name + "\n";
        if (notes) out += "[notes]\n" + *notes + (notes->empty() || notes->back() == '\n' ? "" : "\n");
        return out;
    }
    static std::string oneLine(std::string s) {
        for (auto& c : s) if (c == '\n' || c == '\r') c = ' ';
        return s;
    }

    V callTool(const V& params) {
        const auto* nameV = params.get("name");
        if (nameV == nullptr || !nameV->isString()) return text("tool name missing", true);
        static const V noArgs = V::object();
        const auto* args = params.get("arguments");
        if (args == nullptr || !args->isObject()) args = &noArgs;
        const auto& name = nameV->s;
        const auto str = [&](const char* key) -> std::string {
            const auto* v = args->get(key);
            return v != nullptr ? valueText(*v) : std::string();
        };
        std::string why;

        if (name == "list_synths") {
            const auto all = link.running();
            if (all.empty()) return text("No LMW-Synth is running. Start LMW-Synth (or load the plugin in a DAW).");
            const std::string inUse = link.connected() ? link.address() : std::string();
            std::string out;
            for (const auto& a : all) {
                std::string e, version = "?", patchName;
                if (link.connect(a, e)) {
                    if (auto v = link.read("version", e)) version = trimmed(*v);
                    if (auto p = link.read("patch", e)) {
                        std::istringstream in(*p);
                        for (std::string l; std::getline(in, l) && (l.empty() || l[0] != '[');)
                            if (l.rfind("name =", 0) == 0) patchName = trimmed(l.substr(6));
                    }
                }
                while (!patchName.empty() && patchName.front() == ' ') patchName.erase(0, 1);
                out += a + "  version " + version + "  patch \"" + patchName + "\"" + (a == inUse ? "  (in use)" : "") + "\n";
            }
            std::string e;
            if (!inUse.empty()) link.connect(inUse, e);
            else if (!chosen.empty()) link.connect(chosen, e);
            return text(out);
        }
        if (name == "select_synth") {
            const auto a = str("address");
            if (!link.connect(a, why)) return text(why, true);
            chosen = a;
            return text("Using " + a + ".");
        }
        if (name == "describe_synth") {
            const auto info = rd("info", why);
            if (!info) return text(why, true);
            const auto ports = rd("ports", why);
            if (!ports) return text(why, true);
            return text("Parameters (id, kind, range, unit, default, choices):\n" + *info
                        + "\nSockets (input lines: normal source -> input; output lines):\n" + *ports);
        }
        if (name == "get_sound") {
            auto p = rd("patch", why);
            return p ? text(*p) : text(why, true);
        }
        if (name == "set_parameters") {
            const auto* values = args->get("values");
            if (values == nullptr || !values->isObject() || values->o.empty()) return text("values missing (an object id -> value)", true);
            std::string done, refused;
            int ok = 0;
            for (const auto& [id, v] : values->o) {
                std::string e;
                const auto t = valueText(v);
                if (t.empty() || id.find('.') == std::string::npos) { refused += "  " + id + ": not a parameter id or no value\n"; continue; }
                if (!wr(paramPath(id), t + "\n", e)) { refused += "  " + id + " = " + t + ": " + e + "\n"; continue; }
                ++ok;
                const auto back = rd(paramPath(id), e);
                const auto now = back ? trimmed(*back) : t;
                done += "  " + id + " = " + now + (now != t ? "  (asked " + t + ")" : "") + "\n";
            }
            std::string out = ok ? "Set:\n" + done : std::string("Nothing was set.\n");
            if (!refused.empty()) out += "Refused:\n" + refused;
            return text(out, ok == 0);
        }
        if (name == "set_cables") {
            const auto* cables = args->get("cables");
            const auto* clear = args->get("clear_first");
            std::string refused;
            int tried = 0, done = 0;
            if (clear != nullptr && clear->isBool() && clear->b && !wr("cables", "clear\n", why)) return text(why, true);
            if (cables != nullptr && cables->isArray())
                for (const auto& c : cables->a) {
                    std::string e;
                    ++tried;
                    if (!c.isString() || c.s.find('=') == std::string::npos) { refused += "  " + c.dump() + ": not input=pins\n"; continue; }
                    if (wr("cables", oneLine(c.s) + "\n", e)) ++done;
                    else refused += "  " + c.s + ": " + e + "\n";
                }
            const auto now = rd("cables", why);
            std::string out = "Cables now:\n" + (now ? (now->empty() ? std::string("  (none: everything normalled)\n") : *now) : why + "\n");
            if (!refused.empty()) out += "Refused:\n" + refused;
            return text(out, tried > 0 && done == 0);
        }
        if (name == "list_presets") {
            std::string out = "Instrument presets (apply_instrument_preset):\n";
            for (const auto& i : instruments::builtIn()) out += "  " + i.name + "\n";
            using presets::Kind;
            const std::pair<Kind, const char*> kinds[] = {
                {Kind::Env, "env1, env2"}, {Kind::Vcf, "vcf1, vcf2"}, {Kind::Vco, "vco1, vco2, vco3"},
                {Kind::Formant, "formant"}, {Kind::Comb, "comb"}, {Kind::Delay, "delay"}, {Kind::Reverb, "reverb"}};
            out += "Module presets (apply_module_preset; every module also has Default):\n";
            for (const auto& [k, modules] : kinds) {
                out += "  " + std::string(modules) + ":";
                bool first = true;
                for (const auto& p : presets::builtIn(k)) { out += (first ? " " : ", ") + p.name; first = false; }
                out += "\n";
            }
            return text(out);
        }
        if (name == "apply_instrument_preset") {
            const auto want = lower(str("name"));
            const auto& all = instruments::builtIn();
            const auto it = std::find_if(all.begin(), all.end(), [&](const auto& i) { return lower(i.name) == want; });
            if (it == all.end()) return text("no instrument preset \"" + str("name") + "\" (see list_presets)", true);
            const auto current = rd("patch", why);
            if (!current) return text(why, true);
            // The current header (format, state version) with the new name, the preset's values (missing ones
            // go to their default when the patch is read, S7.1), its cables, and the current schematic and notes.
            std::string head, keep;
            {
                std::istringstream in(*current);
                bool header = true, keeping = false;
                for (std::string l; std::getline(in, l);) {
                    if (!l.empty() && l[0] == '[') { header = false; keeping = l == "[schematic]" || l == "[notes]"; }
                    if (header) { if (l.rfind("name =", 0) != 0) head += l + "\n"; }
                    else if (keeping) keep += l + "\n";
                }
            }
            while (head.size() >= 2 && head.substr(head.size() - 2) == "\n\n") head.pop_back();   // the name right after
            std::string patch = head + "name = " + it->name + "\n\n[parameters]\n";
            for (const auto& [id, value] : it->values) patch += id + " = " + value + "\n";
            patch += "\n[cables]\n";
            std::istringstream cin(it->cables);
            for (std::string c; std::getline(cin, c, ';');)
                if (const auto eq = c.find('='); eq != std::string::npos) patch += c.substr(0, eq) + " = " + c.substr(eq + 1) + "\n";
            patch += "\n" + keep;
            if (!wr("patch", patch, why)) return text(why, true);
            return text("Instrument preset \"" + it->name + "\" set (" + std::to_string(it->values.size())
                        + " values, other parameters at their defaults" + (it->cables.empty() ? "" : "; cables " + it->cables) + ").");
        }
        if (name == "apply_module_preset") {
            const auto module = lower(str("module"));
            const auto kind = presets::kindOf(module);
            if (!kind) return text("no module \"" + str("module") + "\" with presets (env1, env2, vcf1, vcf2, vco1-vco3, formant, comb, delay, reverb)", true);
            const auto want = lower(str("name"));
            std::vector<std::pair<std::string, std::string>> values;
            std::string shown;
            if (want == "default") {
                const auto info = rd("info", why);
                if (!info) return text(why, true);
                const auto all = json::parse(*info);
                if (!all || !all->isArray()) return text("cannot read the parameter list (/info)", true);
                for (const auto& o : all->a) {
                    const auto* id = o.get("id");
                    const auto* def = o.get("default");
                    if (id == nullptr || def == nullptr || !id->isString() || id->s.rfind(module + ".", 0) != 0) continue;
                    const auto key = id->s.substr(module.size() + 1);
                    const auto& ks = presets::keys(*kind);
                    if (std::find(ks.begin(), ks.end(), key) != ks.end()) values.emplace_back(key, valueText(*def));
                }
                shown = "Default";
            } else {
                const auto& list = presets::builtIn(*kind);
                const auto it = std::find_if(list.begin(), list.end(), [&](const auto& p) { return lower(p.name) == want; });
                if (it == list.end()) return text("no " + module + " preset \"" + str("name") + "\" (see list_presets)", true);
                values = it->values;
                shown = it->name;
            }
            std::string out, refused;
            for (const auto& [key, value] : values) {
                std::string e;
                if (wr(paramPath(module + "." + key), value + "\n", e)) out += "  " + module + "." + key + " = " + value + "\n";
                else refused += "  " + module + "." + key + ": " + e + "\n";
            }
            return text(module + " set to \"" + shown + "\":\n" + out + (refused.empty() ? "" : "Refused:\n" + refused), out.empty());
        }
        if (name == "play") {
            const auto* notes = args->get("notes");
            if (notes == nullptr || !notes->isArray() || notes->a.empty()) return text("notes missing (MIDI numbers, 60 = middle C)", true);
            int velocity = 100;
            if (const auto* v = args->get("velocity"); v != nullptr && v->isNumber()) velocity = std::clamp(static_cast<int>(v->n), 1, 127);
            double seconds = 1.0;
            if (const auto* s = args->get("seconds"); s != nullptr && s->isNumber()) seconds = std::clamp(s->n, 0.1, 10.0);
            std::string on, off, shown;
            for (const auto& n : notes->a) {
                if (!n.isNumber() || n.n < 0 || n.n > 127) return text("a note is not a number 0-127: " + n.dump(), true);
                const auto k = std::to_string(static_cast<int>(n.n));
                on += "on " + k + " " + std::to_string(velocity) + "\n";
                off += "off " + k + "\n";
                shown += (shown.empty() ? "" : ", ") + k;
            }
            if (!wr("keys", on, why)) return text(why, true);
            link.sleepMs(static_cast<int>(seconds * 1000.0));
            if (!wr("keys", off, why)) return text(why, true);
            char b[64];
            std::snprintf(b, sizeof b, "%.1f s", seconds);
            return text("Played " + shown + " (velocity " + std::to_string(velocity) + ") for " + b + ".");
        }
        if (name == "name_sound") {
            const auto newName = oneLine(str("name"));
            if (newName.empty()) return text("name missing", true);
            const auto current = rd("patch", why);
            if (!current) return text(why, true);
            std::optional<std::string> notes;
            if (const auto* n = args->get("notes"); n != nullptr && n->isString()) notes = n->s;
            if (!wr("patch", withName(*current, newName, notes), why)) return text(why, true);
            return text("The sound is now called \"" + newName + "\"" + (notes ? " (notes set)." : "."));
        }
        if (name == "save_instrument_preset") {
            const auto presetName = oneLine(str("name"));
            if (presetName.empty()) return text("name missing", true);
            if (opt.documentsDir.empty()) return text("no Documents folder found", true);
            namespace fs = std::filesystem;
            const fs::path dir = fs::path(opt.documentsDir) / "LMW-Synth Presets" / std::string(instruments::folderName);
            const fs::path file = dir / (presets::fileName(presetName) + ".lmwpatch");
            const auto* ow = args->get("overwrite");
            std::error_code ec;
            if (fs::exists(file, ec) && !(ow != nullptr && ow->isBool() && ow->b))
                return text("an own preset \"" + presetName + "\" exists already (" + file.string() + "); ask the user, then use overwrite true", true);
            const auto current = rd("patch", why);
            if (!current) return text(why, true);
            fs::create_directories(dir, ec);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << withName(*current, presetName, std::nullopt);
            out.close();
            if (!out) return text("cannot write " + file.string(), true);
            return text("Saved as \"" + presetName + "\" (" + file.string() + "); it is in the Presets menu under Your presets.");
        }
        return text("unknown tool: " + name, true);
    }

    // ---- resources ---------------------------------------------------------------------------
    V resourcesList() const {
        auto res = [](const char* uri, const char* name, const char* description) {
            return V::object().set("uri", V::string(uri)).set("name", V::string(name))
                .set("description", V::string(description)).set("mimeType", V::string("text/markdown"));
        };
        return V::object().set("resources", V::array()
            .push(res("lmw://guide/sound-design", "Sound design guide", "How LMW-Synth makes sound: modules, parameter names, recipes. Read first."))
            .push(res("lmw://guide/tutorial", "Tutorial", "The eleven lessons for new users.")));
    }
    std::optional<V> readResource(const std::string& uri) const {
        const std::string* t = uri == "lmw://guide/sound-design" ? &opt.guide : uri == "lmw://guide/tutorial" ? &opt.tutorial : nullptr;
        if (t == nullptr) return std::nullopt;
        return V::object().set("contents", V::array().push(V::object().set("uri", V::string(uri))
            .set("mimeType", V::string("text/markdown")).set("text", V::string(*t))));
    }

    Link& link;
    Options opt;
    std::string chosen;                  // select_synth
};

} // namespace lmw::mcp
