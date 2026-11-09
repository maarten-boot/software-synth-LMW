// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S10.1a: 9P2000 server core — a small client builds real protocol messages and checks the
// replies: version, attach, walk, open, read (files and directories), write (immediate and on
// clunk), clunk, stat, wstat, and the error cases.
#include "ninep/Server.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace lmw::ninep;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
struct Reply {
    std::uint8_t type = 0;
    std::uint16_t tag = 0;
    std::vector<std::uint8_t> raw;
    Reader body() const {   // reads from `raw`: keep the Reply alive while reading
        Reader r(raw.data(), raw.size()); r.u32(); r.u8(); r.u16(); return r; }
    std::string error() const { auto r = body(); return type == Rerror ? r.str() : std::string(); }
};

struct Client {
    Session& s;
    std::uint16_t nextTag = 1;
    Reply send(Writer& out) {
        auto msg = out.take();
        auto answer = s.handle(msg.data(), msg.size());
        Reply rep;
        Reader r(answer.data(), answer.size());
        const auto size = r.u32();
        rep.type = r.u8();
        rep.tag = r.u16();
        CHECK(size == answer.size());
        rep.raw = std::move(answer);
        return rep;
    }
    Writer w;
    Reply version(std::uint32_t msize, const char* v) { w.start(Tversion, noTag).u32(msize).str(v); return send(w); }
    Reply attach(std::uint32_t fid) { w.start(Tattach, nextTag++).u32(fid).u32(noFid).str("me").str(""); return send(w); }
    Reply walk(std::uint32_t fid, std::uint32_t newfid, std::vector<std::string> names) {
        w.start(Twalk, nextTag++).u32(fid).u32(newfid).u16(static_cast<std::uint16_t>(names.size()));
        for (auto& n : names) w.str(n);
        return send(w);
    }
    Reply open(std::uint32_t fid, std::uint8_t mode) { w.start(Topen, nextTag++).u32(fid).u8(mode); return send(w); }
    Reply read(std::uint32_t fid, std::uint64_t off, std::uint32_t count) { w.start(Tread, nextTag++).u32(fid).u64(off).u32(count); return send(w); }
    Reply write(std::uint32_t fid, std::uint64_t off, const std::string& data) {
        w.start(Twrite, nextTag++).u32(fid).u64(off).u32(static_cast<std::uint32_t>(data.size())).bytes(data.data(), data.size());
        return send(w);
    }
    Reply clunk(std::uint32_t fid) { w.start(Tclunk, nextTag++).u32(fid); return send(w); }
    Reply remove(std::uint32_t fid) { w.start(Tremove, nextTag++).u32(fid); return send(w); }
    Reply stat(std::uint32_t fid) { w.start(Tstat, nextTag++).u32(fid); return send(w); }
    Reply create(std::uint32_t fid) { w.start(Tcreate, nextTag++).u32(fid).str("new").u32(0644).u8(oWrite); return send(w); }

    // Whole file through open/read/clunk with a fresh fid.
    std::string cat(std::vector<std::string> path, std::uint32_t chunk = 8) {
        walk(0, 99, std::move(path));
        open(99, oRead);
        std::string out;
        for (std::uint64_t off = 0;;) {
            auto rep = read(99, off, chunk);
            auto r = rep.body();
            const auto n = r.u32();
            if (rep.type != Rread || n == 0) break;
            out += std::string(r.bytes(n));
            off += n;
        }
        clunk(99);
        return out;
    }
};
} // namespace

