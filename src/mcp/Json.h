// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S11.2: a small JSON reader and writer for lmw-mcp (JSON-RPC over stdio). Objects keep their key
// order; numbers are doubles (integers up to 2^53 are written without a decimal point); strings are UTF-8,
// \uXXXX escapes (with surrogate pairs) are decoded. Pure C++, no dependencies.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lmw::json {

struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;

    static Value null() { return {}; }
    static Value boolean(bool v) { Value x; x.kind = Kind::Bool; x.b = v; return x; }
    static Value number(double v) { Value x; x.kind = Kind::Number; x.n = v; return x; }
    static Value string(std::string v) { Value x; x.kind = Kind::String; x.s = std::move(v); return x; }
    static Value array() { Value x; x.kind = Kind::Array; return x; }
    static Value object() { Value x; x.kind = Kind::Object; return x; }

    bool isNull() const noexcept { return kind == Kind::Null; }
    bool isBool() const noexcept { return kind == Kind::Bool; }
    bool isNumber() const noexcept { return kind == Kind::Number; }
    bool isString() const noexcept { return kind == Kind::String; }
    bool isArray() const noexcept { return kind == Kind::Array; }
    bool isObject() const noexcept { return kind == Kind::Object; }

    // Object member (nullptr when absent or not an object).
    const Value* get(std::string_view key) const noexcept {
        if (kind != Kind::Object) return nullptr;
        for (const auto& [k, v] : o) if (k == key) return &v;
        return nullptr;
    }
    // Sets (replaces) an object member; returns *this for chaining.
    Value& set(std::string key, Value v) {
        kind = Kind::Object;
        for (auto& [k, old] : o) if (k == key) { old = std::move(v); return *this; }
        o.emplace_back(std::move(key), std::move(v));
        return *this;
    }
    Value& push(Value v) { kind = Kind::Array; a.push_back(std::move(v)); return *this; }

    std::string dump() const { std::string out; write(out); return out; }

private:
    static void writeString(std::string& out, std::string_view str) {
        out += '"';
        for (const char ch : str) {
            const auto c = static_cast<unsigned char>(ch);
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                    else out += ch;
            }
        }
        out += '"';
    }
    void write(std::string& out) const {
        switch (kind) {
            case Kind::Null: out += "null"; break;
            case Kind::Bool: out += b ? "true" : "false"; break;
            case Kind::Number: {
                char buf[32];
                if (!std::isfinite(n)) out += "null";
                else if (n == std::floor(n) && std::fabs(n) < 9007199254740992.0) { std::snprintf(buf, sizeof buf, "%.0f", n); out += buf; }
                else { std::snprintf(buf, sizeof buf, "%.17g", n); out += buf; }
                break;
            }
            case Kind::String: writeString(out, s); break;
            case Kind::Array:
                out += '[';
                for (std::size_t i = 0; i < a.size(); ++i) { if (i) out += ','; a[i].write(out); }
                out += ']';
                break;
            case Kind::Object:
                out += '{';
                for (std::size_t i = 0; i < o.size(); ++i) {
                    if (i) out += ',';
                    writeString(out, o[i].first);
                    out += ':';
                    o[i].second.write(out);
                }
                out += '}';
                break;
        }
    }
};

namespace detail {
struct Parser {
    std::string_view t;
    std::size_t i = 0;
    std::string error;
    int depth = 0;

    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i; }
    bool fail(const char* why) { if (error.empty()) error = std::string(why) + " at " + std::to_string(i); return false; }
    bool literal(std::string_view word) {
        if (t.substr(i, word.size()) != word) return fail("bad literal");
        i += word.size();
        return true;
    }
    static void utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    bool hex4(std::uint32_t& v) {
        if (i + 4 > t.size()) return fail("short \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool str(std::string& out) {
        ++i;                                                // opening quote
        while (i < t.size()) {
            const char c = t[i++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) break;
            const char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {             // a surrogate pair
                        std::uint32_t lo = 0;
                        if (t.substr(i, 2) != "\\u") return fail("lone surrogate");
                        i += 2;
                        if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("lone surrogate");
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool value(Value& v) {
        if (++depth > 64) return fail("nested too deeply");
        ws();
        if (i >= t.size()) return fail("unexpected end");
        const char c = t[i];
        bool ok = true;
        if (c == '{') {
            v = Value::object();
            ++i; ws();
            if (i < t.size() && t[i] == '}') ++i;
            else for (;;) {
                ws();
                if (i >= t.size() || t[i] != '"') { ok = fail("expected a key"); break; }
                std::string key;
                if (!str(key)) { ok = false; break; }
                ws();
                if (i >= t.size() || t[i] != ':') { ok = fail("expected ':'"); break; }
                ++i;
                Value member;
                if (!value(member)) { ok = false; break; }
                v.o.emplace_back(std::move(key), std::move(member));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; break; }
                ok = fail("expected ',' or '}'");
                break;
            }
        } else if (c == '[') {
            v = Value::array();
            ++i; ws();
            if (i < t.size() && t[i] == ']') ++i;
            else for (;;) {
                Value item;
                if (!value(item)) { ok = false; break; }
                v.a.push_back(std::move(item));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; break; }
                ok = fail("expected ',' or ']'");
                break;
            }
        } else if (c == '"') {
            v = Value::string({});
            ok = str(v.s);
        } else if (c == 't') { ok = literal("true"); v = Value::boolean(true); }
        else if (c == 'f') { ok = literal("false"); v = Value::boolean(false); }
        else if (c == 'n') { ok = literal("null"); v = Value::null(); }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            const std::size_t start = i;
            if (t[i] == '-') ++i;
            while (i < t.size() && ((t[i] >= '0' && t[i] <= '9') || t[i] == '.' || t[i] == 'e' || t[i] == 'E'
                                    || t[i] == '+' || t[i] == '-')) ++i;
            const std::string num(t.substr(start, i - start));
            char* end = nullptr;
            const double d = std::strtod(num.c_str(), &end);
            if (end == nullptr || *end != '\0' || num == "-") ok = fail("bad number");
            v = Value::number(d);
        } else {
            ok = fail("unexpected character");
        }
        --depth;
        return ok;
    }
};
} // namespace detail

// The whole text must be one JSON value (surrounding white space allowed).
inline std::optional<Value> parse(std::string_view text, std::string* error = nullptr) {
    detail::Parser p{text};
    Value v;
    if (p.value(v)) {
        p.ws();
        if (p.i == text.size()) return v;
        p.fail("text after the value");
    }
    if (error != nullptr) *error = p.error;
    return std::nullopt;
}

} // namespace lmw::json
