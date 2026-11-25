// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NinePService.h"
#include "LMWVersion.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/PatchText.h"
#include <chrono>
#include <cstdio>
#include <vector>

namespace {
std::string trimmed(std::string_view s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    return std::string(s);
}
std::string name(std::string_view n) { return std::string(n); }
} // namespace

NinePService::NinePService(LMWSynthProcessor& p) : shared(std::make_shared<Shared>()) {
    shared->processor = &p;
    build();
}

NinePService::~NinePService() {
    shared->stopping = true;
    shared->processor = nullptr;                      // message thread: queued calls now do nothing
    if (listener) listener->stop();                   // waiting 9P threads see `stopping` and return
}

std::optional<std::string> NinePService::onMessageThread(std::function<std::string(LMWSynthProcessor&)> fn) {
    if (shared->stopping || juce::MessageManager::getInstanceWithoutCreating() == nullptr) return std::nullopt;
    struct Pending {
        juce::WaitableEvent done;
        std::string result;
        bool ran = false;
    };
    auto pending = std::make_shared<Pending>();
    auto state = shared;
    juce::MessageManager::callAsync([state, pending, fn] {
        if (state->processor != nullptr) {
            pending->result = fn(*state->processor);
            pending->ran = true;
        }
        pending->done.signal();
    });
    // Spec S10.12: give up after 5 s ("busy"); a change may then still be applied later. Measured by
    // the clock, not by counting waits: a wait(50) can take much longer (macOS timer coalescing: 14.5 s
    // for "5 s" in CI, T-S10.12).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!pending->done.wait(50))
        if (shared->stopping || std::chrono::steady_clock::now() >= deadline) return std::nullopt;
    if (!pending->ran) return std::nullopt;
    return pending->result;
}

std::optional<std::string> NinePService::readFile(const std::string& path, std::string& error) const {
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread()) { error = "not from the message thread"; return std::nullopt; }
    const int id = tree.find(path);
    if (id <= 0 || tree.node(id).dir || !tree.node(id).read) { error = path + ": no such file"; return std::nullopt; }
    return tree.node(id).read();
}

bool NinePService::writeFile(const std::string& path, const std::string& text, std::string& error) const {
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread()) { error = "not from the message thread"; return false; }
    const int id = tree.find(path);
    if (id <= 0 || tree.node(id).dir) { error = path + ": no such file"; return false; }
    if (!tree.node(id).write) { error = path + ": permission denied"; return false; }
    error = tree.node(id).write(text);
    return error.empty();
}

