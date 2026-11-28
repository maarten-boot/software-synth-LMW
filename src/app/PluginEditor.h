// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include "PluginProcessor.h"
#include "MatrixView.h"
#include "SchematicView.h"
#include "ModulePresets.h"
#include <map>
#include "ui/RackLayout.h"
#include "ui/Config.h"
#include "ui/Theme.h"
#include "ui/FaderScale.h"

// Spec S6.17: a vertical fader with scale ticks beside its track (drawn by LMWLookAndFeel).
struct ScaleSlider : juce::Slider {
    ScaleSlider() : juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow) {}
    std::vector<lmw::ui::scale::Tick> ticks;
    bool tickLabels = true;              // value texts at the major ticks (normal view)
    bool showTicks = true;               // Spec S6.20: Menu > Show fader scales
};

// Spec S6.2: black-on-white look; every colour pair is 21:1.
class LMWLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LMWLookAndFeel();
    // Spec S6.29: the widget colours from the current theme (lmw::ui::theme::current()); again on every theme change.
    void applyTheme();
    // Spec S6.1: vertical faders, drawn only in black on white (no grey tints).
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override { return 7; }
    // Choice boxes with a narrow (16 px) arrow area, so short names fit in compact view.
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    // Standalone window (Spec S6.2): white title bar with black text and a line below it; window
    // buttons drawn black on white (JUCE's default ones are yellow and red).
    void drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics&, int w, int h, int titleSpaceX,
                                    int titleSpaceW, const juce::Image* icon, bool drawTitleTextOnLeft) override;
    juce::Button* createDocumentWindowButton(int buttonType) override;
    // Disabled buttons: full-contrast text, dashed outline (JUCE dims them to 50 %).
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool isHighlighted, bool isDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
    // File chooser rows: black text (white when selected), outline glyphs (JUCE: grey details, icons).
    void drawFileBrowserRow(juce::Graphics&, int width, int height, const juce::File&, const juce::String& filename,
                            juce::Image* icon, const juce::String& fileSizeDescription,
                            const juce::String& fileTimeDescription, bool isDirectory, bool isItemSelected,
                            int itemIndex, juce::DirectoryContentsDisplayComponent&) override;
    // Menu separators solid black (JUCE draws them at 30 % opacity).
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
};

class LMWSynthEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::FocusChangeListener {
public:
    explicit LMWSynthEditor(LMWSynthProcessor&);
    ~LMWSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // QWERTY notes work whichever control has focus (Spec S3.2): letter keys not used by the
    // focused control reach the editor and are passed to the on-screen keyboard.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    // Spec S6.3: the user config file (also read by the processor for 9P over TCP).
    static juce::File configFile();

private:
    void timerCallback() override;
    static bool isTypingText();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // A fader whose hover text (Spec S6.1, shown after 2 s) is computed when shown, so it always
    // reflects the current patch and mod wheel.
    struct TipSlider : ScaleSlider {
        std::function<juce::String()> tip;
        juce::String getTooltip() override { return tip ? tip() : juce::String(); }
    };
    struct Fader {
        TipSlider slider;
        juce::Label label;
        juce::Label tag;                    // Spec S6.17: lin / ± / log / curve under the name
        juce::String tagText;               // its text (shown while Show fader scales is on, S6.20)
        std::unique_ptr<SliderAttachment> attachment;
    };
    struct Choice {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };
    struct Item {                       // one column in a module group: a fader or one/two choice boxes
        Fader* fader = nullptr;
        Choice* choice = nullptr;
        Choice* choice2 = nullptr;      // stacked below `choice`
        Choice* choice3 = nullptr;      // a third box below, without its label (S5.17: VCF routing)
        bool boxOnTop = false;          // S5.28: the choice box above a short fader (LFO Wave over Shape)
    };
    static Item stacked(Item top, Item bottom) { return {nullptr, top.choice, bottom.choice}; }
    // Spec S5.25: a fader with a choice box (no label) under it in the same column (OUT: Spread and its mode).
    static Item withBox(Item fader, Item box) { return {fader.fader, box.choice}; }
    // Spec S5.28 (choice E a): a choice box with its label, and a short fader under it (LFO: Wave over Shape).
    static Item boxOver(Item box, Item fader) { Item i{fader.fader, box.choice}; i.boxOnTop = true; return i; }
    static Item stacked(Item top, Item middle, Item bottom) { return {nullptr, top.choice, middle.choice, bottom.choice}; }
    struct Group {                      // a framed module panel (Spec S6.1)
        juce::String title;
        int row = 0;                    // row in the default layout (Spec S6.5)
        std::vector<Item> items;
        juce::Rectangle<int> bounds;
        int minColumns = 0;             // wider than its items, to give its sockets room
        int columns() const { return std::max(static_cast<int>(items.size()), minColumns); }
    };
    // Spec S7.5: the "Presets" button at the right of a panel's title bar; it opens the preset menu
    // (built-in, Default, the user's own, Save as preset...). Black text and frame on the title colour.
    // Spec S5.31: a limiter lamp — "limit" and a dot in the text colour, filled while the limiter works (and 0.3 s
    // after), an outline otherwise; the hover text gives the most it took off in the last second. The hover text is
    // not changed while the pointer is on it (a tooltip that changes while shown flickers).
    class LimitLamp final : public juce::Component, public juce::SettableTooltipClient {
    public:
        explicit LimitLamp(juce::String unitName) : unit(std::move(unitName)) { setInterceptsMouseClicks(true, false); }
        void show(bool on, float db) {
            if (on != lit) { lit = on; repaint(); }
            if (!isMouseOverOrDragging())
                setTooltip(unit + " limiter: " + (db > 0.05f ? juce::String::fromUTF8("\xe2\x88\x92") + juce::String(db, 1)
                                                                  + " dB taken off in the last second" : juce::String("not working"))
                           + "\nThe lamp lights while it squashes peaks. If it stays lit, lower the Volume.");
        }
        bool isLit() const noexcept { return lit; }
        void paint(juce::Graphics& g) override;
    private:
        juce::String unit;
        bool lit = false;
    };
    class PresetButton final : public juce::Button {
    public:
        PresetButton(LMWSynthEditor& e, juce::String module, juce::String panelTitle);
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
        void clicked() override;
        int preferredWidth() const;
        juce::String getTooltip() override;     // with the current preset's full name (S7.5)
        juce::String module, panelTitle;
    private:
        LMWSynthEditor& editor;
    };
    // Spec S6.10: the title "LMW-Synth <version>" in bold, as a button that opens the About dialog
    // (underlined on hover; a frame when focused). Its hover text gives the 9P address (S10.5).
    class TitleButton final : public juce::Button {
    public:
        TitleButton() : juce::Button("LMW-Synth") {}
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
        juce::Font font() const { return juce::Font(juce::FontOptions(fontHeight, juce::Font::bold)); }
        float fontHeight = 16.0f;
    };
    // Title bar of a module panel (Spec S6.5): drag it to move the panel; when focused, arrow keys
    // move it; right-click, Enter or the context-menu key opens a move menu. Other keys (QWERTY
    // notes) pass on to the editor.
    class PanelHeader final : public juce::Component, public juce::SettableTooltipClient {
    public:
        PanelHeader(LMWSynthEditor& e, int panelIndex);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        bool keyPressed(const juce::KeyPress&) override;
        void focusGained(FocusChangeType) override { repaint(); }
        void focusLost(FocusChangeType) override { repaint(); }
        void resized() override;
        void setNote(const juce::String& n) { setTooltip(n.isEmpty() ? baseTip : n + "\n" + baseTip); }   // S6.26
    private:
        juce::String baseTip;
        LMWSynthEditor& editor;
        int panel;
        bool dragging = false;
        std::unique_ptr<PresetButton> presets;   // Spec S7.5, for ENV, VCF and VCO panels
    };
    // Spec S7.14: the title of an effect panel (COMB, ENSEMBLE, DELAY, REVERB) on the Effects tab, over the title the
    // tab draws: drag it left / right to move the effect in the chain; when focused, Left / Right move it; right-click,
    // Enter or Shift+F10 opens Move left / Move right / Reset order. Other keys (QWERTY notes) pass on to the editor.
    class EffectHeader final : public juce::Component, public juce::SettableTooltipClient {
    public:
        EffectHeader(LMWSynthEditor& e, int effectIndex);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        bool keyPressed(const juce::KeyPress&) override;
        void focusGained(FocusChangeType) override { repaint(); }
        void focusLost(FocusChangeType) override { repaint(); }
        void setNote(const juce::String& n) { setTooltip(n.isEmpty() ? baseTip : n + "\n" + baseTip); }   // S6.26
    private:
        juce::String baseTip;
        LMWSynthEditor& editor;
        int effect;
        bool dragging = false;
    };
    // Spec S6.6: a patch point (socket). Inputs are white with a black ring (a black centre when a
    // cable is plugged in); outputs are black with a white ring. Drag from a socket to make a cable,
    // drag a plugged input's cable away to move or remove it; Enter, Shift+F10 or right-click opens
    // a menu; Delete removes an input's cable. Other keys (QWERTY notes) pass on to the editor.
    class Jack final : public juce::Component, public juce::TooltipClient {
    public:
        Jack(LMWSynthEditor& e, bool isOutput, int portIndex, juce::String shortName);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        bool keyPressed(const juce::KeyPress&) override;
        void focusGained(FocusChangeType) override;
        void focusLost(FocusChangeType) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        juce::String getTooltip() override;
        juce::Point<float> centreInEditor() const;    // where cables end
        juce::Rectangle<float> socket() const;        // the circle, local coordinates
        const bool output;
        const int port;                               // lmw::dsp::Out or In index
        int column = 0;                               // fader column of its panel it sits under
    private:
        LMWSynthEditor& editor;
        juce::String name;
        bool dragging = false;
    };
    // Draws all cables above the panels; lets every mouse event through to the controls below. Cables are
    // grey at rest, black (1 px thicker, on top) when an end is the socket under the mouse or with keyboard
    // focus (user 2026-11-22).
    class CableLayer final : public juce::Component {
    public:
        explicit CableLayer(LMWSynthEditor& e) : editor(e) { setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics&) override;
    private:
        LMWSynthEditor& editor;
    };
    // Spec S6.21 (Phase 38): the rack of the Synth and Effects tabs — panels, sockets, cables — on a surface that
    // can be larger than the window, shown in rackView (a Viewport). Its paint draws the rack (paintRack);
    // dragging on its empty space pans the view like a hand.
    class RackArea final : public juce::Component {
    public:
        explicit RackArea(LMWSynthEditor& e) : editor(e) { setTitle("Rack"); }
        void paint(juce::Graphics& g) override { editor.paintRack(g); }
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
    private:
        LMWSynthEditor& editor;
        juce::Point<int> viewAtStart;
        bool panning = false;
    };
    std::unique_ptr<RackArea> rackArea;                   // created first: every rack component is its child
    // The rack's view: keys go on to the editor (Home / End there go to both corners; JUCE's Viewport would take
    // Home / End / arrows for up and down only).
    struct RackViewport final : juce::Viewport { bool keyPressed(const juce::KeyPress&) override { return false; } };
    RackViewport rackView;
    RackViewport schematicView;                          // S6.22: the Schematic, centred or scrolling
    void layoutSchematic();
    void schematicLayoutChanged(const juce::String& text, lmw::ui::schematic::Page page);   // merges one page in
    std::array<juce::Point<int>, 2> tabScroll{};          // S6.21: the view position of the Synth / Effects tab
    void paintRack(juce::Graphics&);
    void scrollIntoView(const juce::Component&);           // S6.21: a focused control out of view comes into view
    void autoScrollRack(juce::Point<int> inRack);          // S6.21: near the edge while dragging, pan on
    void globalFocusChanged(juce::Component*) override;
    // Spec S6.24 (Phase 41): playing controls left of the keyboard — the bend wheel (springs back), the mod wheel,
    // Glide (time and Always / Legato) and the bend range. The wheels drive MIDI-like values in the processor (not
    // parameters); Glide, its mode and the range are parameters.
    class Wheel final : public juce::Slider {
    public:
        explicit Wheel(bool springsBack);
        void paint(juce::Graphics&) override;
        void mouseUp(const juce::MouseEvent&) override;
        const bool springBack;
    };
    Wheel bendWheel{true}, modWheelCtl{false};
    juce::Slider glideSlider{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};   // horizontal: the strip is low
    juce::Label bendLabel{{}, "Bend"}, modLabel{{}, "Mod"}, glideLabel{{}, "Glide"}, bendRangeLabel{{}, "Range"};
    juce::ComboBox glideModeBox, bendRangeBox;
    std::unique_ptr<SliderAttachment> glideAttachment;
    std::unique_ptr<ComboAttachment> glideModeAttachment, bendRangeAttachment;
    void layoutPlayStrip(juce::Rectangle<int> area);
    const Jack* hoverJack = nullptr;    // the socket under the mouse (its cables are drawn black)
    struct CableDrag {                  // a cable being drawn or moved with the mouse
        Jack* fixed = nullptr;          // the end that stays where it is
        int detachedInput = -1;         // moving a cable that was plugged into this input...
        int detachedSource = -1;        // ...from this output (Spec S5.14: an input can have several)
        float detachedLevel = 1.0f;
        Jack* target = nullptr;         // compatible socket under the mouse
        juce::Point<float> loose;       // the end following the mouse
    };

