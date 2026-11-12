// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NinePService.h"
#include "PluginProcessor.h"
#include "dsp/PatchText.h"
#include <cstdio>

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
    // Spec S10.12: give up after 5 s ("busy"); a change may then still be applied later.
    for (int waited = 0; !pending->done.wait(50); waited += 50)
        if (shared->stopping || waited >= 5000) return std::nullopt;
    if (!pending->ran) return std::nullopt;
    return pending->result;
}

void NinePService::build() {
    using lmw::dsp::In;
    using lmw::dsp::Out;
    auto& proc = *shared->processor;

    tree.file("source", [] { return std::string(sourceUrl) + "\n"; });

    // /params/<module>/<name>: every parameter, the dotted ID as path (Spec S10.10).
    for (auto* param : proc.getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (r == nullptr) continue;
        auto path = "params/" + r->paramID.toStdString();
        std::replace(path.begin(), path.end(), '.', '/');
        tree.file(path,
            [r] { return LMWSynthProcessor::valueText(*r) + "\n"; },       // atomics: any thread
            [this, r](std::string_view text) -> std::string {
                const auto value = LMWSynthProcessor::parseValue(*r, juce::String::fromUTF8(text.data(), static_cast<int>(text.size())));
                if (!value) return "bad value";
                const float v = *value;
                return onMessageThread([r, v](LMWSynthProcessor&) {
                    LMWSynthProcessor::setNormalised(*r, v);
                    return std::string();
                }).value_or("busy");
            });
    }

    // /cables: "input=output" lines (Spec S10.10).
    tree.file("cables",
        [this] {
            return onMessageThread([](LMWSynthProcessor& p) {
                std::string out;
                const auto& patch = p.currentPatch();
                for (int i = 0; i < lmw::dsp::numInputs; ++i) {
                    const auto in = static_cast<In>(i);
                    if (!patch.isPatched(in)) continue;
                    out += name(lmw::dsp::inputNames[static_cast<std::size_t>(i)]) + "="
                         + name(lmw::dsp::outputNames[static_cast<std::size_t>(patch.source(in))]) + "\n";
                }
                return out;
            }).value_or("");
        },
        [this](std::string_view text) -> std::string {
            // Check every line first, then apply all of them on the message thread.
            struct Change { bool clear = false; In in{}; std::int8_t from = lmw::dsp::Patch::none; };
            std::vector<Change> changes;
            while (!text.empty()) {
                const auto nl = text.find('\n');
                const auto line = trimmed(text.substr(0, nl));
                text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
                if (line.empty()) continue;
                if (line == "clear") { changes.push_back({true, {}, lmw::dsp::Patch::none}); continue; }
                const auto eq = line.find('=');
                if (eq == std::string::npos) return "bad cable line: " + line;
                const auto in = lmw::dsp::inputByName(trimmed(line.substr(0, eq)));
                if (!in) return "unknown input: " + trimmed(line.substr(0, eq));
                const auto outName = trimmed(line.substr(eq + 1));
                std::int8_t from = lmw::dsp::Patch::none;
                if (!outName.empty()) {
                    const auto out = lmw::dsp::outputByName(outName);
                    if (!out) return "unknown output: " + outName;
                    from = static_cast<std::int8_t>(*out);
                }
                changes.push_back({false, *in, from});
            }
            return onMessageThread([changes](LMWSynthProcessor& p) {
                for (const auto& c : changes) {
                    if (c.clear) p.setPatch(lmw::dsp::Patch{});
                    else p.requestPatch(c.in, c.from);
                }
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
                return p.loadPatchText(juce::String::fromUTF8(copy.c_str()), error) ? std::string() : error.toStdString();
            }).value_or("busy");
        },
        true);

    // /notes: the Notes text (Spec S7.2).
    tree.file("notes",
        [this] { return onMessageThread([](LMWSynthProcessor& p) { return p.notes().toStdString(); }).value_or(""); },
        [this](std::string_view text) -> std::string {
            const std::string copy(text);
            return onMessageThread([copy](LMWSynthProcessor& p) { p.setNotes(juce::String::fromUTF8(copy.c_str())); return std::string(); })
                .value_or("busy");
        },
        true);

    // /ports: inputs with their normalled source, then outputs (signal-flow order: source -> input).
    std::string ports;
    for (int i = 0; i < lmw::dsp::numInputs; ++i)
        ports += "input  " + name(lmw::dsp::sourceName(lmw::dsp::normalled[static_cast<std::size_t>(i)]))
               + " -> " + name(lmw::dsp::inputNames[static_cast<std::size_t>(i)]) + "\n";
    for (int o = 0; o < lmw::dsp::numOutputs; ++o)
        ports += "output " + name(lmw::dsp::outputNames[static_cast<std::size_t>(o)]) + "\n";
    tree.file("ports", [ports] { return ports; });
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
