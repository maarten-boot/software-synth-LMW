// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S10.4: 9P over real sockets — Unix domain socket and TCP (localhost), parallel clients,
// oversized messages dropped, prompt stop with a client still connected.
#include "ninep/Transport.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace lmw::ninep;
static std::atomic<int> failures{0};
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
std::uint8_t typeOf(const std::vector<std::uint8_t>& m) { return m.size() >= 5 ? m[4] : 0; }

// version + attach as fid 0; true when both succeed.
bool hello(Client& c) {
    Writer w;
    if (typeOf(c.call(w.start(Tversion, noTag).u32(8192).str("9P2000").take())) != Rversion) return false;
    return typeOf(c.call(w.start(Tattach, 1).u32(0).u32(noFid).str("me").str("").take())) == Rattach;
}
std::string readFile(Client& c, std::uint32_t fid, const std::vector<std::string>& path) {
    Writer w;
    w.start(Twalk, 2).u32(0).u32(fid).u16(static_cast<std::uint16_t>(path.size()));
    for (const auto& p : path) w.str(p);
    if (typeOf(c.call(w.take())) != Rwalk) return "<walk>";
    if (typeOf(c.call(w.start(Topen, 3).u32(fid).u8(oRead).take())) != Ropen) return "<open>";
    auto rep = c.call(w.start(Tread, 4).u32(fid).u64(0).u32(1000).take());
    c.call(w.start(Tclunk, 5).u32(fid).take());
    if (typeOf(rep) != Rread) return "<read>";
    Reader r(rep.data(), rep.size());
    r.u32(); r.u8(); r.u16();
    const auto n = r.u32();
    return std::string(r.bytes(n));
}
bool writeFile(Client& c, std::uint32_t fid, const std::vector<std::string>& path, const std::string& data) {
    Writer w;
    w.start(Twalk, 2).u32(0).u32(fid).u16(static_cast<std::uint16_t>(path.size()));
    for (const auto& p : path) w.str(p);
    if (typeOf(c.call(w.take())) != Rwalk) return false;
    if (typeOf(c.call(w.start(Topen, 3).u32(fid).u8(oWrite).take())) != Ropen) return false;
    const bool ok = typeOf(c.call(w.start(Twrite, 4).u32(fid).u64(0).u32(static_cast<std::uint32_t>(data.size()))
                                       .bytes(data.data(), data.size()).take())) == Rwrite;
    return typeOf(c.call(w.start(Tclunk, 5).u32(fid).take())) == Rclunk && ok;
}
} // namespace

int main() {
    std::atomic<int> counter{0};
    Tree tree;
    tree.file("source", [] { return std::string("https://example.org/lmw\n"); });
    tree.file("counter", [&] { return std::to_string(counter.load()) + "\n"; },
              [&](std::string_view) { ++counter; return std::string(); });

    namespace fs = std::filesystem;
    const auto dir = socketDirectory();
    CHECK(fs::is_directory(dir));
    const std::string path = (fs::path(dir) / ("test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count() % 1000000) + ".sock")).string();

    Listener listener(tree);
    std::string error;
    CHECK(listener.listenUnix(path, error));
    CHECK(error.empty());
    CHECK(listener.listenTcp("127.0.0.1", 0, error));
    CHECK(listener.tcpPort() > 0);
    CHECK(fs::exists(path));

    // Unix socket and TCP both serve the same tree.
    {
        Client u, t;
        CHECK(u.connectUnix(path) && hello(u));
        CHECK(t.connectTcp("127.0.0.1", listener.tcpPort()) && hello(t));
        CHECK(readFile(u, 1, {"source"}) == "https://example.org/lmw\n");
        CHECK(readFile(t, 1, {"source"}) == "https://example.org/lmw\n");
    }

    // Parallel clients: 8 x 100 writes, all counted.
    {
        std::vector<std::thread> clients;
        for (int i = 0; i < 8; ++i)
            clients.emplace_back([&] {
                Client c;
                if (!c.connectUnix(path) || !hello(c)) { CHECK(false); return; }
                for (int k = 0; k < 100; ++k) CHECK(writeFile(c, 7, {"counter"}, "1\n"));
            });
        for (auto& t : clients) t.join();
        CHECK(counter.load() == 800);
        Client c;
        CHECK(c.connectUnix(path) && hello(c) && readFile(c, 1, {"counter"}) == "800\n");
    }

    // A message larger than msize closes that connection; others keep working.
    {
        Client bad;
        CHECK(bad.connectUnix(path) && hello(bad));
        std::vector<std::uint8_t> huge(4 + 8193, 0);
        const std::uint32_t size = 8197;
        for (int k = 0; k < 4; ++k) huge[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(size >> (8 * k));
        huge[4] = Tread;
        CHECK(bad.call(huge, 2000).empty());
        Client good;
        CHECK(good.connectUnix(path) && hello(good));
    }

    // stop() with a client still connected returns promptly and removes the socket file.
    Client idle;
    CHECK(idle.connectUnix(path) && hello(idle));
    const auto t0 = std::chrono::steady_clock::now();
    listener.stop();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(ms < 2000);
    CHECK(!fs::exists(path));
    Writer w;
    CHECK(idle.call(w.start(Tclunk, 9).u32(0).take(), 1000).empty());

    if (failures == 0) std::printf("T-S10.4-socket: all passed (stop took %lld ms)\n", static_cast<long long>(ms));
    return failures == 0 ? 0 : 1;
}
