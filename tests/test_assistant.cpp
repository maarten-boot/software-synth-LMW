// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S12-client: the Assistant tab's Claude client (Spec S12.2–S12.4, S12.7, S12.9) against a fake service
// and a fake synth: the request (headers, model, version, cached guide, tools, effort, fallbacks), the
// tool loop (tool_use -> results in one message, errors, end_turn, max_tokens, refusal, pause_turn, step
// limit, Stop), retries, error texts, the cost and the session total, Undo, and that the key shows up only
// in the x-api-key header.
#include "assistant/Claude.h"
#include "assistant/ToolLine.h"
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <vector>

using namespace lmw;
using assistant::V;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
const std::string key = "sk-ant-test-SECRET-123";

struct Synth final : mcp::Link {
    std::map<std::string, std::string> files{
        {"patch", "# LMW-Synth patch\nname = Start\n\n[parameters]\nvcf1.cutoff = 18000\n"},
        {"params/vcf1/cutoff", "18000\n"}, {"cables", ""}, {"info", "[]\n"}, {"ports", ""}};
    std::vector<std::pair<std::string, std::string>> writes;
    std::vector<std::string> running() override { return {"self"}; }
    bool connect(const std::string&, std::string&) override { return true; }
    bool connected() const override { return true; }
    std::string address() const override { return "self"; }
    std::optional<std::string> read(const std::string& p, std::string& e) override {
        const auto it = files.find(p);
        if (it == files.end()) { e = p + ": no such file"; return std::nullopt; }
        return it->second;
    }
    bool write(const std::string& p, const std::string& t, std::string& e) override {
        if (p.rfind("params/", 0) == 0 && !files.count(p)) { e = p + ": no such file"; return false; }
        writes.emplace_back(p, t);
        files[p] = t;
        if (p == "params/vcf1/cutoff") files["patch"] = "# LMW-Synth patch\nname = Start\n\n[parameters]\nvcf1.cutoff = " + t;
        return true;
    }
    void sleepMs(int) override {}
};

struct Service final : assistant::Http {
    std::deque<assistant::HttpResponse> replies;
    std::vector<std::string> bodies, urls;
    std::vector<assistant::Headers> headers;
    int slept = 0;
    assistant::HttpResponse post(const std::string& url, const assistant::Headers& h, const std::string& body) override {
        urls.push_back(url); headers.push_back(h); bodies.push_back(body);
        if (replies.empty()) return {0, "", "", "test: no reply queued"};
        auto r = replies.front(); replies.pop_front(); return r;
    }
    assistant::HttpResponse get(const std::string& url, const assistant::Headers& h) override {
        urls.push_back(url); headers.push_back(h);
        if (replies.empty()) return {0, "", "", "test: no reply queued"};
        auto r = replies.front(); replies.pop_front(); return r;
    }
    void sleepMs(int ms) override { slept += ms; }
};

