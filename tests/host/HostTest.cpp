// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S9.2-host: loads the built LMW-Synth.vst3 through JUCE's VST3 hosting (as a DAW would) and
// checks scan, playing, state save/restore into a second instance, the reverb (S7.4), two instances with distinct 9P
// endpoints (S10.5), 9P changes answering "busy" while the message thread is blocked (S10.12), and - when a
// display exists - the editor. Usage: LMWHostTest <path to .vst3>
#include "ninep/Transport.h"
#include "app/ModulePresets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX                 // keep std::max usable
 #endif
 #include <windows.h>
#else
 #include <unistd.h>
#endif
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

// Renders `blocks` blocks: note-on (60, 64, 67) in block 0, note-off in block `releaseAt`.
std::vector<float> render(juce::AudioPluginInstance& p, int blocks, int releaseAt) {
    std::vector<float> out;
    juce::AudioBuffer<float> buffer(2, blockSize);
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        for (int note : {60, 64, 67}) {
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
            if (b == releaseAt) midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        }
        buffer.clear();
        p.processBlock(buffer, midi);
        for (int i = 0; i < blockSize; ++i) out.push_back(buffer.getSample(0, i));
    }
    return out;
}

double rms(const std::vector<float>& x, std::size_t from, std::size_t to) {
    double s = 0;
    for (std::size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(to - from));
}

std::map<juce::String, float> parameterValues(juce::AudioPluginInstance& p) {
    std::map<juce::String, float> v;
    for (auto* prm : p.getParameters()) v[prm->getName(100)] = prm->getValue();
    return v;
}

juce::AudioProcessorParameter* findParameter(juce::AudioPluginInstance& p, const juce::String& name) {
    for (auto* prm : p.getParameters())
        if (prm->getName(100) == name) return prm;
    return nullptr;
}

