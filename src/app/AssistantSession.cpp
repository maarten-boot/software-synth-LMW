// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AssistantSession.h"
#include "GuideText.h"
#include "LMWVersion.h"
#include "NinePService.h"
#include "PluginProcessor.h"
#include "TutorialText.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <mutex>

namespace {
template <std::size_t N> std::string joined(const char* const (&parts)[N]) {
    std::string s;
    for (const char* p : parts) s += p;
    return s;
}

// The synth's own files (Spec S12.2): no socket; reads and writes wait for the message thread.
struct InProcessLink final : lmw::mcp::Link {
    const NinePService* service = nullptr;
    std::vector<std::string> running() override { return {"self"}; }
    bool connect(const std::string&, std::string&) override { return service != nullptr; }
    bool connected() const override { return service != nullptr; }
    std::string address() const override { return "self"; }
    std::optional<std::string> read(const std::string& path, std::string& error) override {
        if (service == nullptr) { error = "the synth is not ready"; return std::nullopt; }
        return service->readFile(path, error);
    }
    bool write(const std::string& path, const std::string& text, std::string& error) override {
        if (service == nullptr) { error = "the synth is not ready"; return false; }
        return service->writeFile(path, text, error);
    }
    void sleepMs(int ms) override { juce::Thread::sleep(ms); }
};

// HTTPS with juce::WebInputStream (WinINet, NSURLSession, libcurl on Linux). cancel() ends a running request.
struct JuceHttp final : lmw::assistant::Http {
    std::mutex lock;
    juce::WebInputStream* current = nullptr;
    std::atomic<bool> cancelled{false};

    lmw::assistant::HttpResponse request(const std::string& url, const lmw::assistant::Headers& headers,
                                         const std::string* body) {
        lmw::assistant::HttpResponse r;
        if (cancelled) { r.error = "stopped"; return r; }
        juce::URL u(juce::String::fromUTF8(url.c_str()));
        if (body != nullptr) u = u.withPOSTData(juce::MemoryBlock(body->data(), body->size()));
        juce::WebInputStream in(u, body != nullptr);
        juce::String extra;
        for (const auto& [k, v] : headers) extra << juce::String::fromUTF8(k.c_str()) << ": " << juce::String::fromUTF8(v.c_str()) << "\r\n";
        in.withExtraHeaders(extra).withConnectionTimeout(300000).withNumRedirectsToFollow(0);
        {
            std::lock_guard<std::mutex> g(lock);
            current = &in;
        }
        const bool ok = in.connect(nullptr);
        r.status = std::max(0, in.getStatusCode());       // -1 when cancelled or not connected
        if (ok || r.status > 0) {
            r.body = in.readEntireStreamAsString().toStdString();
            r.retryAfter = in.getResponseHeaders().getValue("retry-after", {}).trim().toStdString();
        }
        if (r.status == 0) r.error = cancelled ? "stopped" : "no answer";
        std::lock_guard<std::mutex> g(lock);
        current = nullptr;
        return r;
    }
    lmw::assistant::HttpResponse post(const std::string& url, const lmw::assistant::Headers& h, const std::string& body) override {
        return request(url, h, &body);
    }
    lmw::assistant::HttpResponse get(const std::string& url, const lmw::assistant::Headers& h) override {
        return request(url, h, nullptr);
    }
    void sleepMs(int ms) override {
        for (int waited = 0; waited < ms && !cancelled; waited += 50) juce::Thread::sleep(50);
    }
    void cancel() {
        cancelled = true;
        std::lock_guard<std::mutex> g(lock);
        if (current != nullptr) current->cancel();
    }
};

// Runs fn on the message thread if the session still exists.
struct Alive {
    std::atomic<bool> yes{true};
};
void post(const std::shared_ptr<Alive>& alive, std::function<void()> fn) {
    juce::MessageManager::callAsync([alive, fn = std::move(fn)] { if (alive->yes) fn(); });
}
} // namespace

struct AssistantSession::Impl {
    InProcessLink link;
    LMWSynthProcessor& proc;
    bool stepOpen = false;                             // S6.14: the answer's undo step (message thread)
    void endStep() { if (stepOpen) { stepOpen = false; proc.endStep(); } }

    JuceHttp http;
    lmw::mcp::Server server;
    lmw::assistant::Assistant assistant;
    std::shared_ptr<Alive> alive = std::make_shared<Alive>();

