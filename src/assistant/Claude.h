// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.2–S12.4, S12.7, S12.9: the Assistant tab's conversation with Claude (Messages API over HTTPS,
// no SDK: there is none for C++). The tools are lmw-mcp's (mcp::Server, S11.4) without list_synths and
// select_synth, plus read_tutorial. HTTP goes through the Http interface (juce::URL in the app, a fake in
// tests/test_assistant.cpp), so all of this is tested without a network. ask() runs the whole tool loop
// and blocks: call it on a background thread (the tools reach the synth through mcp::Link, whose
// in-process version waits for the message thread like a 9P connection does).
#pragma once
#include "mcp/Json.h"
#include "mcp/Server.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace lmw::assistant {

using V = json::Value;
using Headers = std::vector<std::pair<std::string, std::string>>;

struct HttpResponse {
    int status = 0;              // 0: no answer (error says why)
    std::string body;
    std::string retryAfter;      // the retry-after header, seconds
    std::string error;           // transport error
};

struct Http {
    virtual ~Http() = default;
    virtual HttpResponse post(const std::string& url, const Headers& headers, const std::string& body) = 0;
    virtual HttpResponse get(const std::string& url, const Headers& headers) = 0;
    virtual void sleepMs(int ms) = 0;
};

// Dollars per million tokens (S12.7); editable in config.ini later (S12.6).
struct Price {
    std::string model, label;
    double in = 0, out = 0, cacheRead = 0, cacheWrite = 0;
};
inline const std::vector<Price>& defaultPrices() {
    static const std::vector<Price> p{
        {"claude-opus-5-5", "Claude Opus 5.5 (best results)", 4.0, 20.0, 0.20, 5.0},
        {"claude-sonnet-5-5", "Claude Sonnet 5.5 (about half the price)", 2.0, 10.0, 0.10, 2.5},
        {"claude-haiku-5-5", "Claude Haiku 5.5 (cheapest, weaker at longer tasks)", 0.10, 0.50, 0.01, 0.125},
    };
    return p;
}

struct Usage {
    double in = 0, out = 0, cacheWrite = 0, cacheRead = 0;     // tokens
    double cost(const Price& p) const {
        return (in * p.in + out * p.out + cacheWrite * p.cacheWrite + cacheRead * p.cacheRead) / 1e6;
    }
};

struct Settings {
    std::string key;
    std::string model = "claude-opus-5-5";
    std::string effort = "medium";               // low, medium, high
    int maxSteps = 20;                           // requests per question (S12.4)
    double limit = 2.0;                          // dollars per session, 0 = none (S12.7)
    std::string baseUrl = "https://api.anthropic.com";
};

inline constexpr const char* apiVersion = "2026-10-21";
inline constexpr const char* fallbackBeta = "server-side-fallback-2026-07-01";

enum class End { Done, Stopped, StepLimit, CutShort, Refused, Failed };

struct Outcome {
    End end = End::Done;
    std::string message;          // why, for everything but Done
    double cost = 0;              // this answer, dollars
};

struct Events {
    std::function<void(const std::string&)> text;                                      // the answer's words
    std::function<void(const std::string& tool, const V& input, const std::string& result, bool isError)> tool;
    std::function<bool(const std::string& name, bool overwrite)> confirmSave;          // S12.5; unset = allowed
    std::function<void(const std::string&)> status;                                    // "busy, trying again"
};

// The service's error message from an error body, or the status.
inline std::string errorText(const HttpResponse& r) {
    if (r.status <= 0) return "no internet connection (or blocked by a proxy)" + (r.error.empty() ? std::string() : ": " + r.error);
    std::string msg;
    if (auto b = json::parse(r.body))
        if (const auto* e = b->get("error"))
            if (const auto* m = e->get("message"); m != nullptr && m->isString()) msg = m->s;
    if (r.status == 401) return "the key was not accepted" + (msg.empty() ? std::string() : " (" + msg + ")");
    if (r.status == 403) return "this key may not use this model" + (msg.empty() ? std::string() : " (" + msg + ")");
    return "the service answered " + std::to_string(r.status) + (msg.empty() ? std::string() : ": " + msg);
}
inline bool retryable(int status) { return status == 429 || status == 529 || (status >= 500 && status < 600); }

inline Headers headers(const Settings& s) {
    return {{"x-api-key", s.key}, {"anthropic-version", apiVersion}, {"content-type", "application/json"},
            {"anthropic-beta", fallbackBeta}};
}

// S12.6 Test connection: empty when the key works, else the reason.
inline std::string testConnection(Http& http, const Settings& s) {
    if (s.key.empty()) return "no key entered";
    auto h = headers(s);
    h.erase(h.end() - 2, h.end());                    // GET: no body, no beta
    const auto r = http.get(s.baseUrl + "/v1/models", h);
    return r.status == 200 ? std::string() : errorText(r);
}

class Assistant {
public:
    Settings settings;
    std::vector<Price> prices = defaultPrices();