// The 9P sockets of this process's plugin instances: <socket directory>/<pid>-<n>.sock (NinePService).
std::vector<std::string> ourSockets() {
    namespace fs = std::filesystem;
#if JUCE_WINDOWS
    const auto pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto pid = static_cast<long>(getpid());
#endif
    const auto prefix = std::to_string(pid) + "-";
    std::vector<std::string> found;
    std::error_code ec;
    for (fs::directory_iterator it(fs::path(lmw::ninep::socketDirectory()), ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind(prefix, 0) == 0) found.push_back(it->path().string());
    return found;
}
int countOurSockets() { return static_cast<int>(ourSockets().size()); }

// A minimal 9P2000 client for one file (as tools/lmw9p.cpp): connect, attach, walk, open.
struct File9P {
    lmw::ninep::Client c;
    lmw::ninep::Writer w;
    std::uint16_t tag = 1;
    std::string error;                                 // the last Rerror text, or "no answer"

    bool ok(const std::vector<std::uint8_t>& rep, std::uint8_t want) {
        if (rep.size() >= 7 && rep[4] == want) return true;
        error = "no answer";
        if (rep.size() >= 7 && rep[4] == lmw::ninep::Rerror) {
            lmw::ninep::Reader r(rep.data(), rep.size());
            r.u32(); r.u8(); r.u16();
            error = r.str();
        }
        return false;
    }
    bool open(const std::string& socket, std::initializer_list<const char*> path, std::uint8_t mode) {
        using namespace lmw::ninep;
        if (!c.connectUnix(socket)) { error = "cannot connect"; return false; }
        if (!ok(c.call(w.start(Tversion, noTag).u32(8192).str("9P2000").take()), Rversion)) return false;
        if (!ok(c.call(w.start(Tattach, tag++).u32(0).u32(noFid).str("host-test").str("").take()), Rattach)) return false;
        w.start(Twalk, tag++).u32(0).u32(1).u16(static_cast<std::uint16_t>(path.size()));
        for (const char* p : path) w.str(p);
        if (!ok(c.call(w.take()), Rwalk)) return false;
        return ok(c.call(w.start(Topen, tag++).u32(1).u8(mode).take()), Ropen);   // fails after a partial walk
    }
    // "" when written, else the error ("busy", ...). The server may wait 5 s before answering.
    std::string write(const std::string& text) {
        using namespace lmw::ninep;
        w.start(Twrite, tag++).u32(1).u64(0).u32(static_cast<std::uint32_t>(text.size())).bytes(text.data(), text.size());
        return ok(c.call(w.take(), 20000), Rwrite) ? std::string() : error;
    }
    // Closes the file: "" or the error (files like /notes and /patch apply a write here).
    std::string close() {
        using namespace lmw::ninep;
        return ok(c.call(w.start(Tclunk, tag++).u32(1).take(), 20000), Rclunk) ? std::string() : error;
    }
    std::string read() {
        using namespace lmw::ninep;
        std::string out;
        for (std::uint64_t offset = 0;;) {
            auto rep = c.call(w.start(Tread, tag++).u32(1).u64(offset).u32(4096).take());
            if (!ok(rep, Rread)) return "error: " + error;
            Reader r(rep.data(), rep.size());
            r.u32(); r.u8(); r.u16();
            const auto n = r.u32();
            if (n == 0) return out;
            out += std::string(r.bytes(n));
            offset += n;
        }
    }
};

// Reads a whole file over 9P while this (message) thread runs the message loop (/patch and /cables
// are read on the message thread); "error: ..." on failure.
std::string read9P(const std::string& socket, std::initializer_list<const char*> path) {
    std::string text;
    std::atomic<bool> done{false};
    std::thread t([&] {
        File9P f;
        text = f.open(socket, path, 0) ? f.read() : "error: " + f.error;
        done = true;
    });
    for (int i = 0; i < 1000 && !done; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    t.join();
    return text;
}

// Writes over 9P while this (message) thread runs the message loop, as a host does; the reply.
std::string write9P(const std::string& socket, const std::vector<std::string>& path, const std::string& text) {
    std::string reply;
    std::atomic<bool> done{false};
    std::thread t([&] {
        File9P f;
        std::vector<const char*> names;
        for (const auto& p : path) names.push_back(p.c_str());
        bool opened = false;
        if (names.size() == 1) opened = f.open(socket, {names[0]}, 1);
        else if (names.size() == 2) opened = f.open(socket, {names[0], names[1]}, 1);
        else if (names.size() == 3) opened = f.open(socket, {names[0], names[1], names[2]}, 1);
        reply = opened ? f.write(text) : "open: " + f.error;
        if (opened && reply.empty()) reply = f.close();
        done = true;
    });
    for (int i = 0; i < 1000 && !done; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    t.join();
    return reply;
}

// Reads params/vcf1/cutoff over 9P (a read needs no message thread).
double cutoffOver9P(const std::string& socket) {
    File9P f;
    if (!f.open(socket, {"params", "vcf1", "cutoff"}, 0)) return -1;
    return std::atof(f.read().c_str());
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) { std::printf("usage: LMWHostTest <LMW-Synth.vst3>\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::String path = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getFullPathName();

    // 1. Scan: one instrument type named LMW-Synth.
    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> types;
    vst3.findAllTypesForFile(types, path);
    CHECK(types.size() == 1);
    if (types.isEmpty()) { std::printf("FAIL: no plugin found in %s\n", path.toRawUTF8()); return 1; }
    const auto& desc = *types[0];
    CHECK(desc.name == "LMW-Synth");
    CHECK(desc.isInstrument);
    std::printf("found: %s %s by %s (%s)\n", desc.name.toRawUTF8(), desc.version.toRawUTF8(),
                desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8());

    juce::String error;
    auto a = vst3.createInstanceFromDescription(desc, sampleRate, blockSize, error);
    CHECK(a != nullptr);
    if (a == nullptr) { std::printf("FAIL: %s\n", error.toRawUTF8()); return 1; }
    // The scan reads class info without loading the plugin; the channels come from the instance.
    std::printf("channels: %d in, %d out\n", a->getTotalNumInputChannels(), a->getTotalNumOutputChannels());
    CHECK(a->getTotalNumInputChannels() == 0 && a->getTotalNumOutputChannels() == 2);
    a->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    a->prepareToPlay(sampleRate, blockSize);
    // Ours, plus JUCE's hidden VST3 "MIDI CC" parameters (16 channels x 130: how VST3 delivers mod
    // wheel, sustain and pitch bend to a plugin).
    std::printf("parameters: %d\n", a->getParameters().size());
    CHECK(a->getParameters().size() >= 39);
    CHECK(findParameter(*a, "VCF 2 Cutoff") != nullptr && findParameter(*a, "Volume") != nullptr);

    // 2. Play: a C major chord sounds (finite, not clipping), and is silent again after release.
    {
        const auto x = render(*a, 160, 60);
        bool finite = true;
        float peak = 0;
        for (float v : x) { finite = finite && std::isfinite(v); peak = (std::max)(peak, std::fabs(v)); }
        const double held = rms(x, 10 * blockSize, 50 * blockSize), tail = rms(x, 140 * blockSize, 160 * blockSize);
        std::printf("held rms %.4f, peak %.3f, tail rms %.6f\n", held, peak, tail);
        CHECK(finite);
        CHECK(held > 0.01);
        CHECK(peak < 1.0f);
        CHECK(tail < 1e-4);
    }

    // 3. State: change parameters, save, restore into a second instance: same values, same sound.
    auto* cutoff = findParameter(*a, "VCF 1 Cutoff");
    auto* wave = findParameter(*a, "VCO 1 Wave");
    auto* res = findParameter(*a, "VCF 1 Resonance");
    CHECK(cutoff != nullptr && wave != nullptr && res != nullptr);
    if (cutoff != nullptr) cutoff->setValueNotifyingHost(0.35f);
    if (wave != nullptr) wave->setValueNotifyingHost(wave->getValueForText("Square"));
    if (res != nullptr) res->setValueNotifyingHost(0.6f);
    juce::MemoryBlock state;
    a->getStateInformation(state);
    CHECK(state.getSize() > 100);

    auto b = vst3.createInstanceFromDescription(desc, sampleRate, blockSize, error);
    CHECK(b != nullptr);
    if (b == nullptr) return 1;
    b->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    b->prepareToPlay(sampleRate, blockSize);
    b->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto va = parameterValues(*a), vb = parameterValues(*b);
    int differing = 0;
    for (const auto& [name, value] : va)
        if (std::fabs(vb.at(name) - value) > 1e-6f) { ++differing; std::printf("  differs: %s %f vs %f\n", name.toRawUTF8(), value, vb.at(name)); }
    CHECK(differing == 0);
    CHECK(wave == nullptr || wave->getCurrentValueAsText() == "Square");

    // Both instances render the same samples from the same MIDI (fresh voices in both).
    a->releaseResources(); a->prepareToPlay(sampleRate, blockSize);
    b->releaseResources(); b->prepareToPlay(sampleRate, blockSize);
    const auto ya = render(*a, 80, 40), yb = render(*b, 80, 40);
    double maxDiff = 0;
    for (std::size_t i = 0; i < ya.size(); ++i) maxDiff = (std::max)(maxDiff, std::fabs(double(ya[i]) - yb[i]));
    std::printf("restored instance: max sample difference %.3g, rms %.4f\n", maxDiff, rms(ya, 0, ya.size()));
    CHECK(maxDiff < 1e-6);
    CHECK(rms(ya, 0, ya.size()) > 0.005);

    // 3b. Reverb (Spec S7.4): the same note dry and with Mix 40 %: the reverb leaves a tail after the
    //     synth's own release has ended, and is stereo; back at Mix 0 the output is the dry synth again.
    {
        auto* mix = findParameter(*a, "Reverb Mix");
        CHECK(mix != nullptr);
        // One note, released at block 20; left and right channel.
        auto play = [&](std::vector<float>& left, std::vector<float>& right) {
            a->releaseResources(); a->prepareToPlay(sampleRate, blockSize);
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int blk = 0; blk < 120; ++blk) {
                juce::MidiBuffer midi;
                if (blk == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
                if (blk == 20) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
                buffer.clear();
                a->processBlock(buffer, midi);
                for (int i = 0; i < blockSize; ++i) { left.push_back(buffer.getSample(0, i)); right.push_back(buffer.getSample(1, i)); }
            }
        };
        if (mix != nullptr) {
            std::vector<float> dryL, dryR, wetL, wetR;
            play(dryL, dryR);
            mix->setValueNotifyingHost(0.4f);
            play(wetL, wetR);
            const std::size_t from = 60 * blockSize, to = 100 * blockSize;      // 0.43-0.85 s after release
            const double dryTail = rms(dryL, from, to), wetTail = rms(wetL, from, to), held = rms(dryL, 5 * blockSize, 20 * blockSize);
            double side = 0;
            for (std::size_t i = 0; i < wetL.size(); ++i) side += std::fabs(double(wetL[i]) - wetR[i]);
            side /= double(wetL.size());
            std::printf("reverb mix 40 %%: held %.5f; 0.43-0.85 s after release: dry %.7f, with reverb %.7f; left/right differ by %.6f\n",
                        held, dryTail, wetTail, side);
            CHECK(dryL == dryR);                                 // the synth alone is mono
            CHECK(wetTail > 0.01 * held && wetTail > 100.0 * dryTail);
            CHECK(side > 1e-5);                                  // Width 100 %: stereo
            CHECK(a->getTailLengthSeconds() > 0.0);
            mix->setValueNotifyingHost(0.0f);
            std::vector<float> againL, againR;
            play(againL, againR);
            CHECK(againL == dryL);                               // Mix 0: exactly the dry synth
            CHECK(a->getTailLengthSeconds() == 0.0);
        }
    }

    // 4. Two instances in one process: two distinct 9P sockets (S10.5).
    const int sockets = countOurSockets();
    std::printf("9P sockets of this process: %d\n", sockets);
    CHECK(sockets == 2);

    // 4b. Spec S10.12: a 9P change waits for the message thread, here this main thread. While it runs no
    //     message loop, a write answers "busy" after 5 s; the queued change is applied once the loop runs.
    //     With the loop running, a write is applied at once.
    if (const auto socks = ourSockets(); !socks.empty()) {
        // Instance a's socket (created first: <pid>-1.sock), so that sound checks render the instance written to.
        std::string sock = socks.front();
        for (const auto& candidate : socks)
            if (candidate.size() > 7 && candidate.compare(candidate.size() - 7, 7, "-1.sock") == 0) sock = candidate;
        CHECK(sock.size() > 7 && sock.compare(sock.size() - 7, 7, "-1.sock") == 0);
        using clock = std::chrono::steady_clock;
        auto writeCutoff = [&](const char* value, std::string& reply, double& seconds, std::atomic<bool>& done) {
            return std::thread([&reply, &seconds, &done, sock, value] {
                const auto t0 = clock::now();
                File9P f;
                reply = f.open(sock, {"params", "vcf1", "cutoff"}, 1) ? f.write(value) : "open: " + f.error;
                seconds = std::chrono::duration<double>(clock::now() - t0).count();
                done = true;
            });
        };
        const double before = cutoffOver9P(sock);
        std::string reply;
        double seconds = 0;
        std::atomic<bool> done{false};
        writeCutoff("1234", reply, seconds, done).join();              // message loop not running
        std::printf("9P write with the message thread blocked: \"%s\" after %.2f s (cutoff stays %g)\n",
                    reply.c_str(), seconds, cutoffOver9P(sock));
        CHECK(reply == "busy");
        CHECK(seconds >= 4.9 && seconds < 10.0);
        CHECK(std::fabs(cutoffOver9P(sock) - before) < 1e-9);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
        std::printf("after the message loop ran: cutoff %g (the late change)\n", cutoffOver9P(sock));
        CHECK(std::fabs(cutoffOver9P(sock) - 1234.0) < 0.5);

        done = false;
        auto t = writeCutoff("2000", reply, seconds, done);
        for (int i = 0; i < 500 && !done; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        t.join();
        std::printf("9P write with the message loop running: \"%s\" after %.3f s, cutoff %g\n",
                    reply.c_str(), seconds, cutoffOver9P(sock));
        CHECK(reply.empty());
        CHECK(seconds < 2.0);
        CHECK(std::fabs(cutoffOver9P(sock) - 2000.0) < 0.5);

        // 4c. /info (Spec S10.10): one JSON object per parameter, e.g. the cutoff exactly as Kukel expects.
        // /version (Spec S6.10): "<version> <commit>\n", the version from CMakeLists.txt.
        const auto version = read9P(sock, {"version"});
        std::printf("/version: %s", version.c_str());
        const auto versionNumber = version.substr(0, version.find(' '));
        CHECK(!versionNumber.empty() && std::isdigit(static_cast<unsigned char>(versionNumber[0])) != 0
              && std::count(versionNumber.begin(), versionNumber.end(), '.') == 2 && versionNumber.size() < version.size() - 2
              && version.back() == '\n');
        const auto info = read9P(sock, {"info"});
        int objects = 0;
        for (std::size_t at = 0; (at = info.find("{\"id\": ", at)) != std::string::npos; ++at) ++objects;
        std::printf("/info: %zu bytes, %d parameters\n", info.size(), objects);
        CHECK(info.rfind("[\n", 0) == 0 && info.size() > 2 && info.substr(info.size() - 2) == "]\n");
        CHECK(objects == 56);
        CHECK(info.find("{\"id\": \"vcf1.cutoff\", \"name\": \"VCF 1 Cutoff\", \"kind\": \"float\", \"min\": 20.0, "
                        "\"max\": 18000.0, \"default\": 18000.0, \"unit\": \"Hz\", \"centre\": 600.0}") != std::string::npos);
        CHECK(info.find("{\"id\": \"vco1.tune\", \"name\": \"VCO 1 Tune\", \"kind\": \"float\", \"min\": -7.0, \"max\": 7.0, "
                        "\"default\": 0.0, \"unit\": \"st\", \"step\": 0.01}") != std::string::npos);
        CHECK(info.find("{\"id\": \"voice.count\", \"name\": \"Voices\", \"kind\": \"int\", \"min\": 1, \"max\": 32, \"default\": 8}") != std::string::npos);
        CHECK(info.find("\"choices\": [\"24 dB\", \"12 dB\", \"6 dB\"], \"default\": \"24 dB\"}") != std::string::npos);
        CHECK(info.find("{\"id\": \"out.gain\", \"name\": \"Volume\", \"kind\": \"float\", \"min\": 0.0, \"max\": 1.0, "
                        "\"default\": 0.8, \"unit\": \"dB\"}") != std::string::npos);

        // 4d. Exact round trip (Kukel request 2026-11-17): a value written within the range (on the step)
        //     reads back as written, for every float parameter: its ends, its default, round values and values
        //     with many digits that store the same float as a nearby text (Kukel 2026-11-18: 1234.5678 read
        //     back as 1234.5673, 20.000001 as 20.000002).
        int written = 0, exact = 0;
        for (std::size_t at = 0; (at = info.find("\"kind\": \"float\"", at)) != std::string::npos; ++at) {
            const auto open = info.rfind("{\"id\": \"", at) + 8;
            const auto id = info.substr(open, info.find('"', open) - open);
            auto number = [&](const char* key) {
                const auto k = info.find(std::string("\"") + key + "\": ", at);
                return std::atof(info.c_str() + k + std::strlen(key) + 4);
            };
            const double lo = number("min"), hi = number("max");
            const auto end = info.find('}', at);
            const auto stepAt = info.find("\"step\": ", at);
            const double step = stepAt < end ? number("step") : 0.0;
            const auto dot = id.find('.');
            const std::vector<std::string> path{"params", id.substr(0, dot), id.substr(dot + 1)};
            std::vector<std::string> values{"0", "1", "800", "0.5", "0.37", "0.123", "2.5", "1234.5", "-3.27", "0.01",
                                            "1234.5678", "20.000001", "0.33333333", "4.0000001"};
            char buf[32];
            for (double v : {lo, hi, lo + 0.3 * (hi - lo), lo + 0.71 * (hi - lo)}) {
                std::snprintf(buf, sizeof buf, "%.2f", v);                // on the 0.01 step of the tunes
                values.push_back(buf);
            }
            for (auto text : values) {
                const double v = std::atof(text.c_str());
                if (v < lo || v > hi) continue;
                if (step > 0 && std::fabs(v / step - std::round(v / step)) > 1e-6) continue;   // off the step
                while (text.find('.') != std::string::npos && (text.back() == '0' || text.back() == '.')) {
                    const bool wasDot = text.back() == '.';
                    text.pop_back();
                    if (wasDot) break;
                }
                if (text == "-0") text = "0";
                const auto reply = write9P(sock, path, text);
                const auto back = read9P(sock, {path[0].c_str(), path[1].c_str(), path[2].c_str()});
                ++written;
                if (reply.empty() && back == text + "\n") ++exact;
                else std::printf("  %s: wrote %s (%s), read back %s", id.c_str(), text.c_str(), reply.c_str(), back.c_str());
            }
        }
        std::printf("exact round trip: %d of %d float writes\n", exact, written);
        CHECK(written > 200 && exact == written);
        // The written text is kept only while it stores the current value; /patch shows it too (S7.1).
        CHECK(write9P(sock, {"params", "vcf1", "cutoff"}, "1234.5678").empty());
        CHECK(read9P(sock, {"patch"}).find("\nvcf1.cutoff = 1234.5678") != std::string::npos);
        CHECK(write9P(sock, {"params", "vcf1", "cutoff"}, "99999").empty());              // clipped
        CHECK(read9P(sock, {"params", "vcf1", "cutoff"}) == "18000\n");
        CHECK(write9P(sock, {"patch"}, "format = 1\n[parameters]\nvcf1.cutoff = 987.65432\n").empty());
        CHECK(read9P(sock, {"params", "vcf1", "cutoff"}) == "987.65432\n");

        // 4f. Module presets (Spec S7.5): every value of every built-in preset is valid for the real parameter:
        //     written over 9P (as the panel menu sets it) it is accepted and reads back as written.
        int presetValues = 0, presetExact = 0;
        for (const char* module : {"env2", "vcf2", "vco3", "reverb", "formant"}) {
            const auto kind = *lmw::presets::kindOf(module);
            for (const auto& preset : lmw::presets::builtIn(kind))
                for (const auto& [key, value] : preset.values) {
                    const auto presetReply = write9P(sock, {"params", module, key}, value);
                    const auto back = read9P(sock, {"params", module, key.c_str()});
                    ++presetValues;
                    if (presetReply.empty() && back == value + "\n") ++presetExact;
                    else std::printf("  preset %s %s: %s = %s (%s), read back %s", module, preset.name.c_str(),
                                     key.c_str(), value.c_str(), presetReply.c_str(), back.c_str());
                }
        }
        std::printf("module presets: %d of %d values accepted and read back\n", presetExact, presetValues);
        CHECK(presetValues > 200 && presetExact == presetValues);

        // 4e. /serial goes up with every change (parameters, cables, notes); /keys plays notes (Spec S10.10).
        auto serial = [&] { return std::strtoull(read9P(sock, {"serial"}).c_str(), nullptr, 10); };
        const auto s0 = serial();
        CHECK(read9P(sock, {"serial"}).back() == '\n');
        CHECK(write9P(sock, {"params", "vcf1", "res"}, "0.25").empty());
        const auto s1 = serial();
        CHECK(write9P(sock, {"cables"}, "vcf1.cutoff=lfo2.out@0.5").empty());
        const auto s2 = serial();
        CHECK(write9P(sock, {"cables"}, "vcf1.cutoff=").empty());
        CHECK(write9P(sock, {"notes"}, "serial test").empty());
        const auto s3 = serial();
        CHECK(serial() == s3);                                        // nothing changed: the same number
        std::printf("/serial: %llu -> %llu (parameter) -> %llu (cable) -> %llu (cable, notes)\n",
                    static_cast<unsigned long long>(s0), static_cast<unsigned long long>(s1),
                    static_cast<unsigned long long>(s2), static_cast<unsigned long long>(s3));
        CHECK(s1 > s0 && s2 > s1 && s3 > s2);

        for (const char* bad : {"on 200", "on 60 128", "press 60", "off", "on 60 100 7", "off 60 100", "on -1"})
            CHECK(write9P(sock, {"keys"}, bad) == std::string("bad key line: ") + bad);
        // Defaults again after 4d: a patch without parameters sets every one to its default (S7.1).
        CHECK(write9P(sock, {"patch"}, "format = 1\n[parameters]\n").empty());
        a->releaseResources(); a->prepareToPlay(sampleRate, blockSize);
        auto level = [&](int blocks) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            double sum = 0;
            for (int blk = 0; blk < blocks; ++blk) {
                juce::MidiBuffer midi;
                buffer.clear();
                a->processBlock(buffer, midi);
                for (int i = 0; i < blockSize; ++i) sum += double(buffer.getSample(0, i)) * buffer.getSample(0, i);
            }
            return std::sqrt(sum / (blocks * blockSize));
        };
        const double silent = level(20);
        CHECK(write9P(sock, {"keys"}, "on 60 100\non 64\n").empty());
        const double playing = level(40);
        CHECK(write9P(sock, {"keys"}, "off 60\noff 64 \n").empty());
        level(100);                                                   // release (Env 1 release 0.3 s)
        const double released = level(20);
        std::printf("/keys: rms %.6f before, %.4f with on 60 100 + on 64, %.6f after off\n", silent, playing, released);
        CHECK(silent < 1e-6 && playing > 0.01 && released < 1e-4);
    }

    // 5. Editor, only with a display (CI runners on Linux have none).
    if (juce::Desktop::getInstance().getDisplays().displays.size() > 0 && a->hasEditor()) {
        std::unique_ptr<juce::AudioProcessorEditor> editor(a->createEditorAndMakeActive());
        CHECK(editor != nullptr);
        if (editor != nullptr) {
            std::printf("editor: %d x %d\n", editor->getWidth(), editor->getHeight());
            CHECK(editor->getWidth() > 600 && editor->getHeight() > 400);
        }
    } else {
        std::printf("editor: skipped (no display)\n");
    }

    a->releaseResources();
    b->releaseResources();
    a.reset();
    b.reset();
    CHECK(countOurSockets() == 0);                    // sockets removed with their instances

    std::printf(failures == 0 ? "T-S9.2-host: all passed\n" : "T-S9.2-host: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
