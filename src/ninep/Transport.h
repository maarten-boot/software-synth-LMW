// SPDX-License-Identifier: AGPL-3.0-or-later
// 9P transports (Spec S10.4, S10.5): a Unix domain socket (Linux, macOS, Windows 10+) and optional
// TCP. One thread accepts, one thread per connection (at most maxConnections) runs a Session.
// Every blocking call is a poll with a short time-out, so stop() is prompt. Pure C++ (no JUCE).
#pragma once
#include "ninep/Server.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lmw::ninep {

using Socket = std::intptr_t;

class Listener {
public:
    static constexpr int maxConnections = 16;

    explicit Listener(const Tree& t);
    ~Listener();
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    // Unix socket at `path` (an existing socket file there is replaced). False + error on failure.
    bool listenUnix(const std::string& path, std::string& error);
    // TCP on host:port; with port 0 the system picks one (see tcpPort()).
    bool listenTcp(const std::string& host, int port, std::string& error);
    void stop();                                   // closes everything, joins all threads

    const std::string& unixPath() const noexcept { return socketPath; }
    int tcpPort() const noexcept { return boundTcpPort; }
    int connectionCount() const noexcept { return active.load(); }

private:
    void acceptLoop(Socket listening);
    void serve(Socket connection);

    const Tree& tree;
    std::atomic<bool> stopping{false};
    std::atomic<int> active{0};
    std::mutex mutex;
    std::vector<std::thread> threads;
    std::vector<Socket> listeners;
    std::string socketPath;
    int boundTcpPort = 0;
};

// Minimal blocking client for tests and tools: connect, then exchange whole messages.
class Client {
public:
    Client() = default;
    ~Client();
    bool connectUnix(const std::string& path);
    bool connectTcp(const std::string& host, int port);
    // Sends one message and returns the reply (empty on error or after `timeoutMs`).
    std::vector<std::uint8_t> call(const std::vector<std::uint8_t>& message, int timeoutMs = 5000);
    void close();

private:
    Socket sock = -1;
};

// Default per-process directory for the Unix sockets (created with owner-only access):
// Linux $XDG_RUNTIME_DIR/lmw-synth (else /tmp/lmw-synth-<uid>), macOS $TMPDIR/lmw-synth,
// Windows %TEMP%\lmw-synth.
std::string socketDirectory();

// A socket path in socketDirectory() unique to this process and instance: "<pid>-<n>.sock".
std::string uniqueSocketPath();

// Removes "<pid>-<n>.sock" files whose process no longer runs (crashed instances); called by
// uniqueSocketPath() the first time.
void removeStaleSockets();
void removeSocketFile(const std::string& path);

} // namespace lmw::ninep
