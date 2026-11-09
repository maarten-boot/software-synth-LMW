// SPDX-License-Identifier: AGPL-3.0-or-later
// 9P2000 message encoding (Spec S10.3): little-endian integers, strings as len[2] + UTF-8 bytes,
// every message size[4] type[1] tag[2] body. Own implementation, no external 9P library.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::ninep {

enum MsgType : std::uint8_t {
    Tversion = 100, Rversion, Tauth, Rauth, Tattach, Rattach, Terror, Rerror, Tflush, Rflush,
    Twalk, Rwalk, Topen, Ropen, Tcreate, Rcreate, Tread, Rread, Twrite, Rwrite,
    Tclunk, Rclunk, Tremove, Rremove, Tstat, Rstat, Twstat, Rwstat
};

inline constexpr std::uint16_t noTag = 0xFFFF;
inline constexpr std::uint32_t noFid = 0xFFFFFFFFu;
inline constexpr std::uint8_t qtDir = 0x80, qtFile = 0x00;
inline constexpr std::uint32_t dmDir = 0x80000000u;
inline constexpr std::uint8_t oRead = 0, oWrite = 1, oRdWr = 2, oExec = 3, oTrunc = 0x10, oRClose = 0x40;
inline constexpr std::uint32_t headerSize = 7;            // size[4] type[1] tag[2]
inline constexpr std::uint32_t maxMsize = 65536;
inline constexpr std::uint32_t ioHeader = 24;             // Rread/Twrite overhead below msize

struct Qid {
    std::uint8_t type = qtFile;
    std::uint32_t version = 0;
    std::uint64_t path = 0;
};

// Builds one message: start(type, tag), fields..., then take().
class Writer {
public:
    Writer& start(std::uint8_t type, std::uint16_t tag) {
        buf.clear();
        u32(0);
        u8(type);
        u16(tag);
        return *this;
    }
    Writer& u8(std::uint8_t v)   { buf.push_back(v); return *this; }
    Writer& u16(std::uint16_t v) { return le(v, 2); }
    Writer& u32(std::uint32_t v) { return le(v, 4); }
    Writer& u64(std::uint64_t v) { return le(v, 8); }
    Writer& str(std::string_view s) {
        u16(static_cast<std::uint16_t>(s.size()));
        buf.insert(buf.end(), s.begin(), s.end());
        return *this;
    }
    Writer& qid(const Qid& q) { return u8(q.type).u32(q.version).u64(q.path); }
    Writer& bytes(const void* data, std::size_t n) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        buf.insert(buf.end(), p, p + n);
        return *this;
    }
    std::size_t size() const noexcept { return buf.size(); }
    // Overwrites a little-endian integer at `at` (e.g. a count known only at the end).
    void patch16(std::size_t at, std::uint16_t v) { buf[at] = static_cast<std::uint8_t>(v); buf[at + 1] = static_cast<std::uint8_t>(v >> 8); }
    void patch32(std::size_t at, std::uint32_t v) { for (int i = 0; i < 4; ++i) buf[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i)); }
    std::vector<std::uint8_t> take() {
        patch32(0, static_cast<std::uint32_t>(buf.size()));
        return std::move(buf);
    }
    std::vector<std::uint8_t>& raw() noexcept { return buf; }

private:
    Writer& le(std::uint64_t v, int n) {
        for (int i = 0; i < n; ++i) buf.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        return *this;
    }
    std::vector<std::uint8_t> buf;
};

// Reads fields in order; any read past the end sets ok = false (and returns zeros / empty).
class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : p(data), n(size) {}
    bool ok = true;
    std::uint8_t u8()   { return static_cast<std::uint8_t>(le(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(le(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(le(4)); }
    std::uint64_t u64() { return le(8); }
    std::string str() {
        const std::size_t len = u16();
        if (!ok || pos + len > n) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(p + pos), len);
        pos += len;
        return s;
    }
    Qid qid() { Qid q; q.type = u8(); q.version = u32(); q.path = u64(); return q; }
    std::string_view bytes(std::size_t len) {
        if (!ok || pos + len > n) { ok = false; return {}; }
        std::string_view v(reinterpret_cast<const char*>(p + pos), len);
        pos += len;
        return v;
    }
    std::size_t remaining() const noexcept { return n - pos; }

private:
    std::uint64_t le(int bytesWanted) {
        if (!ok || pos + static_cast<std::size_t>(bytesWanted) > n) { ok = false; return 0; }
        std::uint64_t v = 0;
        for (int i = 0; i < bytesWanted; ++i) v |= static_cast<std::uint64_t>(p[pos + static_cast<std::size_t>(i)]) << (8 * i);
        pos += static_cast<std::size_t>(bytesWanted);
        return v;
    }
    const std::uint8_t* p;
    std::size_t n, pos = 0;
};

// One directory entry (stat): size[2] type[2] dev[4] qid[13] mode[4] atime[4] mtime[4] length[8]
// name[s] uid[s] gid[s] muid[s].
struct Stat {
    Qid qid;
    std::uint32_t mode = 0;
    std::uint64_t length = 0;
    std::string name, uid = "lmw", gid = "lmw", muid = "lmw";
};

inline void writeStat(Writer& w, const Stat& s) {
    const std::size_t at = w.size();
    w.u16(0).u16(0).u32(0).qid(s.qid).u32(s.mode).u32(0).u32(0).u64(s.length)
     .str(s.name).str(s.uid).str(s.gid).str(s.muid);
    w.patch16(at, static_cast<std::uint16_t>(w.size() - at - 2));
}

inline Stat readStat(Reader& r) {
    Stat s;
    r.u16(); r.u16(); r.u32();
    s.qid = r.qid();
    s.mode = r.u32();
    r.u32(); r.u32();
    s.length = r.u64();
    s.name = r.str(); s.uid = r.str(); s.gid = r.str(); s.muid = r.str();
    return s;
}

} // namespace lmw::ninep
