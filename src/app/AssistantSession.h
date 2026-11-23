// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.2: the Assistant tab's engine inside the standalone — the tools of lmw-mcp (mcp::Server) on the
// synth's own 9P files without a socket (NinePService::readFile/writeFile), Claude over HTTPS (juce's
// WebInputStream), and one background thread that runs a question's whole tool loop. Everything the tab
// hears back (text, tool lines, status, the end) arrives on the message thread. Message thread only.
#pragma once
#include "assistant/Claude.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class LMWSynthProcessor;

class AssistantSession {
public:
    struct Callbacks {
        std::function<void(const std::string&)> text;
        std::function<void(const std::string& tool, const lmw::json::Value& input, const std::string& result, bool isError)> tool;
        std::function<void(const std::string&)> status;
        std::function<void(const lmw::assistant::Outcome&)> done;
        // S12.5: ask the musician; call reply(true) to save. Unset: saving is allowed.
        std::function<void(const std::string& name, bool overwrite, std::function<void(bool)> reply)> confirmSave;
        std::function<void(bool ok, const std::string& error)> undone;
    };

    explicit AssistantSession(LMWSynthProcessor&);
    ~AssistantSession();                               // stops and waits for the thread

    lmw::assistant::Settings& settings();
    double sessionCost() const;
    bool overLimit() const;
    bool busy() const noexcept { return running; }
    bool canUndo() const;                              // the latest undo step is an answer (S6.14)
    static constexpr const char* answerPrefix = "Assistant: ";

    // Starts a question on the background thread; false when one is running or there is no 9P service.
    bool ask(const std::string& question, Callbacks callbacks);
    void stop();                                       // ends after the running request (S12.4)
    bool undo(Callbacks callbacks);                    // S12.5: the synth's Undo of that step (S6.14)
    void newConversation();                            // when not busy

    // S12.6 Test connection (blocks: call from a background thread); empty = the key works.
    static std::string testConnection(const lmw::assistant::Settings&);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<bool> running{false};
    std::atomic<bool> stopFlag{false};
    std::thread worker;
    void join();
};
