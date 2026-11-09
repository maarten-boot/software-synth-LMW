// SPDX-License-Identifier: AGPL-3.0-or-later
// 9P2000 server core (Spec S10.1, S10.3, S10.10, S10.11): a synthetic file tree whose files are
// read/write callbacks, and a Session that answers one connection's messages. No sockets or threads
// here (Transport does that), so it is unit-tested directly (tests/test_ninep.cpp).
#pragma once
#include "ninep/Message.h"
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::ninep {

// A file's write callback returns an error text; empty means accepted.
using ReadFn = std::function<std::string()>;
using WriteFn = std::function<std::string(std::string_view)>;

class Tree {
public:
    struct Node {
        std::string name;
        bool dir = false;
        int parent = 0;
        std::vector<int> children;
        ReadFn read;                  // files: snapshot taken at open
        WriteFn write;                // null = read-only
        bool applyOnClunk = false;    // collect writes, apply once when the file is closed
    };

    Tree() { nodes.push_back({"/", true, 0, {}, {}, {}, false}); }

    // Directory for a '/'-separated path, created if missing ("params/vcf1").
    int dir(std::string_view path) {
        int at = 0;
        while (!path.empty()) {
            const auto slash = path.find('/');
            const std::string part(path.substr(0, slash));
            path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
            if (part.empty()) continue;
            int next = child(at, part);
            if (next < 0) {
                next = static_cast<int>(nodes.size());
                nodes.push_back({part, true, at, {}, {}, {}, false});
                nodes[static_cast<std::size_t>(at)].children.push_back(next);
            }
            at = next;
        }
        return at;
    }

    // File "dir/name" (directories created as needed).
    int file(std::string_view path, ReadFn read, WriteFn write = {}, bool applyOnClunk = false) {
        const auto slash = path.rfind('/');
        const int parent = slash == std::string_view::npos ? 0 : dir(path.substr(0, slash));
        const std::string name(slash == std::string_view::npos ? path : path.substr(slash + 1));
        const int id = static_cast<int>(nodes.size());
        nodes.push_back({name, false, parent, {}, std::move(read), std::move(write), applyOnClunk});
        nodes[static_cast<std::size_t>(parent)].children.push_back(id);
        return id;
    }

    int child(int parent, std::string_view name) const {
        for (int c : nodes[static_cast<std::size_t>(parent)].children)
            if (nodes[static_cast<std::size_t>(c)].name == name) return c;
        return -1;
    }
    const Node& node(int id) const { return nodes[static_cast<std::size_t>(id)]; }

    Qid qid(int id) const {
        return {node(id).dir ? qtDir : qtFile, 0, static_cast<std::uint64_t>(id) + 1};
    }
    Stat stat(int id) const {
        Stat s;
        s.qid = qid(id);
        const auto& n = node(id);
        s.mode = n.dir ? (dmDir | 0555u) : (n.write ? 0666u : 0444u);
        s.name = n.name;
        return s;
    }

private:
    std::vector<Node> nodes;
};

class Session {
public:
    static constexpr std::size_t maxFids = 256;
    static constexpr std::size_t maxCollected = 1u << 20;   // per open file

    explicit Session(const Tree& t) : tree(t) {}

    std::uint32_t msize() const noexcept { return negotiated; }

    // One complete T-message in, one R-message out.
    std::vector<std::uint8_t> handle(const std::uint8_t* msg, std::size_t len) {
        Reader r(msg, len);
        const std::uint32_t size = r.u32();
        const std::uint8_t type = r.u8();
        const std::uint16_t tag = r.u16();
        if (!r.ok || size != len) return error(tag, "bad message");
        switch (type) {
            case Tversion: return version(r, tag);
            case Tauth:    return error(tag, "authentication not required");
            case Tattach:  return attach(r, tag);
            case Tflush:   r.u16(); return reply(Rflush, tag);           // every request is answered at once
            case Twalk:    return walk(r, tag);
            case Topen:    return open(r, tag);
            case Tcreate:  return error(tag, "permission denied");
            case Tread:    return read(r, tag);
            case Twrite:   return write(r, tag);
            case Tclunk:   return clunk(r, tag, false);
            case Tremove:  return clunk(r, tag, true);
            case Tstat:    return stat(r, tag);
            case Twstat:   return wstat(r, tag);
            default:       return error(tag, "unknown message");
        }
    }

private:
    struct Fid {
        int node = 0;
        bool open = false;
        std::uint8_t mode = 0;
        std::string snapshot;                 // file contents at open (reads)
        std::vector<std::uint8_t> dirData;    // directory entries at open
        std::string collected;                // applyOnClunk writes
        bool wrote = false;
    };

    std::vector<std::uint8_t> reply(std::uint8_t type, std::uint16_t tag) { return w.start(type, tag).take(); }
    std::vector<std::uint8_t> error(std::uint16_t tag, std::string_view text) {
        return w.start(Rerror, tag).str(text).take();
    }
    Fid* find(std::uint32_t fid) {
        auto it = fids.find(fid);
        return it == fids.end() ? nullptr : &it->second;
    }
    static bool wantsWrite(std::uint8_t mode) { return (mode & 3) == oWrite || (mode & 3) == oRdWr || (mode & oTrunc); }
    static bool wantsRead(std::uint8_t mode) { return (mode & 3) == oRead || (mode & 3) == oRdWr || (mode & 3) == oExec; }

    std::vector<std::uint8_t> version(Reader& r, std::uint16_t tag) {
        const std::uint32_t requested = r.u32();
        const std::string v = r.str();
        if (!r.ok) return error(tag, "bad message");
        fids.clear();                                           // a version resets the session
        negotiated = std::min(requested, maxMsize);
        const bool ours = v.rfind("9P2000", 0) == 0;
        return w.start(Rversion, tag).u32(negotiated).str(ours ? "9P2000" : "unknown").take();
    }

    std::vector<std::uint8_t> attach(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        r.u32();            // afid: no authentication
        r.str(); r.str();   // uname, aname
        if (!r.ok) return error(tag, "bad message");
        if (negotiated == 0) return error(tag, "version first");
        if (find(fid)) return error(tag, "fid in use");
        if (fids.size() >= maxFids) return error(tag, "too many fids");
        fids[fid] = Fid{};
        return w.start(Rattach, tag).qid(tree.qid(0)).take();
    }

    std::vector<std::uint8_t> walk(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32(), newfid = r.u32();
        const std::uint16_t count = r.u16();
        std::vector<std::string> names;
        for (int i = 0; i < count && r.ok; ++i) names.push_back(r.str());
        if (!r.ok || count > 16) return error(tag, "bad message");
        Fid* from = find(fid);
        if (from == nullptr) return error(tag, "unknown fid");
        if (from->open) return error(tag, "fid is open");
        if (newfid != fid && find(newfid)) return error(tag, "fid in use");
        if (newfid != fid && fids.size() >= maxFids) return error(tag, "too many fids");
        int at = from->node;
        std::vector<Qid> qids;
        for (const auto& name : names) {
            if (!tree.node(at).dir) break;
            const int next = name == ".." ? tree.node(at).parent : (name == "." ? at : tree.child(at, name));
            if (next < 0) break;
            at = next;
            qids.push_back(tree.qid(at));
        }
        if (!names.empty() && qids.empty()) return error(tag, "file not found");
        if (qids.size() == names.size()) {                     // complete walk: newfid points there
            Fid f; f.node = at;
            fids[newfid] = f;
        }
        w.start(Rwalk, tag).u16(static_cast<std::uint16_t>(qids.size()));
        for (const auto& q : qids) w.qid(q);
        return w.take();
    }

    std::vector<std::uint8_t> open(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        const std::uint8_t mode = r.u8();
        if (!r.ok) return error(tag, "bad message");
        Fid* f = find(fid);
        if (f == nullptr) return error(tag, "unknown fid");
        if (f->open) return error(tag, "already open");
        const auto& n = tree.node(f->node);
        if (mode & oRClose) return error(tag, "permission denied");
        if (wantsWrite(mode) && (n.dir || !n.write)) return error(tag, "permission denied");
        if (n.dir) {
            Writer entries;
            for (int c : n.children) writeStat(entries, tree.stat(c));
            f->dirData = std::move(entries.raw());
        } else if (wantsRead(mode) && n.read) {
            f->snapshot = n.read();
        }
        f->open = true;
        f->mode = mode;
        return w.start(Ropen, tag).qid(tree.qid(f->node)).u32(negotiated - ioHeader).take();
    }

    std::vector<std::uint8_t> read(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        const std::uint64_t offset = r.u64();
        std::uint32_t count = r.u32();
        if (!r.ok) return error(tag, "bad message");
        Fid* f = find(fid);
        if (f == nullptr) return error(tag, "unknown fid");
        if (!f->open || !wantsRead(f->mode)) return error(tag, "not open for reading");
        count = std::min(count, negotiated - ioHeader);
        w.start(Rread, tag).u32(0);
        const std::size_t countAt = headerSize;
        std::uint32_t n = 0;
        if (tree.node(f->node).dir) {
            // Whole entries only, starting at `offset`, which must be 0 or where an earlier read
            // ended (an entry boundary); anything else is refused, never used as an entry length.
            const auto entryAt = [&](std::size_t pos) {
                return 2u + (f->dirData[pos] | (static_cast<std::size_t>(f->dirData[pos + 1]) << 8));
            };
            std::size_t at = 0;
            while (at < offset && at + 2 <= f->dirData.size()) at += entryAt(at);
            if (at != offset && offset < f->dirData.size()) return error(tag, "bad directory offset");
            while (at + 2 <= f->dirData.size()) {
                const std::size_t entry = entryAt(at);
                if (at + entry > f->dirData.size()) break;      // (cannot happen: we built dirData)
                if (n + entry > count) {
                    if (n == 0) return error(tag, "read count too small for a directory entry");
                    break;
                }
                w.bytes(f->dirData.data() + at, entry);
                n += static_cast<std::uint32_t>(entry);
                at += entry;
            }
        } else if (offset < f->snapshot.size()) {
            n = static_cast<std::uint32_t>(std::min<std::uint64_t>(count, f->snapshot.size() - offset));
            w.bytes(f->snapshot.data() + offset, n);
        }
        w.patch32(countAt, n);
        return w.take();
    }

    std::vector<std::uint8_t> write(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        const std::uint64_t offset = r.u64();
        const std::uint32_t count = r.u32();
        const std::string_view data = r.bytes(count);
        if (!r.ok) return error(tag, "bad message");
        Fid* f = find(fid);
        if (f == nullptr) return error(tag, "unknown fid");
        if (!f->open || !wantsWrite(f->mode)) return error(tag, "not open for writing");
        const auto& n = tree.node(f->node);
        if (n.applyOnClunk) {
            if (offset + count > maxCollected) return error(tag, "file too large");
            if (f->collected.size() < offset + count) f->collected.resize(static_cast<std::size_t>(offset + count));
            f->collected.replace(static_cast<std::size_t>(offset), count, data);
            f->wrote = true;
        } else {
            const std::string err = n.write(data);
            if (!err.empty()) return error(tag, err);
        }
        return w.start(Rwrite, tag).u32(count).take();
    }

    std::vector<std::uint8_t> clunk(Reader& r, std::uint16_t tag, bool remove) {
        const std::uint32_t fid = r.u32();
        if (!r.ok) return error(tag, "bad message");
        auto it = fids.find(fid);
        if (it == fids.end()) return error(tag, "unknown fid");
        const Fid f = std::move(it->second);
        fids.erase(it);                                          // the fid is gone in every case
        if (remove) return error(tag, "permission denied");
        const auto& n = tree.node(f.node);
        if (f.open && f.wrote && n.applyOnClunk) {
            const std::string err = n.write(f.collected);
            if (!err.empty()) return error(tag, err);
        }
        return reply(Rclunk, tag);
    }

    std::vector<std::uint8_t> stat(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        if (!r.ok) return error(tag, "bad message");
        Fid* f = find(fid);
        if (f == nullptr) return error(tag, "unknown fid");
        w.start(Rstat, tag).u16(0);
        const std::size_t at = headerSize;
        writeStat(w, tree.stat(f->node));
        w.patch16(at, static_cast<std::uint16_t>(w.size() - at - 2));
        return w.take();
    }

    // Only "change nothing" / truncate requests are accepted (shells may send them before writing).
    std::vector<std::uint8_t> wstat(Reader& r, std::uint16_t tag) {
        const std::uint32_t fid = r.u32();
        r.u16();
        const Stat s = readStat(r);
        if (!r.ok) return error(tag, "bad message");
        Fid* f = find(fid);
        if (f == nullptr) return error(tag, "unknown fid");
        const auto& n = tree.node(f->node);
        const bool keepsName = s.name.empty() || s.name == n.name;
        const bool keepsMode = s.mode == 0xFFFFFFFFu;
        const bool lengthOk = s.length == ~std::uint64_t{0} || (s.length == 0 && n.write);
        if (!(keepsName && keepsMode && lengthOk)) return error(tag, "permission denied");
        return reply(Rwstat, tag);
    }

    const Tree& tree;
    std::map<std::uint32_t, Fid> fids;
    std::uint32_t negotiated = 0;
    Writer w;
};

} // namespace lmw::ninep
