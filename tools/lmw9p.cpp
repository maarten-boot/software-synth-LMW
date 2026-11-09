// SPDX-License-Identifier: AGPL-3.0-or-later
// lmw9p: a tiny 9P2000 client for LMW-Synth (Spec S10), for trying the server without plan9port.
//   lmw9p <address> ls [dir]
//   lmw9p <address> cat <file>
//   lmw9p <address> write <file> [text...]      (no text: read it from stdin)
//   lmw9p <address> bench <file> <rate/s> <seconds> <value> [value...]
//       load test (Spec T-S10.2): writes the values in turn at a fixed rate on one connection and
//       prints the reply latency (median, 99th percentile, maximum) and the number of errors.
// <address>: a Unix socket path, unix!<path>, or tcp!<host>!<port>. With no address the newest
// socket in the LMW-Synth socket directory is used.
#include "ninep/Transport.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace lmw::ninep;

namespace {
std::vector<std::uint8_t> lastReply;

bool check(const std::vector<std::uint8_t>& rep, std::uint8_t want, const char* what) {
    if (rep.size() >= 7 && rep[4] == want) return true;
    if (rep.size() >= 7 && rep[4] == Rerror) {
        Reader r(rep.data(), rep.size());
        r.u32(); r.u8(); r.u16();
        std::fprintf(stderr, "lmw9p: %s: %s\n", what, r.str().c_str());
    } else {
        std::fprintf(stderr, "lmw9p: %s: no answer\n", what);
    }
    return false;
}

std::vector<std::string> split(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    for (std::string p; std::getline(ss, p, '/');) if (!p.empty()) parts.push_back(p);
    return parts;
}

struct Session9P {
    Client c;
    Writer w;
    std::uint16_t tag = 1;
    std::uint32_t msize = 8192;

