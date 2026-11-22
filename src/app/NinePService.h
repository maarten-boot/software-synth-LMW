// SPDX-License-Identifier: AGPL-3.0-or-later
// The synth as a 9P file tree (Spec S10.10–S10.12): builds the Tree from the processor and serves
// it on a Unix socket (and TCP when configured). Callbacks run on 9P connection threads; anything
// that changes the synth, or reads message-thread state, is run on the message thread while the
// 9P thread waits (polling, so stopping never blocks), keeping the audio thread untouched.
#pragma once
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "ninep/Server.h"
#include "ninep/Transport.h"

class LMWSynthProcessor;

class NinePService {
public:
    // Repository URL served as /source (AGPLv3 §13, Spec S10.9).
    static constexpr const char* sourceUrl = "https://github.com/maarten-boot/software-synth-LMW";

    explicit NinePService(LMWSynthProcessor&);
    ~NinePService();                                  // message thread

    // Starts the Unix socket and, when `tcp` is set, TCP (trying up to 32 ports from the given one).
    // Returns the endpoints, e.g. "unix!/run/user/1000/lmw-synth/1234-1.sock tcp!127.0.0.1!5640".
    std::string start(std::optional<std::pair<std::string, int>> tcp);

    // The same files without a socket, for the Assistant tab (Spec S12.2): read a whole file, or write a whole
    // file (applied at once, like a 9P write followed by close). From a background thread only: the files
    // wait for the message thread, as they do for a 9P connection. Empty error = done.
    std::optional<std::string> readFile(const std::string& path, std::string& error) const;
    bool writeFile(const std::string& path, const std::string& text, std::string& error) const;

private:
    // Runs fn on the message thread and returns its result; nothing if stopping or no message loop.
    std::optional<std::string> onMessageThread(std::function<std::string(LMWSynthProcessor&)> fn);
    void build();

    struct Shared {
        LMWSynthProcessor* processor = nullptr;       // null once the processor is going away
        std::atomic<bool> stopping{false};
    };
    std::shared_ptr<Shared> shared;
    lmw::ninep::Tree tree;
    std::unique_ptr<lmw::ninep::Listener> listener;
};