    // Where a dragged panel would land; shown as a black bar (drop marker).
    struct DropTarget {
        int row = 0, index = 0;
        bool newRow = false;
        juce::Rectangle<int> marker;
    };

    // Layout sizes; Spec S6.4: normal or compact view.
    struct Metrics {
        int colW, gap, header, rowH, labelH, boxW, boxH, keysH, titleH, jackH;
        float labelFont, titleFont;
    };
    static constexpr Metrics normalMetrics {80, 10, 28, 222, 22, 72, 26, 90, 36, 36, 15.0f, 15.0f};
    static constexpr Metrics compactMetrics{60,  6, 22, 170, 17, 56, 20, 70, 30, 30, 13.0f, 13.0f};

    Item addFader(const char* paramId, const juce::String& text, std::function<juce::String()> tip = {});
    Item addChoice(const char* paramId, const juce::String& text, const juce::StringArray& options,
                   const juce::String& tip = {});
    void layoutItem(const Item&, juce::Rectangle<int> column) const;
    int tagH() const { return m.colW >= 80 ? 11 : 9; }
    int lampWidth() const { return m.colW >= 80 ? 46 : 38; }   // Spec S5.31: "limit" and its dot    // Spec S6.17: the scale tag under a fader's name
    void layoutJacks(const std::vector<Jack*>& js, int columns, juce::Rectangle<int> jackRow) const;
public:
    // Spec S6.11: the menu's items (About, Contrast, ...), also appended to the standalone window's Options
    // menu (StandaloneApp.cpp). Ids from menuFirstId up; handleMenuItem() acts on one.
    static constexpr int menuFirstId = 6000;
    void addMenuItems(juce::PopupMenu& m) const;
    // Spec S6.16 (user 2026-11-26): with the window manager's own frame (Linux) the standalone window has no Options
    // button; it sets these so its items (Audio/MIDI Settings, Save / Load state, Reset; ids below menuFirstId) head
    // Menu. windowItemChosen may delete this editor (Reset).
    std::function<void(juce::PopupMenu&)> addWindowItems;
    std::function<void(int)> windowItemChosen;
    void handleMenuItem(int id);
    // Spec S7.12 (Phase 31), for 9P /preset, /view, /snapshot (message thread). runPreset: an instrument preset
    // name, "Tutorial <n>" or "<module> <preset>" as the Presets menus; error text or empty. viewText: "tab",
    // "size", "compact", "keyboard" lines, then "panel <x> <y> <w> <h> <title>" per panel / Schematic block shown
    // and "area <x> <y> <w> <h> <name>" (keyboard, matrix, schematic, fxmatrix); applyView: tab / size / compact / keyboard
    // lines. snapshotPng: the editor as shown, as PNG bytes.
    std::string runPreset(const std::string& text);
    std::string viewText() const;
    std::string applyView(const std::string& text);
    std::string snapshotPng();
private:
    void showMainMenu();
    void setKeyboardShown(bool shown);                    // Spec S6.12
    void editColours();                                   // Spec S6.13
    void makeThemeCustom();
    void saveThemeAs();
    void applySavedTheme(const juce::File&);
    static juce::File themeFolder();
    static juce::Array<juce::File> themeFiles();
    juce::Component::SafePointer<juce::DialogWindow> colourWindow;
    // Spec S12.6: the Assistant settings and the "How to get a key" lesson; the key, once read.
    juce::Component::SafePointer<juce::DialogWindow> assistantWindow, lessonWindow, changesWindow, termsWindow;
    void showChanges();                                   // Spec S6.11: Change history...
    void showTerms();                                     // Spec S7.10: Terms...
    std::optional<std::string> keyCache;
    std::string assistantKey();
    void showAssistantSettings();
    void showLesson(int number);
    int keyStripH() const { return m.colW >= 80 ? 20 : 17; }   // the strip with the keyboard button
    static juce::Component::SafePointer<juce::Component> focusNow();   // around popup menus (S3.2)
    void restoreFocus(juce::Component::SafePointer<juce::Component> c);
    void setCompact(bool compact);
    // Window size from metrics and row count (Spec S6.16): the minimum the rack needs; the window grows to it,
    // and with `shrink` (the plugin's Compact / keyboard toggles) is set to it.
    void updateSize(bool shrink = false);
    int screenColumns(const Metrics& metrics) const;     // S6.16: columns that fit on the widest screen
    bool standalone() const;
    int defaultMaxColumns = 0;
    juce::Point<int> sentMinimum;                         // S6.16: the minimum the window manager was last told
    std::optional<DropTarget> findDrop(int panel, juce::Point<int> where) const;
    void dragPanel(int panel, juce::Point<int> where);    // editor coordinates
    void dropPanel(int panel, juce::Point<int> where);
    void cancelDrag();
    bool movePanel(int panel, int direction);             // 0 left, 1 right, 2 up, 3 down
    void showPanelMenu(int panel);
    void layoutChanged(int focusPanel);                   // store, re-layout, keep focus
    // Spec S6.6: cables.
    Jack* jackFor(bool output, int port) const;
    Jack* findJack(juce::Point<float> where, const Jack& from) const;   // compatible socket near `where`
    void cableDrag(Jack& from, juce::Point<float> where);
    void cableDrop(Jack& from, juce::Point<float> where);
    void cancelCableDrag();
    // Spec S5.14: adds a cable (pin) at `level`, or sets its level; level 0 removes that cable;
    // output = Patch::none removes all cables of the input. False (and a message) when it does not fit.
    bool connect(int input, int output, float level = 1.0f);
    // Spec S6.14: cable changes made in one event (a drop that moves a cable: remove + add) are one step.
    void cableStep(const juce::String& name, bool adding);
    bool cableStepOpen = false;
    void undoStep();
    void redoStep();
    void removeAllCables();
    // Spec S6.6, S6.7: the cable menu of a socket (Synth tab) or a port (Schematic tab), shown at `screenArea`.
    void showPortMenu(bool output, int port, juce::Rectangle<int> screenArea);
    void typeCableLevel(int input, int output);            // Spec S6.18: Type a level ▸ a connection
    void setCableLevel(int input, int output, float level);   // S6.18: normalled -> cables, then this level
    juce::String describeOutput(lmw::dsp::Out) const;     // "<output> → <inputs it feeds>"
    // Spec S6.3: config file and contrast.
    void saveConfig() const;
    void applyContrast();                                 // palette from settings, redraw
    void askCustomRatio();
    // Spec S7: tabs (Synth, Effects, Matrix, Notes) and patch files.
    void showTab(int tab);                                // 0 = synth, 1 = effects, 2 = matrix, 3 = notes, 4 = schematic, 5 = assistant
    void savePatch();
    void loadPatch();
    void showError(const juce::String& title, const juce::String& message);
    void refreshPatchInfo();                              // name label and notes from the processor
    void updatePatchLabel();                              // "Patch: name", "*" when changed since (S7.7)
    void showInstrumentMenu();                            // Spec S7.7: whole-instrument presets
    void saveInstrumentAs();
    bool applyLesson(int number, juce::String& error);   // Spec S7.9
    static juce::File instrumentFolder();
    static juce::File patchFolder();
    // Spec S7.5: module presets.
    void showPresetMenu(PresetButton&);
    void applyPreset(const juce::String& module, const lmw::presets::Preset&);
    void applyDefaults(const juce::String& module);
    void savePresetAs(const juce::String& module, const juce::String& panelTitle);
    void writePreset(const juce::String& module, const juce::String& name, const juce::File& file);
    static juce::File presetFolder(lmw::presets::Kind);
    // "VCO 2" -> "vco2", "VCF 1" -> "vcf1", "ENV 1 -> VCA" -> "env1", "COMB" -> "comb", "REVERB" -> "reverb"; else empty.
    static juce::String presetModuleOf(const juce::String& panelTitle);
    // Spec S7.5: "Flute", "Flute*" (changed since), "Default", "Default*" for a module; updated by the timer.
    juce::String computePresetLabel(const juce::String& module) const;
    bool moduleMatches(const juce::String& module, const lmw::presets::Preset&) const;
    bool moduleAtDefaults(const juce::String& module) const;
    void updatePresetLabels();
    juce::String presetLabel(const juce::String& module) const;
    std::map<juce::String, juce::String> presetLabels;   // module -> shown label
    int presetLabelTick = 0;
    int titleTextWidth() const;
    juce::String describeInput(lmw::dsp::In) const;   // "<source> → <input> (normalled|cable)"
    juce::String describeInputOnly(lmw::dsp::In) const;   // without the LFO Delay note (S5.24)
    lmw::ui::schematic::LfoDelays lfoDelays() const;     // lfo1.delay, lfo2.delay (s)
    juce::String monoNote() const;                        // S5.25: "output is mono" line, or empty
    void applyAids();                                     // S6.20: settings.showScales / socketRings to the view

