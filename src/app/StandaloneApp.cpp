// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone application (Spec S6.2): JUCE's standard standalone app, except that the LMW-Synth
// look-and-feel is installed as the default *before* the window is created, so the title bar, its
// window buttons, the Options menu and the audio/MIDI settings dialog are black on white like the
// main panel. Enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 (CMakeLists.txt).
#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone
#include <memory>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginEditor.h"
#include "ninep/Transport.h"
#include <atomic>
#if !JUCE_WINDOWS
 #include <csignal>
#endif

namespace {

// SIGTERM, SIGINT, SIGHUP: remove the 9P socket files at once (a client sees no dead socket), then quit
// normally on the message thread (state saved). A second signal before that ends the process.
std::atomic<bool> quitSignalled{false};
#if !JUCE_WINDOWS
extern "C" void onQuitSignal(int sig) {
    lmw::ninep::removeOwnSocketFilesNow();
    if (quitSignalled.exchange(true)) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
}
#endif

// Spec S6.11: JUCE's standalone window, whose Options menu (top bar) also holds the editor's menu items
// (About, Contrast, ...) after JUCE's own four.
class LMWStandaloneWindow final : public juce::StandaloneFilterWindow {
public:
    using juce::StandaloneFilterWindow::StandaloneFilterWindow;

    void buttonClicked(juce::Button* button) override {
        juce::PopupMenu m;
        m.addItem(1, TRANS("Audio/MIDI Settings..."));
        m.addSeparator();
        m.addItem(2, TRANS("Save current state..."));
        m.addItem(3, TRANS("Load a saved state..."));
        m.addSeparator();
        m.addItem(4, TRANS("Reset to default state"));
        if (auto* e = editor()) { m.addSeparator(); e->addMenuItems(m); }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(button),
                        [safe = juce::Component::SafePointer<LMWStandaloneWindow>(this)](int result) {
                            if (safe == nullptr || result <= 0) return;
                            if (result < LMWSynthEditor::menuFirstId) safe->handleMenuResult(result);
                            else if (auto* e = safe->editor()) e->handleMenuItem(result);
                        });
    }

private:
    LMWSynthEditor* editor() {
        auto* holder = getPluginHolder();
        return holder != nullptr && holder->processor != nullptr
                   ? dynamic_cast<LMWSynthEditor*>(holder->processor->getActiveEditor()) : nullptr;
    }
};

class LMWStandaloneApp final : public juce::JUCEApplication {
public:
    LMWStandaloneApp() {
        juce::PropertiesFile::Options options;   // same settings file as JUCE's default app
        options.applicationName     = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override           { return juce::CharPointer_UTF8(JucePlugin_Name); }
    const juce::String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                 { return true; }
    void anotherInstanceStarted(const juce::String&) override  {}

    void initialise(const juce::String&) override {
       #if !JUCE_WINDOWS
        for (int sig : {SIGTERM, SIGINT, SIGHUP}) std::signal(sig, onQuitSignal);
       #endif
        quitPoll.start();
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);   // before any window exists
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) {
            pluginHolder = createPluginHolder();                // no display: run headless
            return;
        }
        mainWindow = std::make_unique<LMWStandaloneWindow>(
            getApplicationName(), lookAndFeel.findColour(juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
        mainWindow->setVisible(true);
    }

    void shutdown() override {
        quitPoll.stopTimer();
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);       // the windows using it are gone
    }

    void systemRequestedQuit() override {
        if (pluginHolder != nullptr) pluginHolder->savePluginState();
        if (mainWindow != nullptr) mainWindow->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100, [] {
                if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
            });
        } else {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder() {
        return std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(), false,
                                                              juce::String{}, nullptr,
                                                              juce::Array<juce::StandalonePluginHolder::PluginInOuts>{},
                                                              false);
    }

    struct QuitPoll final : juce::Timer {       // turns a quit signal into a normal quit
        void start() { startTimer(100); }
        void timerCallback() override {
            if (!quitSignalled.load()) return;
            stopTimer();
            if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
        }
    };

    QuitPoll quitPoll;
    LMWLookAndFeel lookAndFeel;                 // declared first: destroyed after the windows
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace

START_JUCE_APPLICATION(LMWStandaloneApp)
#endif
