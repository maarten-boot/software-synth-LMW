// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone application (Spec S6.2): JUCE's standard standalone app, except that the LMW-Synth
// look-and-feel is installed as the default *before* the window is created, so the title bar, its
// window buttons, the Options menu and the audio/MIDI settings dialog are black on white like the
// main panel. Enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 (CMakeLists.txt).
#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone
#include <memory>
#include <optional>
#include <vector>
#if JUCE_LINUX
 #include <dlfcn.h>
#endif
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

    // Spec S6.16 (user 2026-11-24): the window fills the work area of the screen it was last on (no task bar /
// dock); the panels keep their size and the free space takes moved panels. Never below the editor's minimum
// (the window's own limits come from the editor through JUCE's decorator constrainer).
#if JUCE_LINUX
// Spec S6.16 (2026-11-26, user: "the keyboard is half hidden on my desktop (xfce) with the bottom menubar"): JUCE
// takes the whole monitor as the work area on Linux ("JUCE currently does not support requesting the user area on
// Linux", juce_XWindowSystem_linux.cpp), so a panel or task bar was ignored. The desktop's work area is in the root
// window's _NET_WORKAREA (x, y, w, h per desktop, without the panels; set by XFCE, GNOME, KDE ...), read here for the
// current desktop. libX11 is opened at run time as JUCE does; its own types are spelled out (no Xlib.h macros).
std::optional<juce::Rectangle<int>> linuxWorkArea() {
    using XDisplay = void;
    using XAtom = unsigned long;
    using XWindow = unsigned long;
    static void* const lib = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (lib == nullptr) return std::nullopt;
    const auto open = reinterpret_cast<XDisplay* (*)(const char*)>(dlsym(lib, "XOpenDisplay"));
    const auto close = reinterpret_cast<int (*)(XDisplay*)>(dlsym(lib, "XCloseDisplay"));
    const auto intern = reinterpret_cast<XAtom (*)(XDisplay*, const char*, int)>(dlsym(lib, "XInternAtom"));
    const auto root = reinterpret_cast<XWindow (*)(XDisplay*)>(dlsym(lib, "XDefaultRootWindow"));
    const auto getProp = reinterpret_cast<int (*)(XDisplay*, XWindow, XAtom, long, long, int, XAtom, XAtom*, int*, unsigned long*,
                                                  unsigned long*, unsigned char**)>(dlsym(lib, "XGetWindowProperty"));
    const auto xfree = reinterpret_cast<int (*)(void*)>(dlsym(lib, "XFree"));
    if (open == nullptr || close == nullptr || intern == nullptr || root == nullptr || getProp == nullptr || xfree == nullptr)
        return std::nullopt;
    XDisplay* d = open(nullptr);
    if (d == nullptr) return std::nullopt;
    constexpr XAtom cardinal = 6;                        // XA_CARDINAL
    const auto readLongs = [&](const char* name, long offset, long count, std::vector<long>& out) {
        const XAtom atom = intern(d, name, 1);           // only if it exists
        if (atom == 0) return false;
        XAtom type = 0; int format = 0; unsigned long n = 0, after = 0; unsigned char* data = nullptr;
        if (getProp(d, root(d), atom, offset, count, 0, cardinal, &type, &format, &n, &after, &data) != 0 || data == nullptr) return false;
        if (format == 32) out.assign(reinterpret_cast<long*>(data), reinterpret_cast<long*>(data) + n);   // 32-bit items come as longs
        xfree(data);
        return format == 32 && static_cast<long>(n) == count;
    };
    std::vector<long> desktop, area;
    const long current = readLongs("_NET_CURRENT_DESKTOP", 0, 1, desktop) ? desktop[0] : 0;
    std::optional<juce::Rectangle<int>> result;
    if (readLongs("_NET_WORKAREA", current * 4, 4, area) || readLongs("_NET_WORKAREA", 0, 4, area))
        if (area[2] > 0 && area[3] > 0)
            result = juce::Rectangle<int>(static_cast<int>(area[0]), static_cast<int>(area[1]), static_cast<int>(area[2]), static_cast<int>(area[3]));
    close(d);
    return result;
}
#endif

// The work area of a display: JUCE's user area, on Linux narrowed to the desktop's work area (see above).
juce::Rectangle<int> workArea(const juce::Displays::Display& display) {
    auto area = display.userBounds.toNearestInt();
   #if JUCE_LINUX
    if (const auto wa = linuxWorkArea()) {
        const auto scaled = (wa->toFloat() / static_cast<float>(display.scale)).toNearestInt();   // physical -> logical
        if (const auto inside = area.getIntersection(scaled); !inside.isEmpty()) area = inside;
    }
   #endif
    return area;
}