    LMWSynthProcessor& processor;
    LMWLookAndFeel lookAndFeel;
    bool shownOsDark = false;                             // Spec S6.29: the OS dark mode the theme was made for
    int systemThemeTick = 0;
    Metrics m = normalMetrics;
    std::array<Fader, 75> faders;                          // exactly the faders made (75 since S5.28: the five Shapes)
    std::array<Choice, 19> choices;                        // 17 since S7.15 (reverb Type), 19 since S5.30 (LFO Polarity)
    std::size_t usedFaders = 0, usedChoices = 0;
    std::vector<Group> groups;
    std::vector<std::unique_ptr<PanelHeader>> headers;    // one per group, same index
    std::optional<lmw::ui::RackLayout> rack;              // panel order per row (Spec S6.5)
    std::vector<juce::Rectangle<int>> rowBounds;          // from resized()
    int draggedPanel = -1;
    // Spec S7.14: the effects order on the Effects tab.
    std::vector<std::unique_ptr<EffectHeader>> effectHeaders;   // COMB, ENSEMBLE, DELAY, REVERB
    int draggedEffect = -1, shownEffectOrder = 0;
    // Spec S6.26: which units have an effect (followed by the timer); the unit a panel shows (its first socket's).
    lmw::dsp::UseResult shownUse;
    lmw::dsp::UseParams shownUseParams;
    void updateUse(bool force = false);
    juce::String useNote(std::string_view unit) const;
    static std::string panelUnit(const std::vector<Jack*>& jacks);
    void drawPanelFrame(juce::Graphics& g, juce::Rectangle<int> b, const std::vector<Jack*>& jacks, int thickness) const;
    std::optional<juce::Rectangle<int>> effectDrop;             // the drop marker while dragging
    std::vector<int> effectPanelOrder() const;                  // effectGroups indices left to right
    int effectPosition(int effect) const;                       // 0..3 in the current order
    std::optional<int> findEffectDrop(int effect, juce::Point<int> where, juce::Rectangle<int>* marker) const;
    void dragEffect(int effect, juce::Point<int> where);
    void dropEffect(int effect, juce::Point<int> where);
    bool moveEffect(int effect, int position);                  // false: no change
    void showEffectMenu(int effect);
    void resetEffectOrder();                                    // the default order (Reset layout on the Effects tab)
    std::optional<DropTarget> drop;
    juce::TextButton resetLayoutButton{"Reset layout"};
    std::vector<std::unique_ptr<Jack>> jacks;
    std::vector<std::vector<Jack*>> groupJacks;           // sockets per panel, same index as groups
    std::vector<std::vector<Jack*>> effectGroupJacks;     // Spec S7.6: sockets of the Effects tab panels
    std::optional<CableDrag> drag;
    Choice* routingChoice = nullptr;    // Spec S5.17: "Serial"/"Parallel", "Ser"/"Par" in compact view
    lmw::dsp::VcfRouting shownRouting = lmw::dsp::VcfRouting::Serial;   // Spec S5.17: redraw on change
    std::uint32_t shownPatchVersion = 0;
    std::unique_ptr<CableLayer> cables;                   // added last: on top of everything
    juce::TextButton compactButton{"Compact"};
    // Spec S6.3: contrast preset (AA / AAA / Max / Custom), kept in the user config file.
    lmw::ui::Settings settings;
    lmw::ui::theme::Palette palette;
    // Spec S7: tabs, patch name, notes and patch files.
    juce::TextButton instrumentButton{"Presets"};          // Spec S7.7, in the header left of Load...
    juce::TextButton panelTab{"Synth"}, effectsTab{"Effects"}, matrixTab{"Matrix"}, schematicTab{"Schematic"}, notesTab{"Notes"}, loadButton{"Load..."},
        saveButton{"Save..."}, menuButton{"Menu"},   // Spec S6.11: About, Contrast, Theme, Show keyboard
        undoButton, redoButton;                    // Spec S6.14 (user 2026-11-24): directly in the header
    void updateUndoButtons();
    std::unique_ptr<MatrixView> matrix;                   // Spec S6.8: the Matrix tab
    std::unique_ptr<MatrixView> fxMatrix;                 // Spec S6.25: the effects matrix, Effects tab below the panels
    std::unique_ptr<SchematicView> schematic;             // Spec S6.7: the Schematic tab
    std::unique_ptr<SchematicView> fxSchematic;           // Spec S6.31: the effects page, on the Effects tab below the panels
    std::unique_ptr<class AssistantView> assistant;       // Spec S12: the Assistant tab (standalone only)
    juce::TextButton assistantTab{"Assistant"};
    // Spec S7.4: the Effects tab — fixed panels (not part of the rack), one so far: REVERB.
    std::vector<Group> effectGroups;
    std::vector<juce::Component*> effectComponents;        // shown only on the Effects tab
    std::map<juce::String, std::unique_ptr<PresetButton>> effectPresets;   // Spec S7.5: COMB, DELAY, REVERB titles
    juce::Label patchLabel;
    juce::Label loadLabel;                                // Spec S6.28: "CPU 7 %"
    LimitLamp outLamp{"OUT"}, mainLamp{"MAIN"};           // Spec S5.31
    int outGroup = -1;                                    // the OUT panel's group (for its lamp)
    void updateLoadLabel();
    TitleButton titleButton;              // "LMW-Synth <version>", opens About (S6.10); hover text: 9P address
    void showAbout();
    juce::TextEditor notesEditor;
    int currentTab = 0;
    std::uint32_t shownPatchFile = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    // While any LMW editor is open, the LMW look is JUCE's default too, so top-level windows we open
    // (file chooser, alerts) are black on white also inside a DAW. Each plugin binary has its own JUCE,
    // so this does not touch the host or other plugins.
    juce::SharedResourcePointer<LMWLookAndFeel> defaultLook;
    inline static int openEditors = 0;                    // message thread only
    // On-screen keyboard drawn with the palette: a pressed key is filled with a colour that meets the
    // chosen contrast against the keys around it (darker on white keys, lighter on black keys; inverted
    // at Max), its note name in black or white, and hover is an outline instead of a grey tint.
    class Keyboard final : public juce::MidiKeyboardComponent {
    public:
        Keyboard(juce::MidiKeyboardState& s, const lmw::ui::theme::Palette& p)
            : juce::MidiKeyboardComponent(s, horizontalKeyboard), pal(p) {}
        void drawWhiteNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                           juce::Colour lineColour, juce::Colour textColour) override;
        void drawBlackNote(int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                           juce::Colour noteFillColour) override;
        // Octave scroll arrows: solid black (JUCE draws them at 40 % opacity, ~2.8:1), hover = outline.
        void drawUpDownButton(juce::Graphics&, int w, int h, bool isMouseOver, bool isButtonPressed,
                              bool movesOctavesUp) override;
        // Spec S3.2 (found 2026-11-24): notes held on the computer keys keep sounding when the focus moves to a
        // button or menu of LMW-Synth (choosing a preset while playing); JUCE released them on every focus loss.
        // They are released when the focus goes to a text box or to another program.
        void focusLost(FocusChangeType) override;
        bool keyStateChanged(bool isKeyDown) override;
        // Called by the editor's timer: while computer keys are held without the focus, follows the keys (a key
        // released while a menu was open), and releases them all when LMW-Synth is no longer in front.
        void followHeldKeys();
    private:
        static bool anyNoteKeyDown();
        const lmw::ui::theme::Palette& pal;
        bool held = false;                                // notes from the computer keys may be sounding
    };
    Keyboard keyboard;
    // Spec S3.2 (user 2026-11-28): while a socket menu with level faders is open, the computer keys still play
    // notes. A menu window ignores the keys it does not use and passes them nowhere (it has no parent), so this
    // listener, attached to the menu window by its level rows, hands them to the keyboard.
    struct MenuNoteKeys final : juce::KeyListener {
        explicit MenuNoteKeys(LMWSynthEditor& e) : editor(e) {}
        bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
        bool keyStateChanged(bool isKeyDown, juce::Component*) override;
        LMWSynthEditor& editor;
    };
    MenuNoteKeys menuNoteKeys{*this};
    bool keyboardShown = true;                            // Spec S6.12
    juce::TextButton keyboardButton;                      // "▾ keyboard" / "▴ keyboard"
    juce::TooltipWindow tooltips{this, 2000};   // hover texts appear after 2 s

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
