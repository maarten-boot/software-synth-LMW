// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S11-mcp: lmw-mcp's JSON reader/writer and its MCP server against a fake synth (Spec S11): the protocol
// (initialize, tools/list, errors, notifications, resources), every tool's 9P writes, connecting to the
// newest synth, connecting again after a lost connection, and saving an own preset.
#include "mcp/Json.h"
#include "mcp/Server.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace lmw;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
struct Fake final : mcp::Link {
    std::vector<std::string> sockets{"/run/a.sock", "/run/b.sock"};
    std::string addr;
    bool live = false;
    bool dropNext = false;                         // the next read loses the connection
    std::map<std::string, std::string> files;
    std::vector<std::pair<std::string, std::string>> writes;
    int slept = 0;
    int undoable = 2;
    Fake() {
        files["version"] = "1.2.1 abc1234\n";
        files["patch"] = "# LMW-Synth patch\nformat = 1\nstate_version = 4\nname = Old\n\n[parameters]\nvcf1.cutoff = 18000\n\n[cables]\n\n[notes]\nmy notes\n";
        files["info"] = "[\n{\"id\": \"vcf1.cutoff\", \"kind\": \"float\", \"default\": 18000.0},\n"
                        "{\"id\": \"vcf1.res\", \"kind\": \"float\", \"default\": 0.0},\n"
                        "{\"id\": \"vcf1.slope\", \"kind\": \"choice\", \"default\": \"24 dB\"},\n"
                        "{\"id\": \"vco1.wave\", \"kind\": \"choice\", \"default\": \"Saw\"}\n]\n";
        files["ports"] = "input  mix.out -> vcf1.in\n";
        files["cables"] = "";
        files["params/vcf1/cutoff"] = "18000\n";
        files["params/vco1/wave"] = "Saw\n";
    }
    std::vector<std::string> running() override { return sockets; }
    bool connect(const std::string& a, std::string& e) override {
        if (std::find(sockets.begin(), sockets.end(), a) == sockets.end()) { e = "cannot connect to " + a; return false; }
        addr = a; live = true; return true;
    }
    bool connected() const override { return live; }
    std::string address() const override { return addr; }
    std::optional<std::string> read(const std::string& p, std::string& e) override {
        if (!live) { e = "not connected"; return std::nullopt; }
        if (dropNext) { dropNext = false; live = false; e = "no answer"; return std::nullopt; }
        const auto it = files.find(p);
        if (it == files.end()) { e = p + ": no such file"; return std::nullopt; }
        return it->second;
    }
    bool write(const std::string& p, const std::string& t, std::string& e) override {
        if (!live) { e = "not connected"; return false; }
        if (p.rfind("params/", 0) == 0 && files.find(p) == files.end()
            && p.find("delay") == std::string::npos && p.find("vcf1") == std::string::npos) { e = p + ": no such file"; return false; }
        if (p == "undo") {                                     // Spec S6.14: two steps to take back
            if (undoable == 0) { e = "nothing to undo"; return false; }
            files["history"] = --undoable == 1 ? "> VCF 1 Cutoff\n  cables\n" : "  VCF 1 Cutoff\n  cables\n";
        }
        writes.emplace_back(p, t);
        if (p.rfind("params/", 0) == 0) files[p] = p == "params/vcf1/cutoff" && t == "99999\n" ? "18000\n" : t;
        if (p == "patch") files["patch"] = t;
        if (p == "cables") files["cables"] = t == "clear\n" ? "" : files["cables"] + t;
        return true;
    }
    void sleepMs(int ms) override { slept += ms; }
};