    Assistant(mcp::Server& srv, mcp::Link& l, Http& h, std::string guide, std::string tutorial)
        : server(srv), link(l), http(h), guideText(std::move(guide)), tutorialText(std::move(tutorial)) {
        buildTools();
    }

    // One question, the whole tool loop (S12.4). Blocks; `stop` is checked after every request.
    Outcome ask(const std::string& question, const std::atomic<bool>& stop, const Events& ev = {}) {
        Outcome o;
        {   // S12.5: the sound before this answer, for Undo
            std::string e;
            if (auto p = link.read("patch", e)) undoStack.push_back(*p);
        }
        // A user message: merged into the last one when that is a user message (after Stop or a failure,
        // the conversation ends with the question or tool results; the API needs alternating turns).
        const V textBlock = V::object().set("type", V::string("text")).set("text", V::string(question));
        if (!messages.a.empty() && roleOf(messages.a.back()) == "user") messages.a.back().set("content", contentOf(messages.a.back()).push(textBlock));
        else messages.push(V::object().set("role", V::string("user")).set("content", V::array().push(textBlock)));

        for (int step = 0;; ++step) {
            if (step >= settings.maxSteps) {
                o.end = End::StepLimit;
                o.message = "stopped after " + std::to_string(settings.maxSteps) + " steps";
                return o;
            }
            HttpResponse r;
            for (int attempt = 0;; ++attempt) {
                r = http.post(settings.baseUrl + "/v1/messages", headers(settings), requestBody().dump());
                if (!retryable(r.status) || attempt >= 2) break;
                if (ev.status) ev.status("busy, trying again");
                const int secs = std::clamp(r.retryAfter.empty() ? 2 : std::atoi(r.retryAfter.c_str()), 1, 30);
                http.sleepMs(secs * 1000);
                if (stop) break;
            }
            if (r.status != 200 && stop) {           // Stop cancelled the request (S12.4)
                o.end = End::Stopped;
                o.message = "stopped";
                return o;
            }
            if (r.status != 200) {
                o.end = End::Failed;
                o.message = errorText(r);
                return o;
            }
            std::string perr;
            auto reply = json::parse(r.body, &perr);
            if (!reply || !reply->isObject()) { o.end = End::Failed; o.message = "the answer could not be read: " + perr; return o; }

            count(*reply, o);
            const std::string stopReason = str(reply->get("stop_reason"));
            const V* content = reply->get("content");
            const V contentCopy = content != nullptr && content->isArray() ? *content : V::array();

            if (stopReason == "refusal") {           // not kept: the conversation ends with the user message
                o.end = End::Refused;
                std::string cat;
                if (const auto* d = reply->get("stop_details")) cat = str(d->get("category"));
                o.message = "the model declined this request" + (cat.empty() ? std::string() : " (" + cat + ")");
                return o;
            }
            if (contentCopy.a.empty()) {             // nothing to keep (an empty assistant turn is refused later)
                o.end = stopReason == "max_tokens" ? End::CutShort : End::Done;
                if (o.end == End::CutShort) o.message = "the answer was cut short";
                return o;
            }
            messages.push(V::object().set("role", V::string("assistant")).set("content", contentCopy));
            for (const auto& b : contentCopy.a)
                if (str(b.get("type")) == "text" && ev.text) ev.text(str(b.get("text")));

            if (stopReason == "pause_turn") continue;
            if (stopReason == "tool_use") {
                if (stop) {
                    closePending(contentCopy, "not run: the musician pressed Stop");
                    o.end = End::Stopped;
                    o.message = "stopped";
                    return o;
                }
                if (step + 1 >= settings.maxSteps) {
                    closePending(contentCopy, "not run: the step limit was reached");
                    o.end = End::StepLimit;
                    o.message = "stopped after " + std::to_string(settings.maxSteps) + " steps";
                    return o;
                }
                V results = V::array();
                for (const auto& b : contentCopy.a) {
                    if (str(b.get("type")) != "tool_use") continue;
                    static const V none = V::object();
                    const V* input = b.get("input");
                    if (input == nullptr || !input->isObject()) input = &none;
                    const auto [text, isError] = runTool(str(b.get("name")), *input, ev);
                    if (ev.tool) ev.tool(str(b.get("name")), *input, text, isError);
                    results.push(toolResult(str(b.get("id")), text, isError));
                }
                messages.push(V::object().set("role", V::string("user")).set("content", std::move(results)));
                continue;
            }
            closePending(contentCopy, "not run: the answer was cut short");
            if (stopReason == "max_tokens") { o.end = End::CutShort; o.message = "the answer was cut short"; }
            return o;
        }
    }

    void newConversation() { messages = V::array(); undoStack.clear(); }

    // S12.5: puts back the sound from before the last answer; again for the one before.
    bool canUndo() const { return !undoStack.empty(); }
    bool undo(std::string& error) {
        if (undoStack.empty()) { error = "nothing to undo"; return false; }
        if (!link.write("patch", undoStack.back(), error)) return false;
        undoStack.pop_back();
        return true;
    }