void NinePService::build() {
    using lmw::dsp::In;
    using lmw::dsp::Out;
    auto& proc = *shared->processor;

    tree.file("source", [] { return std::string(sourceUrl) + "\n"; });
    tree.file("version", [] { return std::string(LMW_VERSION " " LMW_COMMIT "\n"); });   // Spec S6.10, S10.10

    // /serial: a number that goes up with every change (Spec S10.10; Kukel polls it instead of /patch).
    tree.file("serial", [this] {
        const auto* p = shared->processor;
        return std::to_string(p != nullptr ? p->changeCount() : 0) + "\n";
    });

    // /keys: "on <note> [velocity]" and "off <note>" lines, played like the on-screen keyboard (Spec
    // S10.10). All lines are checked first; velocity 1-127 (default 100, 0 = off); notes 0-127.
    tree.file("keys",
        [] { return std::string(); },
        [this](std::string_view text) -> std::string {
            struct Key { bool on; int note; int velocity; };
            std::vector<Key> keys;
            while (!text.empty()) {
                const auto nl = text.find('\n');
                const auto line = trimmed(text.substr(0, nl));
                text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
                if (line.empty()) continue;
                std::vector<std::string> words;
                for (std::size_t at = 0; at < line.size();) {
                    const auto start = line.find_first_not_of(" \t", at);
                    if (start == std::string::npos) break;
                    const auto end = line.find_first_of(" \t", start);
                    words.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
                    at = end == std::string::npos ? line.size() : end;
                }
                auto number = [](const std::string& w, int lo, int hi) -> std::optional<int> {
                    if (w.empty() || w.size() > 3 || w.find_first_not_of("0123456789") != std::string::npos) return std::nullopt;
                    const int v = std::stoi(w);
                    return v >= lo && v <= hi ? std::optional<int>(v) : std::nullopt;
                };
                const bool on = !words.empty() && words[0] == "on", off = !words.empty() && words[0] == "off";
                const auto note = words.size() >= 2 ? number(words[1], 0, 127) : std::nullopt;
                const auto velocity = words.size() == 3 ? number(words[2], 0, 127) : std::optional<int>(100);
                if (!(on || off) || !note || !velocity || words.size() > (on ? 3u : 2u)) return "bad key line: " + line;
                keys.push_back({on && *velocity > 0, *note, *velocity});
            }
            return onMessageThread([keys](LMWSynthProcessor& p) {
                for (const auto& k : keys) p.playKey(k.on, k.note, k.velocity);
                return std::string();
            }).value_or("busy");
        });

    // /info: every parameter described as JSON (Spec S10.10; for the Kukel GUI, 2026-11-17).
    tree.file("info", [info = proc.parameterInfo()] { return info; });

    // /params/<module>/<name>: every parameter, the dotted ID as path (Spec S10.10).
    for (auto* param : proc.getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        auto path = "params/" + r->paramID.toStdString();
        std::replace(path.begin(), path.end(), '.', '/');
        tree.file(path,
            [p = &proc, r] { return p->currentText(*r) + "\n"; },          // atomics and a mutex: any thread
            [this, r](std::string_view text) -> std::string {
                const auto value = LMWSynthProcessor::parseValue(*r, juce::String::fromUTF8(text.data(), static_cast<int>(text.size())));
                if (!value) return "bad value";
                const float v = *value;
                return onMessageThread([r, v, written = std::string(text)](LMWSynthProcessor& p) {
                    p.beginStep(r->name.toStdString());                    // Spec S6.14: one step per write
                    p.rememberText(*r, written);                            // reads back as written (S10.10)
                    LMWSynthProcessor::setNormalised(*r, v);
                    p.endStep();
                    return std::string();
                }).value_or("busy");
            });
    }

    // /cables: "input=pins" lines, e.g. "vcf1.cutoff=env2.out@0.5+lfo1.out" (Spec S10.10, S5.14).
    tree.file("cables",
        [this] {
            return onMessageThread([](LMWSynthProcessor& p) {
                std::string out;
                const auto& patch = p.currentPatch();
                for (int i = 0; i < lmw::dsp::numInputs; ++i) {
                    const auto in = static_cast<In>(i);
                    if (!patch.isPatched(in)) continue;
                    out += name(lmw::dsp::inputNames[static_cast<std::size_t>(i)]) + "=" + lmw::dsp::pinsToText(patch, in) + "\n";
                }
                return out;
            }).value_or("");
        },
        [this](std::string_view text) -> std::string {
            // Check every line first, then apply all of them on the message thread.
            // "input=pins" sets all pins of the input; "input=" removes them; "clear" removes every cable.
            struct Change { bool clear = false; In in{}; std::vector<lmw::dsp::Patch::Pin> pins; };
            std::vector<Change> changes;
            while (!text.empty()) {
                const auto nl = text.find('\n');
                const auto line = trimmed(text.substr(0, nl));
                text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
                if (line.empty()) continue;
                if (line == "clear") { changes.push_back({true, {}, {}}); continue; }
                const auto eq = line.find('=');
                if (eq == std::string::npos) return "bad cable line: " + line;
                const auto in = lmw::dsp::inputByName(trimmed(line.substr(0, eq)));
                if (!in) return "unknown input: " + trimmed(line.substr(0, eq));
                const auto pinText = trimmed(line.substr(eq + 1));
                const auto pins = lmw::dsp::pinsFromText(pinText);
                if (!pins) return "bad cables (unknown output, bad level, output twice or more than 4): " + pinText;
                for (const auto& pin : *pins)                           // Spec S5.21
                    if (pin.level < lmw::dsp::minLevel(*in))
                        return "not possible: " + trimmed(line.substr(0, eq)) + " is an audio or gate input: levels 0 to 100 %, "
                               "only modulation inputs take a negative level";
                for (const auto& pin : *pins)                           // Spec S7.6: two domains
                    if (!lmw::dsp::canConnect(*in, static_cast<lmw::dsp::Out>(pin.source)))
                        return "not possible: " + trimmed(line.substr(0, eq)) + " is "
                             + (lmw::dsp::isEffectIn(*in) ? "an effect" : "a synth") + " input, "
                             + name(lmw::dsp::outputNames[static_cast<std::size_t>(pin.source)]) + " is not";
                changes.push_back({false, *in, *pins});
            }
            return onMessageThread([changes](LMWSynthProcessor& p) {
                p.beginStep("cables");                                      // Spec S6.14
                for (const auto& c : changes) {
                    if (c.clear) { p.setPatch(lmw::dsp::Patch{}); continue; }
                    p.requestPatch(c.in, lmw::dsp::Patch::none);
                    for (const auto& pin : c.pins) p.requestPatch(c.in, pin.source, pin.level);
                }
                p.endStep();
                return std::string();
            }).value_or("busy");
        });

    // /patch: the whole patch text; writing loads it when the file is closed (Spec S7.1, S10.11).
    tree.file("patch",
        [this] { return onMessageThread([](LMWSynthProcessor& p) { return p.patchText(p.patchName()).toStdString(); }).value_or(""); },
        [this](std::string_view text) -> std::string {
            const std::string copy(text);
            return onMessageThread([copy](LMWSynthProcessor& p) {
                juce::String error;
                p.beginStep("patch");                                       // Spec S6.14
                const bool ok = p.loadPatchText(juce::String::fromUTF8(copy.c_str()), error);
                p.endStep();
                return ok ? std::string() : error.toStdString();
            }).value_or("busy");
        },
        true);

    // /notes: the Notes text (Spec S7.2).
    // Spec S6.14: writing anything to /undo or /redo takes back or redoes one step; /history lists them.
    tree.file("undo", [] { return std::string(); }, [this](std::string_view) -> std::string {
        return onMessageThread([](LMWSynthProcessor& p) { return p.undo() ? std::string() : std::string("nothing to undo"); }).value_or("busy");
    });
    tree.file("redo", [] { return std::string(); }, [this](std::string_view) -> std::string {
        return onMessageThread([](LMWSynthProcessor& p) { return p.redo() ? std::string() : std::string("nothing to redo"); }).value_or("busy");
    });
    tree.file("history", [this] {
        return onMessageThread([](LMWSynthProcessor& p) { return p.steps().lines(); }).value_or("");
    });

    tree.file("notes",
        [this] { return onMessageThread([](LMWSynthProcessor& p) { return p.notes().toStdString(); }).value_or(""); },
        [this](std::string_view text) -> std::string {
            const std::string copy(text);
            return onMessageThread([copy](LMWSynthProcessor& p) { p.setNotes(juce::String::fromUTF8(copy.c_str())); return std::string(); })
                .value_or("busy");
        },
        true);

    // Spec S7.12 (Phase 31): /preset (w) as the Presets menus, /view (rw) the tab, size, compact view, keyboard and
    // where the panels are, /snapshot (r) the window as PNG. They use the window: without one they say so.
    const auto withEditor = [this](std::function<std::string(LMWSynthEditor&)> fn, std::string none) {
        return onMessageThread([fn, none](LMWSynthProcessor& p) {
            auto* e = dynamic_cast<LMWSynthEditor*>(p.getActiveEditor());
            return e != nullptr ? fn(*e) : none;
        });
    };
    tree.file("preset", [] { return std::string(); }, [withEditor](std::string_view text) -> std::string {
        const std::string copy(text);
        return withEditor([copy](LMWSynthEditor& e) { return e.runPreset(copy); }, "no window open: /preset needs the LMW-Synth window")
            .value_or("busy");
    });
    tree.file("view",
        [withEditor] { return withEditor([](LMWSynthEditor& e) { return e.viewText(); }, "no window\n").value_or(""); },
        [withEditor](std::string_view text) -> std::string {
            const std::string copy(text);
            return withEditor([copy](LMWSynthEditor& e) { return e.applyView(copy); }, "no window open").value_or("busy");
        });
    tree.file("snapshot", [withEditor] { return withEditor([](LMWSynthEditor& e) { return e.snapshotPng(); }, "").value_or(""); });

    // /ports: inputs with their normalled source, then outputs (signal-flow order: source -> input); each line ends
    // with the socket's type (S6.19): audio, cv ("cv+audio" for FM / PM) or gate.
    // Spec S5.17: with parallel VCFs, vcf2.in is normalled from mix.out and formant.in from both VCFs
    // ("vcf1.out@0.5+vcf2.out@0.5", as in /cables).
    tree.file("ports", [p = &proc] {
        const auto routing = p->vcfRouting();          // an atomic parameter: safe on the 9P thread
        std::string ports;
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto ns = lmw::dsp::normals(static_cast<lmw::dsp::In>(i), routing);
            std::string src = ns.n == 0 ? std::string("none") : std::string();
            for (int k = 0; k < ns.n; ++k) {
                const auto& x = ns.s[static_cast<std::size_t>(k)];
                if (k > 0) src += "+";
                src += lmw::dsp::sourceName(x.source);
                if (x.level < 1.0f) { char buf[16]; std::snprintf(buf, sizeof buf, "@%g", static_cast<double>(x.level)); src += buf; }
            }
            const auto in = static_cast<lmw::dsp::In>(i);                 // Spec S6.19: the type at the end
            ports += "input  " + name(src) + " -> " + name(lmw::dsp::inputNames[static_cast<std::size_t>(i)]) + "  "
                   + name(lmw::dsp::typeName(lmw::dsp::typeOf(in))) + (lmw::dsp::alsoTakesAudio(in) ? "+audio" : "") + "\n";
        }
        for (int o = 0; o < lmw::dsp::numOutputs; ++o)
            ports += "output " + name(lmw::dsp::outputNames[static_cast<std::size_t>(o)]) + "  "
                   + name(lmw::dsp::typeName(lmw::dsp::typeOf(static_cast<lmw::dsp::Out>(o)))) + "\n";
        return ports;
    });
}

std::string NinePService::start(std::optional<std::pair<std::string, int>> tcp) {
    listener = std::make_unique<lmw::ninep::Listener>(tree);
    std::string endpoints, error;
    const auto path = lmw::ninep::uniqueSocketPath();
    if (listener->listenUnix(path, error)) endpoints = "unix!" + path;
    else std::fprintf(stderr, "LMW-Synth 9P: %s\n", error.c_str());
    if (tcp) {
        for (int port = tcp->second; port < tcp->second + 32 && port <= 65535; ++port) {
            if (listener->listenTcp(tcp->first, port, error)) {
                endpoints += (endpoints.empty() ? "" : " ") + std::string("tcp!") + tcp->first + "!" + std::to_string(port);
                break;
            }
        }
    }
    return endpoints;
}
