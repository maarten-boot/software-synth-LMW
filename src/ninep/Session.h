// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S11.2: a 9P2000 client session (ClientSession; Session is the server side, Server.h) for tools (lmw-mcp): connect to an LMW-Synth address, read a whole
// file, write a whole file (applied when it is closed, S10.11). Errors come back as text (the server's
// Rerror text, or why the connection failed); after a transport failure connected() is false, so the
// caller can connect again (e.g. the synth was restarted). One file at a time (fid 1); pure C++.
#pragma once
#include "ninep/Message.h"
#include "ninep/Transport.h"
#include <algorithm>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace lmw::ninep {

class ClientSession {
public:
    // address: a Unix socket path, unix!<path>, or tcp!<host>!<port> (as lmw9p).
    bool connect(const std::string& address, std::string& error) {
        close();
        bool ok = false;
        if (address.rfind("tcp!", 0) == 0) {
            const auto rest = address.substr(4);
            const auto bang = rest.rfind('!');
            try {
                ok = bang != std::string::npos && c.connectTcp(rest.substr(0, bang), std::stoi(rest.substr(bang + 1)));
            } catch (...) { ok = false; }
        } else {
            ok = c.connectUnix(address.rfind("unix!", 0) == 0 ? address.substr(5) : address);
        }
        if (!ok) { error = "cannot connect to " + address; return false; }
        auto v = c.call(w.start(Tversion, noTag).u32(65536).str("9P2000").take());
        if (!check(v, Rversion, "version", error)) { close(); return false; }
        Reader r(v.data(), v.size()); r.u32(); r.u8(); r.u16();
        msize = std::max<std::uint32_t>(r.u32(), ioHeader + 64);
        if (!check(c.call(w.start(Tattach, nextTag()).u32(0).u32(noFid).str("lmw-mcp").str("").take()), Rattach, "attach", error)) {
            close();
            return false;
        }
        live = true;
        current = address;
        return true;
    }
    bool connected() const noexcept { return live; }
    const std::string& address() const noexcept { return current; }
    void close() { c.close(); live = false; current.clear(); }

    std::optional<std::string> read(const std::string& path, std::string& error) {
        if (!open(path, oRead, error)) return std::nullopt;
        std::string out;
        for (std::uint64_t off = 0;;) {
            auto rep = c.call(w.start(Tread, nextTag()).u32(1).u64(off).u32(msize - ioHeader).take());
            if (!check(rep, Rread, path, error)) { clunk(); return std::nullopt; }
            Reader r(rep.data(), rep.size()); r.u32(); r.u8(); r.u16();
            const auto n = r.u32();
            if (n == 0) break;
            out += std::string(r.bytes(n));
            off += n;
        }
        if (!clunk(&error)) return std::nullopt;
        return out;
    }
    bool write(const std::string& path, const std::string& text, std::string& error) {
        if (!open(path, oWrite | oTrunc, error)) return false;
        const std::uint32_t chunk = msize - ioHeader;
        for (std::size_t off = 0; off < text.size(); off += chunk) {
            const auto n = static_cast<std::uint32_t>(std::min<std::size_t>(chunk, text.size() - off));
            auto rep = c.call(w.start(Twrite, nextTag()).u32(1).u64(off).u32(n).bytes(text.data() + off, n).take());
            if (!check(rep, Rwrite, path, error)) { clunk(); return false; }
        }
        return clunk(&error);                              // /patch and /notes apply here
    }

private:
    std::uint16_t nextTag() { tag = tag >= 0xFFFE ? 1 : static_cast<std::uint16_t>(tag + 1); return tag; }
    bool check(const std::vector<std::uint8_t>& rep, std::uint8_t want, const std::string& what, std::string& error) {
        if (rep.size() >= 7 && rep[4] == want) return true;
        if (rep.size() >= 7 && rep[4] == Rerror) {
            Reader r(rep.data(), rep.size());
            r.u32(); r.u8(); r.u16();
            error = what + ": " + r.str();
        } else {
            error = what + ": no answer (LMW-Synth closed?)";
            live = false;
        }
        return false;
    }
    bool open(const std::string& path, std::uint8_t mode, std::string& error) {
        if (!live) { error = "not connected"; return false; }
        std::vector<std::string> parts;
        std::stringstream ss(path);
        for (std::string p; std::getline(ss, p, '/');) if (!p.empty()) parts.push_back(p);
        w.start(Twalk, nextTag()).u32(0).u32(1).u16(static_cast<std::uint16_t>(parts.size()));
        for (const auto& p : parts) w.str(p);
        auto rep = c.call(w.take());
        if (!check(rep, Rwalk, path, error)) return false;
        Reader r(rep.data(), rep.size()); r.u32(); r.u8(); r.u16();
        if (r.u16() != parts.size()) { error = path + ": no such file"; return false; }
        if (!check(c.call(w.start(Topen, nextTag()).u32(1).u8(mode).take()), Ropen, path, error)) { clunk(); return false; }
        return true;
    }
    bool clunk(std::string* error = nullptr) {
        std::string e;
        const bool ok = check(c.call(w.start(Tclunk, nextTag()).u32(1).take()), Rclunk, "close", e);
        if (!ok && error != nullptr) *error = e;
        return ok;
    }

    Client c;
    Writer w;
    std::uint16_t tag = 0;
    std::uint32_t msize = 8192;
    bool live = false;
    std::string current;
};

// The LMW-Synth sockets in socketDirectory(), newest first.
inline std::vector<std::string> runningSockets() {
    namespace fs = std::filesystem;
    std::vector<std::pair<fs::file_time_type, std::string>> found;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(socketDirectory(), ec)) {
        if (e.path().extension() != ".sock") continue;
        found.emplace_back(e.last_write_time(ec), e.path().string());
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::string> out;
    for (auto& f : found) out.push_back(std::move(f.second));
    return out;
}

} // namespace lmw::ninep