    double sessionCost() const { return session; }
    bool overLimit() const { return settings.limit > 0 && session >= settings.limit; }   // S12.7
    const Price* price() const { return priceOf(settings.model); }

    V requestBody() const {
        V system = V::array().push(V::object().set("type", V::string("text")).set("text", V::string(systemPrompt()))
                                       .set("cache_control", V::object().set("type", V::string("ephemeral"))));
        return V::object()
            .set("model", V::string(settings.model))
            .set("max_tokens", V::number(16000))
            .set("system", std::move(system))
            .set("tools", tools)
            .set("output_config", V::object().set("effort", V::string(settings.effort)))
            .set("fallbacks", V::string("default"))
            .set("messages", messages);
    }
    const V& conversation() const { return messages; }

private:
    static std::string str(const V* v) { return v != nullptr && v->isString() ? v->s : std::string(); }
    static std::string roleOf(const V& m) { return str(m.get("role")); }
    static V contentOf(const V& m) {
        const V* c = m.get("content");
        return c != nullptr && c->isArray() ? *c : V::array();
    }
    static V toolResult(const std::string& id, const std::string& text, bool isError) {
        return V::object().set("type", V::string("tool_result")).set("tool_use_id", V::string(id))
            .set("content", V::string(text)).set("is_error", V::boolean(isError));
    }
    // Every tool_use in `content` gets a result, so the conversation stays valid.
    void closePending(const V& content, const std::string& why) {
        V results = V::array();
        for (const auto& b : content.a)
            if (str(b.get("type")) == "tool_use") results.push(toolResult(str(b.get("id")), why, true));
        if (!results.a.empty()) messages.push(V::object().set("role", V::string("user")).set("content", std::move(results)));
    }

    const Price* priceOf(const std::string& model) const {
        for (const auto& p : prices) if (p.model == model) return &p;
        return nullptr;
    }
    void count(const V& reply, Outcome& o) {
        const V* u = reply.get("usage");
        if (u == nullptr) return;
        const auto num = [&](const char* k) { const V* v = u->get(k); return v != nullptr && v->isNumber() ? v->n : 0.0; };
        Usage use{num("input_tokens"), num("output_tokens"), num("cache_creation_input_tokens"), num("cache_read_input_tokens")};
        const Price* p = priceOf(str(reply.get("model")));       // a fallback answer is billed at its own model
        if (p == nullptr) p = priceOf(settings.model);
        if (p == nullptr) return;
        const double c = use.cost(*p);
        o.cost += c;
        session += c;
    }

    std::pair<std::string, bool> runTool(const std::string& name, const V& input, const Events& ev) {
        if (name == "read_tutorial") return {tutorialText, false};
        if (name == "list_synths" || name == "select_synth") return {"not available inside the synth", true};
        if (name == "save_instrument_preset" && ev.confirmSave) {
            const V* ow = input.get("overwrite");
            if (!ev.confirmSave(str(input.get("name")), ow != nullptr && ow->isBool() && ow->b))
                return {"Not saved: the musician chose Cancel.", false};
        }
        const V r = server.call(name, input);
        std::string text;
        if (const V* c = r.get("content"); c != nullptr && c->isArray())
            for (const auto& b : c->a) text += str(b.get("text"));
        const V* e = r.get("isError");
        return {text, e != nullptr && e->isBool() && e->b};
    }

    void buildTools() {
        tools = V::array();
        const V list = server.toolList();
        if (const V* t = list.get("tools"); t != nullptr && t->isArray())
            for (const auto& tool : t->a) {
                const auto name = str(tool.get("name"));
                if (name == "list_synths" || name == "select_synth") continue;
                tools.push(V::object().set("name", V::string(name)).set("description", *tool.get("description"))
                               .set("input_schema", *tool.get("inputSchema")));
            }
        tools.push(V::object().set("name", V::string("read_tutorial"))
                       .set("description", V::string("The LMW-Synth tutorial: lessons on every module, the Matrix, the "
                                                     "Schematic, presets and effects. Read it to explain how to do "
                                                     "something in the synth's window."))
                       .set("input_schema", V::object().set("type", V::string("object")).set("properties", V::object())));
    }

    std::string systemPrompt() const {
        return "You are the assistant inside LMW-Synth, a semi-modular software synthesizer. You talk with a "
               "musician in the synth's Assistant tab; they may not know synthesizer terms or computers well. "
               "Answer in their language, briefly and in plain words, without Markdown headings or tables (simple "
               "lists are fine). When they describe a sound, make it with the tools: start from the nearest preset, "
               "change a few parameters, then play a note or a chord so they hear it, and say in one or two "
               "sentences what you changed. Ask before big changes they did not ask for. Only save a preset when "
               "they ask; the synth asks them to confirm. They can undo your changes with the Undo last answer "
               "button.\n\nThe sound-design guide follows.\n\n" + guideText;
    }

    mcp::Server& server;
    mcp::Link& link;
    Http& http;
    std::string guideText, tutorialText;
    V tools = V::array();
    V messages = V::array();
    std::vector<std::string> undoStack;
    double session = 0;
};

} // namespace lmw::assistant
