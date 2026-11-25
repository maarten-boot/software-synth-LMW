// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S7.10-terms: docs/Terms.md (built in as TermsText.h) parsed by src/ui/Terms.h; the Find rules.
#include "ui/Terms.h"
#include "TermsText.h"
#include <cstdio>
#include <map>
#include <set>
#include <string>

namespace t = lmw::terms;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    std::string md;
    for (const char* part : t::textParts) md += part;
    const auto terms = t::parse(md);
    std::printf("%zu terms\n", terms.size());
    CHECK(terms.size() >= 70);
    // Every term: a topic, a text, a Where line and at least one See also naming an existing term; names unique,
    // anchors unique; every topic has terms.
    std::set<std::string> names, anchors;
    std::map<std::string, int> perTopic;
    for (const auto& x : terms) {
        CHECK(!x.topic.empty() && !x.text.empty() && !x.where.empty() && !x.seeAlso.empty());
        if (x.text.empty() || x.where.empty() || x.seeAlso.empty()) std::printf("  incomplete: %s\n", x.name.c_str());
        CHECK(names.insert(x.name).second);
        CHECK(anchors.insert(t::anchor(x.name)).second);
        ++perTopic[x.topic];
        for (const auto& s : x.seeAlso) {
            CHECK(t::byName(terms, s) != nullptr);
            if (!t::byName(terms, s)) std::printf("  %s: see also %s is no term\n", x.name.c_str(), s.c_str());
            CHECK(s != x.name);
        }
    }
    CHECK(perTopic.size() == 9);
    for (const auto& [topic, n] : perTopic) CHECK(n >= 3);
    // Every See also link's anchor in the file is the anchor of the term it names.
    for (std::size_t at = md.find("](#"); at != std::string::npos; at = md.find("](#", at + 1)) {
        const auto open = md.rfind('[', at), close = md.find(')', at);
        const auto name = md.substr(open + 1, at - open - 1), a = md.substr(at + 3, close - at - 3);
        CHECK(t::anchor(name) == a);
        if (t::anchor(name) != a) std::printf("  link [%s](#%s): anchor should be %s\n", name.c_str(), a.c_str(), t::anchor(name).c_str());
    }
    // Anchor rule (as md2html's heading ids).
    CHECK(t::anchor("Patch file") == "patch-file" && t::anchor("dB") == "db" && t::anchor("lmw-mcp") == "lmw-mcp"
          && t::anchor("Semi-modular") == "semi-modular" && t::anchor("9P") == "9p");
    // Find: any case, spaces at the ends ignored; names first; text and Where count; empty = none.
    const auto f = t::find(terms, "  CUT ");
    CHECK(!f.empty() && f.front()->name == "Cutoff");
    bool textHit = false;
    for (const auto* x : f) if (x->name.find("ut") == std::string::npos && x->name.find("UT") == std::string::npos) textHit = true;
    CHECK(textHit);                                       // e.g. Key tracking ("cutoff" in its text)
    CHECK(t::find(terms, "").empty() && t::find(terms, "   ").empty());
    CHECK(t::find(terms, "zzqx").empty());
    const auto w = t::find(terms, "Options");             // only in Where lines / texts
    CHECK(!w.empty());
    if (failures == 0) std::printf("ok\n");
    else std::printf("%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
