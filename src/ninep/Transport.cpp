// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ninep/Transport.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <filesystem>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <afunix.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <poll.h>
  #include <signal.h>
  #include <sys/socket.h>
  #include <sys/stat.h>
  #include <sys/un.h>
  #include <unistd.h>
#endif

namespace lmw::ninep {
namespace {

constexpr Socket invalidSocket = -1;
#if defined(_WIN32)
using IoLength = int;                 // recv/send length type differs per platform
#else
using IoLength = std::size_t;
#endif
constexpr int pollMs = 200;

#if defined(_WIN32)
struct WinsockInit {
    WinsockInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WinsockInit() { WSACleanup(); }
};
void ensureNetwork() { static WinsockInit init; }
SOCKET native(Socket s) { return static_cast<SOCKET>(s); }
void closeSocket(Socket s) { if (s != invalidSocket) closesocket(native(s)); }
int pollOne(Socket s, short events, int timeoutMs) {
    WSAPOLLFD p{native(s), events, 0};
    return WSAPoll(&p, 1, timeoutMs);
}
Socket toSocket(SOCKET s) { return s == INVALID_SOCKET ? invalidSocket : static_cast<Socket>(s); }
#else
void ensureNetwork() {}
int native(Socket s) { return static_cast<int>(s); }
void closeSocket(Socket s) { if (s != invalidSocket) ::close(native(s)); }
int pollOne(Socket s, short events, int timeoutMs) {
    pollfd p{native(s), events, 0};
    return ::poll(&p, 1, timeoutMs);
}
Socket toSocket(int s) { return s < 0 ? invalidSocket : static_cast<Socket>(s); }
#endif

// Receives exactly n bytes; false on close, error, stop request or time-out (timeoutMs < 0: none).
bool receiveAll(Socket s, std::uint8_t* data, std::size_t n, const std::atomic<bool>* stop, int timeoutMs) {
    std::size_t got = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs);
    while (got < n) {
        if (stop != nullptr && stop->load()) return false;
        if (timeoutMs >= 0 && std::chrono::steady_clock::now() > deadline) return false;
        const int ready = pollOne(s, POLLIN, pollMs);
        if (ready < 0) return false;
        if (ready == 0) continue;
        const auto r = ::recv(native(s), reinterpret_cast<char*>(data + got), static_cast<IoLength>(n - got), 0);
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

bool sendAll(Socket s, const std::uint8_t* data, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
#if defined(MSG_NOSIGNAL)
        const auto r = ::send(native(s), reinterpret_cast<const char*>(data + sent), static_cast<IoLength>(n - sent), MSG_NOSIGNAL);
#else
        const auto r = ::send(native(s), reinterpret_cast<const char*>(data + sent), static_cast<IoLength>(n - sent), 0);
#endif
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

bool unixAddress(const std::string& path, sockaddr_un& addr) {
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    if (path.size() + 1 > sizeof addr.sun_path) return false;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    return true;
}

bool resolve(const std::string& host, int port, sockaddr_storage& out, socklen_t& len) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0 || result == nullptr) return false;
    std::memcpy(&out, result->ai_addr, result->ai_addrlen);
    len = static_cast<socklen_t>(result->ai_addrlen);
    freeaddrinfo(result);
    return true;
}

} // namespace

std::string socketDirectory() {
    namespace fs = std::filesystem;
    fs::path dir;
#if defined(_WIN32)
    const char* temp = std::getenv("TEMP");
    dir = fs::path(temp != nullptr ? temp : "C:\\Windows\\Temp") / "lmw-synth";
#elif defined(__APPLE__)
    const char* temp = std::getenv("TMPDIR");
    dir = fs::path(temp != nullptr ? temp : "/tmp") / "lmw-synth";
#else
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    dir = runtime != nullptr && *runtime != '\0' ? fs::path(runtime) / "lmw-synth"
                                                 : fs::path("/tmp") / ("lmw-synth-" + std::to_string(::getuid()));
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
#if !defined(_WIN32)
    ::chmod(dir.c_str(), 0700);                    // owner only: 9P2000 has no authentication
#endif
    return dir.string();
}

// Deletes a socket file. Plain C remove(): on Windows an AF_UNIX socket file is a reparse point,
// which std::filesystem may refuse or throw on.
void removeSocketFile(const std::string& path) { std::remove(path.c_str()); }

namespace {
bool processAlive(long pid) {
#if defined(_WIN32)
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) return GetLastError() == ERROR_ACCESS_DENIED;   // exists, owned by someone else
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}
} // namespace

