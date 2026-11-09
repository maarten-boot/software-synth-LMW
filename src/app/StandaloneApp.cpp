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

namespace {

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
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);   // before any window exists
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) {
            pluginHolder = createPluginHolder();                // no display: run headless
            return;
        }
        mainWindow = std::make_unique<juce::StandaloneFilterWindow>(
            getApplicationName(), lookAndFeel.findColour(juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
        mainWindow->setVisible(true);
    }

    void shutdown() override {
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

    LMWLookAndFeel lookAndFeel;                 // declared first: destroyed after the windows
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace

START_JUCE_APPLICATION(LMWStandaloneApp)
#endif