void fillScreen(juce::StandaloneFilterWindow& w) {
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(w.getScreenBounds());
    auto* editor = w.getContentComponent() != nullptr ? w.getContentComponent()->getChildComponent(0) : nullptr;
    if (display == nullptr) return;
    auto area = workArea(*display);
    if (auto* ed = dynamic_cast<juce::AudioProcessorEditor*>(editor); ed != nullptr && ed->getConstrainer() != nullptr) {
        const int extraW = w.getWidth() - ed->getWidth(), extraH = w.getHeight() - ed->getHeight();
        const int minW = ed->getConstrainer()->getMinimumWidth() + extraW, minH = ed->getConstrainer()->getMinimumHeight() + extraH;
        area.setSize(std::max(area.getWidth(), minW), std::max(area.getHeight(), minH));
    }
    w.setBounds(area);
}

// Spec S6.11: JUCE's standalone window, whose Options menu (top bar) also holds the editor's menu items
// (About, Contrast, ...) after JUCE's own four.
class LMWStandaloneWindow final : public juce::StandaloneFilterWindow {
public:
    using juce::StandaloneFilterWindow::StandaloneFilterWindow;

    // Spec S6.16 (user 2026-11-24): the maximise button fills the screen's work area, or goes back to the size before.
    void maximiseButtonPressed() override {
        const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
        if (display != nullptr && getBounds() == workArea(*display) && !restoreBounds.isEmpty()) {
            setBounds(restoreBounds);
            return;
        }
        restoreBounds = getBounds();
        fillScreen(*this);
    }

    void buttonClicked(juce::Button* button) override {
        juce::PopupMenu m;
        addWindowItems(m);
        if (auto* e = editor()) { m.addSeparator(); e->addMenuItems(m); }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(button),
                        [safe = juce::Component::SafePointer<LMWStandaloneWindow>(this)](int result) {
                            if (safe == nullptr || result <= 0) return;
                            if (result < LMWSynthEditor::menuFirstId) safe->windowItem(result);
                            else if (auto* e = safe->editor()) e->handleMenuItem(result);
                        });
    }

    // Spec S6.16 (user 2026-11-26: resizing left grey lines, froze, and went on after the mouse was let go): on Linux
    // the window manager's own frame — it resizes, maximises (to its work area) and stops on release. JUCE's title
    // bar resizes by itself, one window resize and full repaint per mouse move, which piled up. No Options button
    // then: its items head the editor's Menu.
    void useNativeFrame() {
        setUsingNativeTitleBar(true);
        wireEditor();
    }

    void resized() override {
        juce::StandaloneFilterWindow::resized();
        if (isUsingNativeTitleBar())                        // JUCE's Options button lives in its own title bar
            for (auto* c : getChildren())
                if (c->getName() == "Options" && dynamic_cast<juce::TextButton*>(c) != nullptr) c->setVisible(false);
    }

private:
    juce::Rectangle<int> restoreBounds;

    static void addWindowItems(juce::PopupMenu& m) {
        m.addItem(1, TRANS("Audio/MIDI Settings..."));
        m.addSeparator();
        m.addItem(2, TRANS("Save current state..."));
        m.addItem(3, TRANS("Load a saved state..."));
        m.addSeparator();
        m.addItem(4, TRANS("Reset to default state"));
    }

    void windowItem(int result) {
        handleMenuResult(result);
        if (result != 4) return;
        // Reset to default state makes a new editor at its minimum size: fill the screen again (S6.16; found
        // 2026-11-24, the window shrank), and hand the new editor the window's items.
        if (isUsingNativeTitleBar()) { wireEditor(); maximise(); }
        else fillScreen(*this);
    }

    void wireEditor() {
        auto* e = editor();
        if (e == nullptr || !isUsingNativeTitleBar()) return;
        e->addWindowItems = &LMWStandaloneWindow::addWindowItems;
        e->windowItemChosen = [safe = juce::Component::SafePointer<LMWStandaloneWindow>(this)](int result) {
            if (safe != nullptr) safe->windowItem(result);
        };
    }

public:
    // The work area first; with a window manager (it publishes _NET_WORKAREA) ask it to maximise, the size to go
    // back to (the frame's restore button) 5 % smaller on each side so the frame fits on screen. Without one (e.g.
    // Xvfb) the work area stays.
    void maximise() {
        fillScreen(*this);
       #if JUCE_LINUX
        if (!isUsingNativeTitleBar() || !linuxWorkArea()) return;
       #else
        if (!isUsingNativeTitleBar()) return;
       #endif
        const auto r = getBounds();
        setBoundsConstrained(r.reduced(r.getWidth() / 20, r.getHeight() / 20));
        juce::Timer::callAfterDelay(300, [safe = juce::Component::SafePointer<LMWStandaloneWindow>(this)] {
            if (safe != nullptr && safe->isUsingNativeTitleBar()) safe->setFullScreen(true);
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
        // Spec S6.16: minimise, maximise (fill the screen / back) and close; JUCE's standalone has no maximise.
        mainWindow->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, false);
       #if JUCE_LINUX || JUCE_BSD
        mainWindow->useNativeFrame();
        mainWindow->setVisible(true);
        mainWindow->maximise();
       #else
        fillScreen(*mainWindow);
        mainWindow->setVisible(true);
       #endif
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
    std::unique_ptr<LMWStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace

START_JUCE_APPLICATION(LMWStandaloneApp)
#endif