    bool connect(const std::string& address) {
        bool ok = false;
        if (address.rfind("tcp!", 0) == 0) {
            const auto rest = address.substr(4);
            const auto bang = rest.rfind('!');
            ok = bang != std::string::npos && c.connectTcp(rest.substr(0, bang), std::stoi(rest.substr(bang + 1)));
        } else {
            ok = c.connectUnix(address.rfind("unix!", 0) == 0 ? address.substr(5) : address);
        }
        if (!ok) { std::fprintf(stderr, "lmw9p: cannot connect to %s\n", address.c_str()); return false; }
        auto v = c.call(w.start(Tversion, noTag).u32(65536).str("9P2000").take());
        if (!check(v, Rversion, "version")) return false;
        Reader r(v.data(), v.size()); r.u32(); r.u8(); r.u16();
        msize = r.u32();
        return check(c.call(w.start(Tattach, tag++).u32(0).u32(noFid).str("lmw9p").str("").take()), Rattach, "attach");
    }
    bool openPath(const std::string& path, std::uint8_t mode) {
        const auto parts = split(path);
        w.start(Twalk, tag++).u32(0).u32(1).u16(static_cast<std::uint16_t>(parts.size()));
        for (const auto& p : parts) w.str(p);
        auto rep = c.call(w.take());
        if (!check(rep, Rwalk, path.c_str())) return false;
        Reader r(rep.data(), rep.size()); r.u32(); r.u8(); r.u16();
        if (r.u16() != parts.size()) { std::fprintf(stderr, "lmw9p: %s: file not found\n", path.c_str()); return false; }
        return check(c.call(w.start(Topen, tag++).u32(1).u8(mode).take()), Ropen, path.c_str());
    }
    std::string readAll() {
        std::string out;
        for (std::uint64_t off = 0;;) {
            auto rep = c.call(w.start(Tread, tag++).u32(1).u64(off).u32(msize - ioHeader).take());
            if (!check(rep, Rread, "read")) break;
            Reader r(rep.data(), rep.size()); r.u32(); r.u8(); r.u16();
            const auto n = r.u32();
            if (n == 0) break;
            out += std::string(r.bytes(n));
            off += n;
        }
        return out;
    }
    bool clunk() { return check(c.call(w.start(Tclunk, tag++).u32(1).take()), Rclunk, "close"); }
};

std::string newestSocket() {
    namespace fs = std::filesystem;
    std::string best;
    fs::file_time_type bestTime{};
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(socketDirectory(), ec)) {
        if (e.path().extension() != ".sock") continue;
        const auto t = e.last_write_time(ec);
        if (best.empty() || t > bestTime) { best = e.path().string(); bestTime = t; }
    }
    return best;
}
} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (!args.empty() && args[0] != "ls" && args[0] != "cat" && args[0] != "write" && args[0] != "bench") {
        // first argument is the address
    } else {
        const auto found = newestSocket();
        if (found.empty()) { std::fprintf(stderr, "lmw9p: no LMW-Synth socket in %s\n", socketDirectory().c_str()); return 1; }
        args.insert(args.begin(), found);
    }
    if (args.size() < 2) {
        std::fprintf(stderr, "usage: lmw9p [address] ls [dir] | cat <file> | write <file> [text...]\n");
        return 2;
    }
    Session9P s;
    if (!s.connect(args[0])) return 1;
    const std::string& cmd = args[1];
    if (cmd == "ls") {
        if (!s.openPath(args.size() > 2 ? args[2] : "", oRead)) return 1;
        const auto data = s.readAll();
        Reader r(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
        while (r.remaining() > 0 && r.ok) {
            const auto st = readStat(r);
            std::printf("%s%s\n", st.name.c_str(), (st.mode & dmDir) ? "/" : "");
        }
        return s.clunk() ? 0 : 1;
    }
    if (cmd == "cat" && args.size() > 2) {
        if (!s.openPath(args[2], oRead)) return 1;
        std::fputs(s.readAll().c_str(), stdout);
        return s.clunk() ? 0 : 1;
    }
    if (cmd == "write" && args.size() > 2) {
        std::string text;
        if (args.size() > 3) {
            for (std::size_t i = 3; i < args.size(); ++i) text += (i > 3 ? " " : "") + args[i];
            text += "\n";
        } else {
            text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
        }
        if (!s.openPath(args[2], oWrite | oTrunc)) return 1;
        const std::uint32_t chunk = s.msize - ioHeader;
        for (std::size_t off = 0; off < text.size(); off += chunk) {
            const auto n = static_cast<std::uint32_t>(std::min<std::size_t>(chunk, text.size() - off));
            auto rep = s.c.call(s.w.start(Twrite, s.tag++).u32(1).u64(off).u32(n).bytes(text.data() + off, n).take());
            if (!check(rep, Rwrite, args[2].c_str())) { s.clunk(); return 1; }
        }
        return s.clunk() ? 0 : 1;                    // /patch and /notes apply here
    }
    if (cmd == "bench" && args.size() > 5) {
        const double rate = std::stod(args[3]), seconds = std::stod(args[4]);
        const std::vector<std::string> values(args.begin() + 5, args.end());
        if (!s.openPath(args[2], oWrite)) return 1;
        using clock = std::chrono::steady_clock;
        const auto period = std::chrono::duration<double>(1.0 / rate);
        const auto t0 = clock::now();
        std::vector<double> latency;
        int errors = 0;
        for (std::size_t i = 0; clock::now() - t0 < std::chrono::duration<double>(seconds); ++i) {
            std::this_thread::sleep_until(t0 + std::chrono::duration_cast<clock::duration>(period * static_cast<double>(i)));
            const std::string v = values[i % values.size()] + "\n";
            const auto start = clock::now();
            auto rep = s.c.call(s.w.start(Twrite, s.tag++).u32(1).u64(0).u32(static_cast<std::uint32_t>(v.size())).bytes(v.data(), v.size()).take());
            latency.push_back(std::chrono::duration<double, std::milli>(clock::now() - start).count());
            if (rep.size() < 5 || rep[4] != Rwrite) ++errors;
        }
        const double elapsed = std::chrono::duration<double>(clock::now() - t0).count();
        std::sort(latency.begin(), latency.end());
        const auto pct = [&](double p) { return latency.empty() ? 0.0 : latency[std::min(latency.size() - 1, static_cast<std::size_t>(p * static_cast<double>(latency.size())))]; };
        std::printf("%zu writes in %.1f s (%.0f/s), errors %d, latency ms: median %.3f, p99 %.3f, max %.3f\n",
                    latency.size(), elapsed, static_cast<double>(latency.size()) / elapsed, errors, pct(0.5), pct(0.99),
                    latency.empty() ? 0.0 : latency.back());
        s.clunk();
        return errors == 0 ? 0 : 1;
    }
    std::fprintf(stderr, "usage: lmw9p [address] ls [dir] | cat <file> | write <file> [text...] | bench <file> <rate> <s> <values...>\n");
    return 2;
}