assistant::HttpResponse ok(const std::string& stop, const std::string& content, int in = 1000, int out = 100,
                           int cacheWrite = 0, int cacheRead = 0, const std::string& model = "claude-opus-5-5") {
    return {200, R"({"id":"msg_1","type":"message","role":"assistant","model":")" + model + R"(","stop_reason":")" + stop
                 + R"(","content":)" + content + R"(,"usage":{"input_tokens":)" + std::to_string(in)
                 + R"(,"output_tokens":)" + std::to_string(out) + R"(,"cache_creation_input_tokens":)"
                 + std::to_string(cacheWrite) + R"(,"cache_read_input_tokens":)" + std::to_string(cacheRead) + "}}", "", ""};
}
std::string headerOf(const assistant::Headers& h, const std::string& name) {
    for (const auto& [k, v] : h) if (k == name) return v;
    return {};
}
bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
bool near(double a, double b) { return a - b < 1e-9 && b - a < 1e-9; }
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::atomic<bool> stop{false};

    // The request (S12.3) and a two-step tool loop (S12.4) with the cost (S12.7).
    {
        Synth synth; Service svc;
        mcp::Server server(synth, {"1.2.1 test", "/nonexistent", "GUIDE-TEXT", "TUTORIAL-TEXT"});
        assistant::Assistant a(server, synth, svc, "GUIDE-TEXT", "TUTORIAL-TEXT");
        CHECK(a.settings.model == "claude-haiku-5-5");          // the cheapest is the default (S12.3)
        a.settings.key = key;
        a.settings.model = "claude-opus-5-5";
        svc.replies.push_back(ok("tool_use", R"([{"type":"text","text":"Making it darker."},)"
                                             R"({"type":"tool_use","id":"tu_1","name":"set_parameters","input":{"values":{"vcf1.cutoff":800}}},)"
                                             R"({"type":"tool_use","id":"tu_2","name":"read_tutorial","input":{}}])",
                                 1000, 200, 5000, 0));
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"Done: cutoff 800 Hz."}])", 300, 50, 0, 5000));
        std::vector<std::string> texts, tools;
        assistant::Events ev;
        ev.text = [&](const std::string& t) { texts.push_back(t); };
        ev.tool = [&](const std::string& n, const V&, const std::string& r, bool err) { tools.push_back(n + (err ? "!" : "") + ":" + r); };
        const auto o = a.ask("make it darker", stop, ev);
        CHECK(o.end == assistant::End::Done);
        CHECK(svc.bodies.size() == 2 && svc.urls[0] == "https://api.anthropic.com/v1/messages");
        const auto& h = svc.headers[0];
        CHECK(headerOf(h, "x-api-key") == key && headerOf(h, "anthropic-version") == "2026-10-21");
        CHECK(headerOf(h, "anthropic-beta") == "server-side-fallback-2026-07-01" && headerOf(h, "content-type") == "application/json");
        const auto body = json::parse(svc.bodies[0]);
        CHECK(body && body->get("model")->s == "claude-opus-5-5" && body->get("max_tokens")->n == 16000);
        CHECK(body->get("output_config")->get("effort")->s == "medium" && body->get("fallbacks")->s == "default");
        CHECK(body->get("thinking") == nullptr);
        const auto& sys = body->get("system")->a.at(0);
        CHECK(has(sys.get("text")->s, "GUIDE-TEXT") && sys.get("cache_control")->get("type")->s == "ephemeral");
        std::vector<std::string> names;
        for (const auto& t : body->get("tools")->a) {
            names.push_back(t.get("name")->s);
            CHECK(t.get("input_schema") != nullptr && t.get("inputSchema") == nullptr);
        }
        CHECK(std::find(names.begin(), names.end(), "list_synths") == names.end());
        CHECK(std::find(names.begin(), names.end(), "select_synth") == names.end());
        CHECK(std::find(names.begin(), names.end(), "set_parameters") != names.end());
        CHECK(std::find(names.begin(), names.end(), "read_tutorial") != names.end() && names.size() == 11);
        CHECK(body->get("messages")->a.size() == 1 && body->get("messages")->a[0].get("content")->a[0].get("text")->s == "make it darker");
        // second request: question, assistant turn as sent, both tool results in ONE user message
        const auto b2 = json::parse(svc.bodies[1]);
        const auto& msgs = b2->get("messages")->a;
        CHECK(msgs.size() == 3 && msgs[1].get("role")->s == "assistant" && msgs[1].get("content")->a.size() == 3);
        CHECK(msgs[2].get("role")->s == "user" && msgs[2].get("content")->a.size() == 2);
        CHECK(msgs[2].get("content")->a[0].get("tool_use_id")->s == "tu_1" && msgs[2].get("content")->a[1].get("tool_use_id")->s == "tu_2");
        CHECK(has(msgs[2].get("content")->a[0].get("content")->s, "vcf1.cutoff = 800"));
        CHECK(msgs[2].get("content")->a[1].get("content")->s == "TUTORIAL-TEXT" && !msgs[2].get("content")->a[1].get("is_error")->b);
        CHECK(synth.files["params/vcf1/cutoff"] == "800\n");
        CHECK(texts.size() == 2 && texts[0] == "Making it darker." && texts[1] == "Done: cutoff 800 Hz.");
        CHECK(tools.size() == 2 && tools[0].rfind("set_parameters:", 0) == 0 && tools[1] == "read_tutorial:TUTORIAL-TEXT");
        // cost: (1000*4 + 200*20 + 5000*5)/1e6 + (300*4 + 50*20 + 5000*0.2)/1e6 = 0.033 + 0.0032
        CHECK(near(o.cost, 0.0362) && near(a.sessionCost(), 0.0362) && !a.overLimit());
        // the key only in the header
        for (const auto& b : svc.bodies) CHECK(!has(b, key));
        for (const auto& t : texts) CHECK(!has(t, key));
        // Undo (S12.5): back to the sound before the answer
        CHECK(a.canUndo());
        std::string e;
        CHECK(a.undo(e) && has(synth.files["patch"], "vcf1.cutoff = 18000") && !a.canUndo());
        CHECK(!a.undo(e) && e == "nothing to undo");

        // a second question continues the conversation; model and effort from the settings
        a.settings.model = "claude-haiku-5-5";
        a.settings.effort = "low";
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"It is a filter."}])", 100, 10, 0, 0, "claude-haiku-5-5"));
        const auto o2 = a.ask("what is a VCF?", stop);
        const auto b3 = json::parse(svc.bodies.back());
        CHECK(o2.end == assistant::End::Done && b3->get("model")->s == "claude-haiku-5-5" && b3->get("output_config")->get("effort")->s == "low");
        CHECK(b3->get("messages")->a.size() == 5 && b3->get("messages")->a[4].get("role")->s == "user");
        CHECK(near(o2.cost, (100 * 0.10 + 10 * 0.50) / 1e6));
        a.newConversation();
        CHECK(json::parse(a.requestBody().dump())->get("messages")->a.empty() && !a.canUndo());
    }

    // Tool errors, the save confirmation, refusal, max_tokens, pause_turn, Stop, step limit (S12.4, S12.5).
    {
        Synth synth; Service svc;
        mcp::Server server(synth, {"1.2.1 test", "/nonexistent", "G", "T"});
        assistant::Assistant a(server, synth, svc, "G", "T");
        a.settings.key = key;
        // a failing tool and a cancelled save: both answered, the loop goes on
        svc.replies.push_back(ok("tool_use", R"([{"type":"tool_use","id":"t1","name":"set_parameters","input":{"values":{"bad.x":1}}},)"
                                             R"({"type":"tool_use","id":"t2","name":"save_instrument_preset","input":{"name":"Mine"}}])"));
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"ok"}])"));
        std::string asked;
        assistant::Events ev;
        ev.confirmSave = [&](const std::string& n, bool) { asked = n; return false; };
        CHECK(a.ask("save it", stop, ev).end == assistant::End::Done);
        const auto m = json::parse(svc.bodies[1])->get("messages")->a;
        CHECK(m[2].get("content")->a[0].get("is_error")->b);
        CHECK(m[2].get("content")->a[1].get("content")->s == "Not saved: the musician chose Cancel." && asked == "Mine");

        // refusal: not kept; the next question joins the same user message
        svc.replies.push_back({200, R"({"model":"claude-opus-5-5","stop_reason":"refusal","stop_details":{"type":"refusal","category":"cyber"},"content":[],"usage":{"input_tokens":10,"output_tokens":0}})", "", ""});
        const auto r = a.ask("something odd", stop);
        CHECK(r.end == assistant::End::Refused && has(r.message, "declined") && has(r.message, "cyber"));
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"ok"}])"));
        a.ask("next", stop);
        const auto m2 = json::parse(svc.bodies.back())->get("messages")->a;
        CHECK(m2.back().get("role")->s == "user" && m2.back().get("content")->a.size() == 2);
        CHECK(m2.back().get("content")->a[1].get("text")->s == "next");

        // max_tokens with a tool call: cut short, the call answered as not run
        svc.replies.push_back(ok("max_tokens", R"([{"type":"tool_use","id":"t9","name":"get_sound","input":{}}])"));
        const auto c = a.ask("long", stop);
        CHECK(c.end == assistant::End::CutShort && has(c.message, "cut short"));
        CHECK(a.conversation().a.back().get("content")->a[0].get("tool_use_id")->s == "t9");

        // pause_turn: sent again without a new user message
        svc.replies.push_back(ok("pause_turn", R"([{"type":"text","text":"thinking..."}])"));
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"done"}])"));
        const auto before = svc.bodies.size();
        CHECK(a.ask("pause", stop).end == assistant::End::Done && svc.bodies.size() == before + 2);
        const auto mp = json::parse(svc.bodies.back())->get("messages")->a;
        CHECK(mp.back().get("role")->s == "assistant");

        // Stop: the pending call is not run, answered as stopped
        svc.replies.push_back(ok("tool_use", R"([{"type":"tool_use","id":"s1","name":"set_parameters","input":{"values":{"vcf1.cutoff":500}}}])"));
        stop = true;
        const auto s = a.ask("stop me", stop);
        stop = false;
        CHECK(s.end == assistant::End::Stopped && synth.files["params/vcf1/cutoff"] != "500\n");
        CHECK(has(a.conversation().a.back().get("content")->a[0].get("content")->s, "Stop"));

        // Stop while a request runs: the request fails (cancelled), the answer ends as stopped
        svc.replies.push_back({0, "", "", "stopped"});
        stop = true;
        const auto sc = a.ask("cancel me", stop);
        stop = false;
        CHECK(sc.end == assistant::End::Stopped && sc.message == "stopped");

        // step limit
        a.settings.maxSteps = 3;
        for (int i = 0; i < 5; ++i)
            svc.replies.push_back(ok("tool_use", R"([{"type":"tool_use","id":"l)" + std::to_string(i) + R"(","name":"get_sound","input":{}}])"));
        const auto before2 = svc.bodies.size();
        const auto l = a.ask("loop", stop);
        CHECK(l.end == assistant::End::StepLimit && has(l.message, "3 steps") && svc.bodies.size() == before2 + 3);
        svc.replies.clear();
    }

    // Errors and retries (S12.9), the spending limit (S12.7), Test connection (S12.6).
    {
        Synth synth; Service svc;
        mcp::Server server(synth, {"1.2.1 test", "/nonexistent", "G", "T"});
        assistant::Assistant a(server, synth, svc, "G", "T");
        a.settings.key = key;
        svc.replies.push_back({401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})", "", ""});
        auto r = a.ask("hi", stop);
        CHECK(r.end == assistant::End::Failed && r.message == "the key was not accepted (invalid x-api-key)" && !has(r.message, key));
        svc.replies.push_back({403, R"({"error":{"message":"no access"}})", "", ""});
        CHECK(a.ask("hi", stop).message == "this key may not use this model (no access)");
        svc.replies.push_back({400, R"({"error":{"message":"bad request"}})", "", ""});
        CHECK(a.ask("hi", stop).message == "the service answered 400: bad request");
        svc.replies.push_back({0, "", "", "could not resolve host"});
        CHECK(has(a.ask("hi", stop).message, "no internet connection"));
        // 429 then 529 then success: two retries, waiting retry-after (3 s) then the default 2 s
        std::vector<std::string> status;
        assistant::Events ev;
        ev.status = [&](const std::string& s) { status.push_back(s); };
        svc.replies.push_back({429, R"({"error":{"message":"rate"}})", "3", ""});
        svc.replies.push_back({529, R"({"error":{"message":"overloaded"}})", "", ""});
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"hi"}])"));
        CHECK(a.ask("hi", stop, ev).end == assistant::End::Done && svc.slept == 5000 && status.size() == 2 && status[0] == "busy, trying again");
        // three busy answers: gives up
        for (int i = 0; i < 3; ++i) svc.replies.push_back({500, "", "", ""});
        r = a.ask("hi", stop);
        CHECK(r.end == assistant::End::Failed && r.message == "the service answered 500");
        // the conversation stays valid: only alternating roles
        const auto& conv = a.conversation().a;
        for (std::size_t i = 1; i < conv.size(); ++i) CHECK(conv[i].get("role")->s != conv[i - 1].get("role")->s);
        // spending limit
        a.settings.limit = 0.001;
        CHECK(a.overLimit());
        a.settings.limit = 0;
        CHECK(!a.overLimit());
        // Test connection
        svc.replies.push_back({200, R"({"data":[]})", "", ""});
        CHECK(assistant::testConnection(svc, a.settings).empty() && svc.urls.back() == "https://api.anthropic.com/v1/models");
        CHECK(headerOf(svc.headers.back(), "x-api-key") == key && headerOf(svc.headers.back(), "anthropic-beta").empty());
        svc.replies.push_back({401, R"({"error":{"message":"invalid x-api-key"}})", "", ""});
        CHECK(has(assistant::testConnection(svc, a.settings), "not accepted"));
        assistant::Settings none;
        CHECK(assistant::testConnection(svc, none) == "no key entered");
        // prices: the three models, Haiku (the default) at $0.10 input
        CHECK(assistant::defaultPrices().size() == 3 && a.price() != nullptr && a.price()->in == 0.10);
    }

    // S12.3, S12.7: Haiku 5.5 (the default) — no server-side fallback (the API answers "fallbacks" with 400),
    // and a prompt over 100K tokens costs five times as much.
    {
        Synth synth; Service svc;
        mcp::Server server(synth, {"1.2.1 test", "/nonexistent", "G", "T"});
        assistant::Assistant a(server, synth, svc, "G", "T");
        a.settings.key = key;
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"hi"}])", 1000, 100, 2000, 7000, "claude-haiku-5-5"));
        // (1000*0.10 + 100*0.50 + 2000*0.125 + 7000*0.01)/1e6
        CHECK(near(a.ask("hi", stop).cost, 0.00047));
        const auto body = json::parse(svc.bodies[0]);
        CHECK(body && body->get("model")->s == "claude-haiku-5-5" && body->get("fallbacks") == nullptr);
        CHECK(body->get("output_config")->get("effort")->s == "medium");
        CHECK(headerOf(svc.headers[0], "anthropic-beta").empty() && headerOf(svc.headers[0], "content-type") == "application/json");
        // 90K input + 20K cache read = 110K prompt: the higher rate card, 5x
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"hi"}])", 90000, 1000, 0, 20000, "claude-haiku-5-5"));
        CHECK(near(a.ask("again", stop).cost, 5 * (90000 * 0.10 + 1000 * 0.50 + 20000 * 0.01) / 1e6));
        // exactly 100K: still the lower one
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"hi"}])", 100000, 0, 0, 0, "claude-haiku-5-5"));
        CHECK(near(a.ask("once more", stop).cost, 0.01));
        // a config.ini price for Haiku keeps the long-prompt rule
        const auto p = assistant::parsePrice("claude-haiku-5-5", "0.2 1 0.02");
        CHECK(p && p->longAbove == 100000 && p->longFactor == 5);
        // Sonnet keeps the fallback
        a.settings.model = "claude-sonnet-5-5";
        CHECK(headerOf(assistant::headers(a.settings), "anthropic-beta") == "server-side-fallback-2026-07-01");
        CHECK(json::parse(a.requestBody().dump())->get("fallbacks")->s == "default");
    }

    // S12.7: prices from config.ini replace or add to the built-in ones, and the cost follows them.
    {
        const auto p = assistant::parsePrice("claude-opus-5-5", "3 15 0.3");
        CHECK(p && p->in == 3 && p->out == 15 && p->cacheRead == 0.3 && p->cacheWrite == 3.75 && p->label.rfind("Claude Opus 5.5", 0) == 0);
        CHECK(!assistant::parsePrice("x", "3 15") && !assistant::parsePrice("x", "a b c") && !assistant::parsePrice("x", "1 -2 3"));
        const auto all = assistant::withPrices({*p, *assistant::parsePrice("my-model", "1 2 0.1 1.5")});
        CHECK(all.size() == 4 && all[0].in == 3 && all[3].model == "my-model" && all[3].cacheWrite == 1.5);
        Synth synth; Service svc;
        mcp::Server server(synth, {"1.2.1 test", "/nonexistent", "G", "T"});
        assistant::Assistant a(server, synth, svc, "G", "T");
        a.settings.key = key;
        a.settings.prices = all;
        svc.replies.push_back(ok("end_turn", R"([{"type":"text","text":"hi"}])", 1000000, 0));
        CHECK(near(a.ask("hi", stop).cost, 3.0));
    }

    // S12.5: the tab's line per tool call.
    {
        using assistant::toolLine;
        const auto in = [](const char* t) { return *json::parse(t); };
        CHECK(toolLine("set_parameters", in(R"({"values":{"vcf1.cutoff":800,"vco1.wave":"Pulse"}})"), "Set:\n", false)
              == "set vcf1.cutoff 800, vco1.wave Pulse");
        CHECK(toolLine("set_parameters", in(R"({"values":{"bad.x":1}})"), "Nothing was set.\nRefused:\n  bad.x = 1: params/bad/x: no such file\n", true)
              == "set bad.x 1 (refused: bad.x = 1: params/bad/x: no such file)");
        CHECK(toolLine("play", in(R"({"notes":[48,52,55],"seconds":2})"), "", false) == "played C3 E3 G3 for 2 s");
        CHECK(toolLine("play", in(R"({"notes":[60]})"), "", false) == "played C4 for 1 s");
        CHECK(toolLine("apply_instrument_preset", in(R"j({"name":"Male voice (ooo)"})j"), "", false) == "applied preset Male voice (ooo)");
        CHECK(toolLine("apply_module_preset", in(R"({"module":"delay","name":"Ping-pong"})"), "", false) == "applied delay preset Ping-pong");
        CHECK(toolLine("set_cables", in(R"({"cables":["vcf1.cutoff=lfo2.out@0.3"],"clear_first":true})"), "", false)
              == "removed all cables; cables: vcf1.cutoff=lfo2.out@0.3");
        CHECK(toolLine("save_instrument_preset", in(R"({"name":"Mine"})"), "Not saved: the musician chose Cancel.", false) == "did not save Mine");
        CHECK(toolLine("save_instrument_preset", in(R"({"name":"Mine"})"), "Saved.", false) == "saved preset Mine");
        CHECK(toolLine("get_sound", in("{}"), "", false) == "looked at the sound");
        CHECK(toolLine("apply_instrument_preset", in(R"({"name":"Nope"})"), "unknown preset: Nope\nmore", true)
              == "applied preset Nope (refused: unknown preset: Nope)");
    }

    std::printf(failures ? "%d FAILED\n" : "T-S12-client: all passed\n", failures);
    return failures ? 1 : 0;
}