    explicit Impl(LMWSynthProcessor& p)
        : proc(p), server(link, {LMW_VERSION " " LMW_COMMIT,
                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getFullPathName().toStdString(),
                        joined(lmw::guide::textParts), joined(lmw::tutorial::textParts)}),
          assistant(server, link, http, joined(lmw::guide::textParts), joined(lmw::tutorial::textParts)) {
        link.service = p.ninepService();
        // For tests (T-S12-tab): a fake service instead of api.anthropic.com.
        if (const auto url = juce::SystemStats::getEnvironmentVariable("LMW_ASSISTANT_URL", {}); url.isNotEmpty())
            assistant.settings.baseUrl = url.toStdString();
    }
};

AssistantSession::AssistantSession(LMWSynthProcessor& p) : impl(std::make_unique<Impl>(p)) {}

AssistantSession::~AssistantSession() {
    impl->alive->yes = false;
    stop();
    join();
    impl->endStep();                                   // closed mid-answer: the changes so far are one step
}

void AssistantSession::join() {
    if (worker.joinable()) worker.join();
}

lmw::assistant::Settings& AssistantSession::settings() { return impl->assistant.settings; }
double AssistantSession::sessionCost() const { return impl->assistant.sessionCost(); }
bool AssistantSession::overLimit() const { return impl->assistant.overLimit(); }
// S12.5, S6.14: the tab's Undo takes back the latest step while that is an Assistant answer.
bool AssistantSession::canUndo() const {
    const auto& h = impl->proc.steps();
    return !running && h.canUndo() && h.undoName().rfind(answerPrefix, 0) == 0;
}

bool AssistantSession::ask(const std::string& question, Callbacks cb) {
    if (running || impl->link.service == nullptr) return false;
    join();
    running = true;
    stopFlag = false;
    impl->http.cancelled = false;
    auto shown = question.substr(0, 40);               // S6.14: one step for the whole answer
    for (auto& c : shown) if (c == '\n' || c == '\r') c = ' ';
    impl->proc.beginStep(answerPrefix + shown + (question.size() > 40 ? "..." : ""));
    impl->stepOpen = true;
    worker = std::thread([this, question, cb] {
        const auto alive = impl->alive;
        lmw::assistant::Events ev;
        ev.text = [alive, cb](const std::string& t) { if (cb.text) post(alive, [cb, t] { cb.text(t); }); };
        ev.tool = [alive, cb](const std::string& n, const lmw::json::Value& in, const std::string& r, bool e) {
            if (cb.tool) post(alive, [cb, n, in, r, e] { cb.tool(n, in, r, e); });
        };
        ev.status = [alive, cb](const std::string& s) { if (cb.status) post(alive, [cb, s] { cb.status(s); }); };
        if (cb.confirmSave)
            ev.confirmSave = [this, alive, cb](const std::string& name, bool overwrite) {
                struct Answer { juce::WaitableEvent done; std::atomic<bool> yes{false}; };
                auto answer = std::make_shared<Answer>();
                post(alive, [cb, name, overwrite, answer] {
                    cb.confirmSave(name, overwrite, [answer](bool yes) { answer->yes = yes; answer->done.signal(); });
                });
                while (!answer->done.wait(100))
                    if (stopFlag || !alive->yes) return false;
                return answer->yes.load();
            };
        const auto outcome = impl->assistant.ask(question, stopFlag, ev);
        running = false;
        post(alive, [this, cb, outcome] {
            impl->endStep();
            if (cb.done) cb.done(outcome);
        });
    });
    return true;
}

void AssistantSession::stop() {
    stopFlag = true;
    impl->http.cancel();
}

bool AssistantSession::undo(Callbacks cb) {
    if (!canUndo()) return false;
    const bool ok = impl->proc.undo();
    if (cb.undone) cb.undone(ok, ok ? std::string() : std::string("nothing to undo"));
    return true;
}

void AssistantSession::newConversation() {
    if (!running) impl->assistant.newConversation();
}

std::string AssistantSession::testConnection(const lmw::assistant::Settings& settings) {
    JuceHttp http;
    auto s = settings;
    if (const auto url = juce::SystemStats::getEnvironmentVariable("LMW_ASSISTANT_URL", {}); url.isNotEmpty())
        s.baseUrl = url.toStdString();                     // tests (T-S12-tab): the fake service
    return lmw::assistant::testConnection(http, s);
}