void removeStaleSockets() {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(socketDirectory(), ec)) {
        const auto name = e.path().filename().string();
        const auto dash = name.find('-');
        if (e.path().extension() != ".sock" || dash == std::string::npos || dash == 0) continue;
        long pid = 0;
        bool digits = true;
        for (std::size_t i = 0; i < dash; ++i) {
            if (name[i] < '0' || name[i] > '9') { digits = false; break; }
            pid = pid * 10 + (name[i] - '0');
        }
        if (digits && pid > 0 && !processAlive(pid)) removeSocketFile(e.path().string());
    }
}

std::string uniqueSocketPath() {
    static std::atomic<int> instance{0};
    if (instance.load() == 0) removeStaleSockets();       // left behind by crashed instances
#if defined(_WIN32)
    const unsigned long pid = GetCurrentProcessId();
#else
    const long pid = static_cast<long>(::getpid());
#endif
    return (std::filesystem::path(socketDirectory()) / (std::to_string(pid) + "-" + std::to_string(++instance) + ".sock")).string();
}

Listener::Listener(const Tree& t) : tree(t) { ensureNetwork(); }
Listener::~Listener() { stop(); }

bool Listener::listenUnix(const std::string& path, std::string& error) {
    sockaddr_un addr{};
    if (!unixAddress(path, addr)) { error = "socket path too long: " + path; return false; }
    removeSocketFile(path);
    const Socket s = toSocket(::socket(AF_UNIX, SOCK_STREAM, 0));
    if (s == invalidSocket) { error = "cannot create a Unix socket"; return false; }
    if (::bind(native(s), reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(native(s), 8) != 0) {
        closeSocket(s);
        error = "cannot listen on " + path;
        return false;
    }
#if !defined(_WIN32)
    ::chmod(path.c_str(), 0600);
#endif
    socketPath = path;
    std::lock_guard<std::mutex> lock(mutex);
    listeners.push_back(s);
    threads.emplace_back([this, s] { acceptLoop(s); });
    return true;
}

bool Listener::listenTcp(const std::string& host, int port, std::string& error) {
    sockaddr_storage addr{};
    socklen_t len = 0;
    if (!resolve(host, port, addr, len)) { error = "unknown address " + host; return false; }
    const Socket s = toSocket(::socket(addr.ss_family, SOCK_STREAM, 0));
    if (s == invalidSocket) { error = "cannot create a TCP socket"; return false; }
    int yes = 1;
    ::setsockopt(native(s), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    if (::bind(native(s), reinterpret_cast<sockaddr*>(&addr), len) != 0 || ::listen(native(s), 8) != 0) {
        closeSocket(s);
        error = "cannot listen on " + host + ":" + std::to_string(port);
        return false;
    }
    sockaddr_storage bound{};
    socklen_t boundLen = sizeof bound;
    ::getsockname(native(s), reinterpret_cast<sockaddr*>(&bound), &boundLen);
    boundTcpPort = bound.ss_family == AF_INET6 ? ntohs(reinterpret_cast<sockaddr_in6*>(&bound)->sin6_port)
                                               : ntohs(reinterpret_cast<sockaddr_in*>(&bound)->sin_port);
    std::lock_guard<std::mutex> lock(mutex);
    listeners.push_back(s);
    threads.emplace_back([this, s] { acceptLoop(s); });
    return true;
}

void Listener::stop() {
    stopping = true;
    std::vector<std::thread> joining;
    {
        std::lock_guard<std::mutex> lock(mutex);
        joining.swap(threads);
    }
    for (auto& t : joining) if (t.joinable()) t.join();   // loops notice `stopping` within pollMs
    std::lock_guard<std::mutex> lock(mutex);
    for (Socket s : listeners) closeSocket(s);
    listeners.clear();
    if (!socketPath.empty()) {
        removeSocketFile(socketPath);
        socketPath.clear();
    }
}

void Listener::acceptLoop(Socket listening) {
    while (!stopping) {
        const int ready = pollOne(listening, POLLIN, pollMs);
        if (ready <= 0) continue;
        const Socket c = toSocket(::accept(native(listening), nullptr, nullptr));
        if (c == invalidSocket) continue;
        if (active.load() >= maxConnections) { closeSocket(c); continue; }
        ++active;
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) { --active; closeSocket(c); break; }
        threads.emplace_back([this, c] { serve(c); --active; });
    }
}

void Listener::serve(Socket connection) {
    Session session(tree);
    std::vector<std::uint8_t> message;
    while (!stopping) {
        std::uint8_t sizeBytes[4];
        if (!receiveAll(connection, sizeBytes, 4, &stopping, -1)) break;
        const std::uint32_t size = static_cast<std::uint32_t>(sizeBytes[0]) | (static_cast<std::uint32_t>(sizeBytes[1]) << 8)
                                 | (static_cast<std::uint32_t>(sizeBytes[2]) << 16) | (static_cast<std::uint32_t>(sizeBytes[3]) << 24);
        const std::uint32_t limit = session.msize() > 0 ? session.msize() : maxMsize;
        if (size < headerSize || size > limit) break;        // not 9P, or too large: drop the connection
        message.assign(sizeBytes, sizeBytes + 4);
        message.resize(size);
        if (!receiveAll(connection, message.data() + 4, size - 4, &stopping, 30000)) break;
        const auto reply = session.handle(message.data(), message.size());
        if (!sendAll(connection, reply.data(), reply.size())) break;
    }
    closeSocket(connection);
}

Client::~Client() { close(); }

void Client::close() { closeSocket(sock); sock = invalidSocket; }

bool Client::connectUnix(const std::string& path) {
    ensureNetwork();
    sockaddr_un addr{};
    if (!unixAddress(path, addr)) return false;
    sock = toSocket(::socket(AF_UNIX, SOCK_STREAM, 0));
    if (sock == invalidSocket) return false;
    if (::connect(native(sock), reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) { close(); return false; }
    return true;
}

bool Client::connectTcp(const std::string& host, int port) {
    ensureNetwork();
    sockaddr_storage addr{};
    socklen_t len = 0;
    if (!resolve(host, port, addr, len)) return false;
    sock = toSocket(::socket(addr.ss_family, SOCK_STREAM, 0));
    if (sock == invalidSocket) return false;
    if (::connect(native(sock), reinterpret_cast<sockaddr*>(&addr), len) != 0) { close(); return false; }
    return true;
}

std::vector<std::uint8_t> Client::call(const std::vector<std::uint8_t>& message, int timeoutMs) {
    if (sock == invalidSocket || !sendAll(sock, message.data(), message.size())) return {};
    std::vector<std::uint8_t> reply(4);
    if (!receiveAll(sock, reply.data(), 4, nullptr, timeoutMs)) return {};
    const std::uint32_t size = reply[0] | (reply[1] << 8) | (reply[2] << 16) | (static_cast<std::uint32_t>(reply[3]) << 24);
    if (size < headerSize || size > maxMsize) return {};
    reply.resize(size);
    if (!receiveAll(sock, reply.data() + 4, size - 4, nullptr, timeoutMs)) return {};
    return reply;
}

} // namespace lmw::ninep