std::string call(mcp::Server& s, const std::string& tool, const std::string& args, bool* isError = nullptr) {
    const auto reply = s.handle(R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":")" + tool
                                + R"(","arguments":)" + args + "}}");
    if (!reply) return "(no reply)";
    const auto v = json::parse(*reply);
    if (!v) return "(bad json)";
    const auto* r = v->get("result");
    if (r == nullptr) return "(no result)";
    if (isError != nullptr) *isError = r->get("isError") != nullptr && r->get("isError")->b;
    return r->get("content")->a.at(0).get("text")->s;
}
bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);              // FAIL lines shown even if the test aborts
    // JSON: round trip, escapes and surrogate pairs, errors.
    {
        const auto v = json::parse(R"( {"a": [1, 2.5, -3e2, true, false, null], "s": "x\"\n\u00e9\ud83c\udfb9", "o": {}} )");
        CHECK(v && v->isObject() && v->get("a")->a.size() == 6 && v->get("a")->a[2].n == -300.0);
        CHECK(v->get("s")->s == "x\"\n\xc3\xa9\xf0\x9f\x8e\xb9");
        CHECK(v->dump() == R"({"a":[1,2.5,-300,true,false,null],"s":"x\"\n)" "\xc3\xa9\xf0\x9f\x8e\xb9" R"(","o":{}})");
        std::string e;
        CHECK(!json::parse("{\"a\":}", &e) && !e.empty());
        CHECK(!json::parse("[1,2] x") && !json::parse("\"\\ud83c\"") && !json::parse("tru") && !json::parse(""));
        CHECK(json::Value::string("\x01").dump() == "\"\\u0001\"");
    }
    const auto docs = std::filesystem::temp_directory_path() / ("lmw-mcp-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fake f;
    mcp::Server s(f, {"1.2.1", docs.string(), "# guide", "# tutorial"});

    // Protocol.
    {
        const auto r = json::parse(*s.handle(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"t","version":"1"}}})"));
        CHECK(r && r->get("id")->n == 1 && r->get("result")->get("protocolVersion")->s == "2025-03-26");
        CHECK(r->get("result")->get("serverInfo")->get("name")->s == "lmw-mcp" && r->get("result")->get("capabilities")->get("tools"));
        CHECK(has(r->get("result")->get("instructions")->s, "lmw://guide/sound-design"));
        const auto r2 = json::parse(*s.handle(R"({"jsonrpc":"2.0","id":"x","method":"initialize","params":{"protocolVersion":"1999-01-01"}})"));
        CHECK(r2 && r2->get("id")->s == "x" && r2->get("result")->get("protocolVersion")->s == mcp::Server::protocolVersion);
        CHECK(!s.handle(R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));
        const auto bad = json::parse(*s.handle("{nope"));
        CHECK(bad && bad->get("error")->get("code")->n == -32700 && bad->get("id")->isNull());
        CHECK(json::parse(*s.handle(R"({"jsonrpc":"2.0","id":2,"method":"x/y"})"))->get("error")->get("code")->n == -32601);
        CHECK(json::parse(*s.handle(R"({"jsonrpc":"2.0","id":3,"method":"ping"})"))->get("result")->isObject());
        const auto tl = json::parse(*s.handle(R"({"jsonrpc":"2.0","id":4,"method":"tools/list"})"));
        const auto& list = tl->get("result")->get("tools")->a;
        CHECK(list.size() == 13);
        for (const auto& t : list) CHECK(t.get("inputSchema") && t.get("inputSchema")->get("type")->s == "object" && !t.get("description")->s.empty());
        const auto rl = json::parse(*s.handle(R"({"jsonrpc":"2.0","id":5,"method":"resources/list"})"));
        CHECK(rl->get("result")->get("resources")->a.size() == 2);
        const auto rr = json::parse(*s.handle(R"({"jsonrpc":"2.0","id":6,"method":"resources/read","params":{"uri":"lmw://guide/sound-design"}})"));
        CHECK(rr->get("result")->get("contents")->a.at(0).get("text")->s == "# guide");
        CHECK(json::parse(*s.handle(R"({"jsonrpc":"2.0","id":6,"method":"resources/read","params":{"uri":"lmw://x"}})"))->get("error"));
    }
    // Tools: connects to the newest synth on first use.
    bool err = false;
    {
        const auto t = call(s, "get_sound", "{}", &err);
        CHECK(!err && has(t, "name = Old") && f.addr == "/run/a.sock");
        CHECK(has(call(s, "describe_synth", "{}"), "vcf1.cutoff") && has(call(s, "describe_synth", "{}"), "mix.out -> vcf1.in"));
        const auto ls = call(s, "list_synths", "{}");
        CHECK(has(ls, "/run/a.sock  version 1.2.1 abc1234  patch \"Old\"  (in use)") && has(ls, "/run/b.sock"));
        CHECK(has(call(s, "select_synth", R"({"address":"/run/b.sock"})", &err), "Using /run/b.sock") && !err && f.addr == "/run/b.sock");
        call(s, "select_synth", R"({"address":"/run/nope.sock"})", &err);
        CHECK(err);
    }
    // set_parameters: numbers and choice names; a refused id; a clipped value is reported.
    {
        f.writes.clear();
        const auto t = call(s, "set_parameters", R"({"values":{"vcf1.cutoff":800,"vco1.wave":"Square","bogus.x":1}})", &err);
        CHECK(!err && has(t, "vcf1.cutoff = 800") && has(t, "vco1.wave = Square") && has(t, "Refused:") && has(t, "bogus.x"));
        CHECK(f.files["params/vcf1/cutoff"] == "800\n");
        const auto clip = call(s, "set_parameters", R"({"values":{"vcf1.cutoff":99999}})");
        CHECK(has(clip, "vcf1.cutoff = 18000  (asked 99999)"));
        call(s, "set_parameters", R"({"values":{}})", &err);
        CHECK(err);
    }
    // set_cables.
    {
        const auto t = call(s, "set_cables", R"({"clear_first":true,"cables":["vcf1.cutoff=lfo2.out@0.3","nonsense"]})", &err);
        CHECK(!err && has(t, "vcf1.cutoff=lfo2.out@0.3") && has(t, "Refused:") && has(t, "nonsense"));
        CHECK(f.writes.size() >= 2 && f.writes[f.writes.size() - 2].second == "clear\n");
    }
    // play: on, wait, off.
    {
        f.writes.clear();
        const auto t = call(s, "play", R"({"notes":[60,64],"velocity":90,"seconds":0.5})", &err);
        CHECK(!err && has(t, "Played 60, 64") && f.slept == 500);
        CHECK(f.writes.size() == 2 && f.writes[0].second == "on 60 90\non 64 90\n" && f.writes[1].second == "off 60\noff 64\n");
        call(s, "play", R"({"notes":[200]})", &err);
        CHECK(err);
    }
    // Presets.
    {
        CHECK(has(call(s, "list_presets", "{}"), "Clarinet") && has(call(s, "list_presets", "{}"), "delay: Doubling"));
        const auto t = call(s, "apply_instrument_preset", R"j({"name":"electric piano (fm)"})j", &err);
        CHECK(!err && has(t, "Electric piano (FM)"));
        const auto& p = f.files["patch"];
        CHECK(has(p, "state_version = 4\nname = Electric piano (FM)\n") && has(p, "vco1.pm = 1.5") && has(p, "[cables]\nvco1.pm = vco2.out\n"));
        CHECK(has(p, "[notes]\nmy notes\n") && !has(p, "name = Old"));
        call(s, "apply_instrument_preset", R"({"name":"Kazoo"})", &err);
        CHECK(err);
        f.writes.clear();
        CHECK(has(call(s, "apply_module_preset", R"({"module":"delay","name":"ping-pong"})", &err), "Ping-pong") && !err);
        CHECK(f.files["params/delay/time"] == "0.375\n" && f.files["params/delay/spread"] == "1\n" && f.writes.size() == 5);
        f.writes.clear();
        call(s, "apply_module_preset", R"({"module":"vcf1","name":"Default"})", &err);
        CHECK(!err && f.files["params/vcf1/cutoff"] == "18000\n" && f.files["params/vcf1/slope"] == "24 dB\n" && f.writes.size() == 3);
        call(s, "apply_module_preset", R"({"module":"lfo1","name":"x"})", &err);
        CHECK(err);
    }
    // name_sound and save_instrument_preset.
    {
        call(s, "name_sound", R"({"name":"Warm\nbass","notes":"line 1\nline 2"})", &err);
        const auto& p = f.files["patch"];
        CHECK(!err && has(p, "name = Warm bass\n") && has(p, "[notes]\nline 1\nline 2\n") && !has(p, "my notes"));
        const auto t = call(s, "save_instrument_preset", R"({"name":"My bass/1"})", &err);
        const auto file = docs / "LMW-Synth Presets" / "INSTRUMENT" / "My bass_1.lmwpatch";
        CHECK(!err && std::filesystem::exists(file) && has(t, "Your presets"));
        std::string saved;
        {   // closed before the folder is removed (Windows cannot delete an open file)
            std::ifstream in(file);
            saved.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        CHECK(has(saved, "name = My bass/1\n") && has(saved, "[notes]\nline 1"));
        call(s, "save_instrument_preset", R"({"name":"My bass/1"})", &err);
        CHECK(err);
        call(s, "save_instrument_preset", R"({"name":"My bass/1","overwrite":true})", &err);
        CHECK(!err);
        std::error_code ec;
        std::filesystem::remove_all(docs, ec);                 // no exception: a leftover is harmless
    }
    // undo (S6.14): one step, then more than there are, then nothing left.
    {
        auto t = call(s, "undo", "{}", &err);
        CHECK(!err && has(t, "Took back 1 step.") && has(t, "> VCF 1 Cutoff"));
        t = call(s, "undo", R"({"steps":5})", &err);
        CHECK(!err && has(t, "Took back 1 step (no more to undo).") && has(t, "  VCF 1 Cutoff\n  cables"));
        t = call(s, "undo", "{}", &err);
        CHECK(err && has(t, "nothing to undo"));
        call(s, "undo", R"({"steps":0})", &err);
        CHECK(err);
    }
    // A lost connection: connected again on the next use; no synth at all: a clear error.
    {
        f.dropNext = true;
        const auto t = call(s, "get_sound", "{}", &err);
        CHECK(!err && has(t, "[parameters]"));
        f.live = false;
        f.sockets.clear();
        const auto none = call(s, "get_sound", "{}", &err);
        CHECK(err && has(none, "no LMW-Synth is running"));
        CHECK(has(call(s, "list_synths", "{}"), "No LMW-Synth is running"));
        call(s, "no_such_tool", "{}", &err);
        CHECK(err);
    }
    std::printf(failures ? "%d FAILED\n" : "T-S11-mcp: all passed\n", failures);
    return failures ? 1 : 0;
}
