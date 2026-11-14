// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S9.2-host: loads the built LMW-Synth.vst3 through JUCE's VST3 hosting (as a DAW would) and
// checks scan, playing, state save/restore into a second instance, the reverb (S7.4), two instances with distinct 9P
// endpoints (S10.5), 9P changes answering "busy" while the message thread is blocked (S10.12), and - when a
// display exists - the editor. Usage: LMWHostTest <path to .vst3>
#include "ninep/Transport.h"
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
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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
    std::string read() {
        using namespace lmw::ninep;
        auto rep = c.call(w.start(Tread, tag++).u32(1).u64(0).u32(4096).take());
        if (!ok(rep, Rread)) return "error: " + error;
        Reader r(rep.data(), rep.size());
        r.u32(); r.u8(); r.u16();
        return std::string(r.bytes(r.u32()));
    }
};

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
        const std::string sock = socks.front();
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