int main() {
    std::string cutoff = "18000", notes = "first";
    int notesWrites = 0;
    Tree tree;
    tree.file("source", [] { return std::string("https://github.com/maarten-boot/software-synth-LMW\n"); });
    tree.file("params/vcf1/cutoff", [&] { return cutoff + "\n"; }, [&](std::string_view v) -> std::string {
        std::string s(v);
        while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s.find_first_not_of("0123456789.") != std::string::npos) return "bad value";
        cutoff = s;
        return {};
    });
    tree.file("params/vcf1/res", [] { return std::string("0\n"); });
    tree.file("notes", [&] { return notes; }, [&](std::string_view v) { notes = std::string(v); ++notesWrites; return std::string(); }, true);

    Session session(tree);
    Client c{session, 1, {}};

    // version: msize capped, unknown version answered "unknown"; attach needs a version first.
    CHECK(c.attach(0).error() == "version first");
    auto v = c.version(1u << 20, "9P2000.L");
    { auto r = v.body(); CHECK(v.type == Rversion && r.u32() == maxMsize && r.str() == "9P2000"); }
    { auto rep = c.version(8192, "foo"); auto r = rep.body(); r.u32(); CHECK(r.str() == "unknown"); }
    c.version(8192, "9P2000");
    CHECK(session.msize() == 8192);
    auto a = c.attach(0);
    { auto r = a.body(); CHECK(a.type == Rattach && r.qid().type == qtDir); }
    CHECK(c.attach(0).error() == "fid in use");

    // walk: full, partial (error only when the first name fails), clone, "..".
    auto wk = c.walk(0, 1, {"params", "vcf1", "cutoff"});
    { auto r = wk.body(); CHECK(wk.type == Rwalk && r.u16() == 3); r.qid(); r.qid(); CHECK(r.qid().type == qtFile); }
    auto part = c.walk(0, 2, {"params", "nothing"});
    { auto r = part.body(); CHECK(part.type == Rwalk && r.u16() == 1); }
    CHECK(c.stat(2).error() == "unknown fid");                    // partial walk: newfid not created
    CHECK(c.walk(0, 2, {"nothing"}).error() == "file not found");
    CHECK(c.walk(0, 2, {}).type == Rwalk);                        // clone
    { auto rep = c.walk(2, 2, {"params", ".."}); auto r = rep.body(); CHECK(r.u16() == 2); }
    CHECK(c.walk(0, 1, {"source"}).error() == "fid in use");
    CHECK(c.walk(0, 3, {"source", "x"}).type == Rwalk);           // cannot walk into a file: partial
    CHECK(c.clunk(3).error() == "unknown fid");

    // read: whole file in small chunks; past the end gives 0 bytes.
    CHECK(c.cat({"source"}, 7) == "https://github.com/maarten-boot/software-synth-LMW\n");
    CHECK(c.cat({"params", "vcf1", "cutoff"}) == "18000\n");

    // directory read: whole entries only, names in order, read in pieces.
    {
        c.walk(0, 10, {"params", "vcf1"});
        CHECK(c.open(10, oRead).type == Ropen);
        std::vector<std::string> names;
        for (std::uint64_t off = 0;;) {
            auto rep = c.read(10, off, 70);                      // room for one entry (64 bytes) at a time
            auto r = rep.body();
            const auto n = r.u32();
            if (n == 0) break;
            const auto before = r.remaining();
            while (before - r.remaining() < n) { auto st = readStat(r); names.push_back(st.name); CHECK((st.mode & dmDir) == 0); }
            off += n;
        }
        CHECK(names.size() == 2 && names[0] == "cutoff" && names[1] == "res");
        CHECK(c.read(10, 0, 30).error() == "read count too small for a directory entry");
        CHECK(c.read(10, 5, 200).error() == "bad directory offset");     // not an entry boundary
        { auto rep = c.read(10, 100000, 200); auto r = rep.body(); CHECK(rep.type == Rread && r.u32() == 0); }   // past the end
        c.clunk(10);
    }

    // write, immediate: accepted values change the file; bad values are an Rerror.
    CHECK(c.open(1, oWrite).type == Ropen);
    { auto rep = c.write(1, 0, "1000\n"); auto r = rep.body(); CHECK(r.u32() == 5); }
    CHECK(cutoff == "1000");
    CHECK(c.write(1, 0, "loud").error() == "bad value");
    CHECK(cutoff == "1000");
    CHECK(c.read(1, 0, 10).error() == "not open for reading");
    CHECK(c.clunk(1).type == Rclunk);

    // permissions: read-only file, directories, create, remove, ORCLOSE.
    c.walk(0, 4, {"params", "vcf1", "res"});
    CHECK(c.open(4, oWrite).error() == "permission denied");
    CHECK(c.open(4, oRead | oTrunc).error() == "permission denied");
    c.clunk(4);
    c.walk(0, 5, {"params"});
    CHECK(c.open(5, oWrite).error() == "permission denied");
    CHECK(c.create(5).error() == "permission denied");
    CHECK(c.remove(5).error() == "permission denied");
    CHECK(c.stat(5).error() == "unknown fid");                    // remove clunks even when refused
    c.walk(0, 6, {"source"});
    CHECK(c.open(6, oRead | oRClose).error() == "permission denied");
    c.clunk(6);

    // write on clunk: collected in pieces (also out of order), applied once when closed.
    c.walk(0, 7, {"notes"});
    CHECK(c.open(7, oWrite | oTrunc).type == Ropen);
    c.write(7, 6, "world");
    c.write(7, 0, "hello ");
    CHECK(notes == "first" && notesWrites == 0);
    CHECK(c.clunk(7).type == Rclunk);
    CHECK(notes == "hello world" && notesWrites == 1);
    c.walk(0, 7, {"notes"});                                      // opened for writing, nothing written:
    c.open(7, oWrite);                                            // nothing applied
    c.clunk(7);
    CHECK(notesWrites == 1);
    c.walk(0, 7, {"notes"});
    c.open(7, oWrite);
    CHECK(c.write(7, Session::maxCollected, "x").error() == "file too large");
    c.clunk(7);

    // stat and wstat.
    {
        c.walk(0, 8, {"params", "vcf1", "cutoff"});
        auto rep = c.stat(8);
        auto r = rep.body();
        r.u16();
        const auto st = readStat(r);
        CHECK(rep.type == Rstat && r.ok && st.name == "cutoff" && st.mode == 0666u);
        Writer w2;
        w2.start(Twstat, 77).u32(8);
        const std::size_t at = w2.size();
        w2.u16(0);
        Stat keep; keep.name = ""; keep.mode = 0xFFFFFFFFu; keep.length = 0; keep.uid = keep.gid = keep.muid = "";
        writeStat(w2, keep);
        w2.patch16(at, static_cast<std::uint16_t>(w2.size() - at - 2));
        CHECK(c.send(w2).type == Rwstat);                         // truncate request: accepted, no-op
        w2.start(Twstat, 78).u32(8);
        w2.u16(0);
        Stat rename = keep; rename.name = "other"; rename.length = ~std::uint64_t{0};
        writeStat(w2, rename);
        w2.patch16(at, static_cast<std::uint16_t>(w2.size() - at - 2));
        CHECK(c.send(w2).error() == "permission denied");
        c.clunk(8);
    }

    // malformed and unknown messages; flush; a new version drops all fids.
    {
        std::vector<std::uint8_t> bad{9, 0, 0, 0, Tclunk, 1, 0, 0, 0};   // size says 9, fid field cut short
        auto out = session.handle(bad.data(), bad.size());
        Reader r(out.data(), out.size()); r.u32();
        CHECK(r.u8() == Rerror);
        Writer w3; w3.start(Terror, 5);
        CHECK(c.send(w3).error() == "unknown message");
        w3.start(Tflush, 6).u16(5);
        CHECK(c.send(w3).type == Rflush);
    }
    c.version(8192, "9P2000");
    CHECK(c.stat(0).error() == "unknown fid");

    // Robustness (the server reads from a socket): random and mutated messages never crash it,
    // every reply is a well-formed message with the request's tag.
    {
        std::uint32_t seed = 12345;
        auto rnd = [&seed] { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
        Writer good;
        std::vector<std::vector<std::uint8_t>> samples;
        samples.push_back(good.start(Twalk, 3).u32(0).u32(50).u16(2).str("params").str("vcf1").take());
        samples.push_back(good.start(Tread, 3).u32(50).u64(0).u32(100).take());
        samples.push_back(good.start(Twrite, 3).u32(50).u64(0).u32(4).bytes("1000", 4).take());
        samples.push_back(good.start(Topen, 3).u32(50).u8(oRead).take());
        c.attach(0);
        for (int i = 0; i < 50000; ++i) {
            std::vector<std::uint8_t> m;
            if (i % 2 == 0) {                                   // fully random body
                m.resize(7 + rnd() % 40);
                for (auto& b : m) b = static_cast<std::uint8_t>(rnd());
                m[4] = static_cast<std::uint8_t>(100 + rnd() % 28);
            } else {                                            // a real message with a few bytes changed
                m = samples[rnd() % samples.size()];
                for (int k = 0; k < 3; ++k) m[7 + rnd() % (m.size() - 7)] = static_cast<std::uint8_t>(rnd());
                if (rnd() % 4 == 0) m.resize(7 + rnd() % (m.size() - 6));   // truncated
            }
            const auto size = static_cast<std::uint32_t>(m.size());
            for (int k = 0; k < 4; ++k) m[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(size >> (8 * k));
            const std::uint16_t tag = static_cast<std::uint16_t>(m[5] | (m[6] << 8));
            auto out = session.handle(m.data(), m.size());
            Reader r(out.data(), out.size());
            const auto outSize = r.u32(); r.u8();
            if (!(outSize == out.size() && r.u16() == tag)) { CHECK(false); break; }
        }
    }

    if (failures == 0) std::printf("T-S10.1a-ninep: all passed\n");
    return failures == 0 ? 0 : 1;
}
