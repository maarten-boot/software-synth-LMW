// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.10 (Phase 27): the Terms page — docs/Terms.md read into topics and terms, and the Find rules of the
// Terms window (as mini-daw's Terms tab). JUCE-free, so it can be unit-tested.
// File form: "## <topic>", then per term "### <name>", its text (one or more lines), "Where: ..." and
// "See also: [Name](#anchor), ...". The anchor is the name in lower case with anything but letters and
// digits as '-' (the same rule scripts/md2html.py gives the headings).
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace lmw::terms {

struct Term {
    std::string name, topic, text, where;
    std::vector<std::string> seeAlso;    // names of other terms
};

inline std::string anchor(std::string_view name) {
    std::string out;
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) != 0) out += static_cast<char>(std::tolower(u));
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

namespace detail {
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}
inline bool startsWith(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }
inline std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
} // namespace detail

// All terms in file order (by topic). Lines before the first topic (the title, the intro) are skipped.
inline std::vector<Term> parse(std::string_view md) {
    std::vector<Term> terms;
    std::string topic;
    while (!md.empty()) {
        const auto nl = md.find('\n');
        const auto line = detail::trim(md.substr(0, nl));
        md = nl == std::string_view::npos ? std::string_view{} : md.substr(nl + 1);
        if (detail::startsWith(line, "### ")) {
            terms.push_back({std::string(detail::trim(line.substr(4))), topic, {}, {}, {}});
        } else if (detail::startsWith(line, "## ")) {
            topic = std::string(detail::trim(line.substr(3)));
        } else if (!terms.empty() && terms.back().topic == topic && !topic.empty() && !line.empty()) {
            auto& t = terms.back();
            if (detail::startsWith(line, "Where:")) {
                t.where = std::string(detail::trim(line.substr(6)));
            } else if (detail::startsWith(line, "See also:")) {
                auto rest = line.substr(9);
                for (auto open = rest.find('['); open != std::string_view::npos; open = rest.find('[')) {
                    const auto close = rest.find(']', open);
                    if (close == std::string_view::npos) break;
                    t.seeAlso.emplace_back(rest.substr(open + 1, close - open - 1));
                    rest = rest.substr(close + 1);
                }
            } else {
                if (!t.text.empty()) t.text += ' ';
                t.text += line;
            }
        }
    }
    return terms;
}

// Spec S7.10 Find: the terms containing `text` (any case, spaces at the ends ignored) in the name, the text or
// the Where line; those whose name has it first, then the others, each group in file order. Empty Find: none.
inline std::vector<const Term*> find(const std::vector<Term>& terms, std::string_view text) {
    const auto want = detail::lower(detail::trim(text));
    std::vector<const Term*> byName, other;
    if (want.empty()) return {};
    for (const auto& t : terms) {
        if (detail::lower(t.name).find(want) != std::string::npos) byName.push_back(&t);
        else if (detail::lower(t.text).find(want) != std::string::npos || detail::lower(t.where).find(want) != std::string::npos)
            other.push_back(&t);
    }
    byName.insert(byName.end(), other.begin(), other.end());
    return byName;
}

inline const Term* byName(const std::vector<Term>& terms, std::string_view name) {
    const auto it = std::find_if(terms.begin(), terms.end(), [&](const Term& t) { return t.name == name; });
    return it != terms.end() ? &*it : nullptr;
}

} // namespace lmw::terms
