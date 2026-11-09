// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S9.2-host: loads the built LMW-Synth.vst3 through JUCE's VST3 hosting (as a DAW would) and
// checks scan, playing, state save/restore into a second instance, two instances with distinct 9P
// endpoints (S10.5), and - when a display exists - the editor. Usage: LMWHostTest <path to .vst3>
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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>

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

int countOurSockets() {
    namespace fs = std::filesystem;
#if JUCE_WINDOWS
    const char* base = std::getenv("TEMP");
    const fs::path dir = fs::path(base != nullptr ? base : "C:\\Windows\\Temp") / "lmw-synth";
#elif JUCE_MAC
    const char* base = std::getenv("TMPDIR");
    const fs::path dir = fs::path(base != nullptr ? base : "/tmp") / "lmw-synth";
#else
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    const fs::path dir = runtime != nullptr && *runtime != '\0' ? fs::path(runtime) / "lmw-synth"
                                                                : fs::path("/tmp") / ("lmw-synth-" + std::to_string(getuid()));
#endif
#if JUCE_WINDOWS
    const auto pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto pid = static_cast<long>(getpid());
#endif
    const auto prefix = std::to_string(pid) + "-";    // NinePService: <pid>-<n>.sock
    int n = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind(prefix, 0) == 0) ++n;
    return n;
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

    // 4. Two instances in one process: two distinct 9P sockets (S10.5).
    const int sockets = countOurSockets();
    std::printf("9P sockets of this process: %d\n", sockets);
    CHECK(sockets == 2);

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
