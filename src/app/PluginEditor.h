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
#include "ui/RackLayout.h"

// Spec S6.2: black-on-white look; every colour pair is 21:1.
class LMWLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LMWLookAndFeel();
    // Spec S6.1: vertical faders, drawn only in black on white (no grey tints).
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override { return 7; }
    // Choice boxes with a narrow (16 px) arrow area, so short names fit in compact view.
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};

class LMWSynthEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LMWSynthEditor(LMWSynthProcessor&);
    ~LMWSynthEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // QWERTY notes work whichever control has focus (Spec S3.2): letter keys not used by the
    // focused control reach the editor and are passed to the on-screen keyboard.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override;
    static bool isTypingText();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // A fader whose hover text (Spec S6.1, shown after 2 s) is computed when shown, so it always
    // reflects the current patch and mod wheel.
    struct TipSlider : juce::Slider {
        TipSlider() : juce::Slider(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow) {}
        std::function<juce::String()> tip;
        juce::String getTooltip() override { return tip ? tip() : juce::String(); }
    };
    struct Fader {
        TipSlider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };
    struct Choice {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };
    struct Item {                       // one column in a module group: a fader or a choice box
        Fader* fader = nullptr;
        Choice* choice = nullptr;
    };
    struct Group {                      // a framed module panel (Spec S6.1)
        juce::String title;
        int row = 0;                    // row in the default layout (Spec S6.5)
        std::vector<Item> items;
        juce::Rectangle<int> bounds;
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
    private:
        LMWSynthEditor& editor;
        int panel;
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
        void focusGained(FocusChangeType) override { repaint(); }
        void focusLost(FocusChangeType) override { repaint(); }
        juce::String getTooltip() override;
        juce::Point<float> centreInEditor() const;    // where cables end
        juce::Rectangle<float> socket() const;        // the circle, local coordinates
        const bool output;
        const int port;                               // lmw::dsp::Out or In index
    private:
        LMWSynthEditor& editor;
        juce::String name;
        bool dragging = false;
    };
    // Draws all cables above the panels; lets every mouse event through to the controls below.
    class CableLayer final : public juce::Component {
    public:
        explicit CableLayer(LMWSynthEditor& e) : editor(e) { setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics&) override;
    private:
        LMWSynthEditor& editor;
    };
    struct CableDrag {                  // a cable being drawn or moved with the mouse
        Jack* fixed = nullptr;          // the end that stays where it is
        int detachedInput = -1;         // moving the cable that was plugged into this input
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
    static constexpr Metrics normalMetrics {80, 10, 28, 266, 22, 72, 26, 90, 36, 36, 15.0f, 15.0f};
    static constexpr Metrics compactMetrics{60,  6, 22, 200, 17, 56, 20, 70, 30, 30, 13.0f, 13.0f};

    Item addFader(const char* paramId, const juce::String& text, std::function<juce::String()> tip = {});
    Item addChoice(const char* paramId, const juce::String& text, const juce::StringArray& options);
    void layoutItem(const Item&, juce::Rectangle<int> column) const;
    void setCompact(bool compact);
    void updateSize();                                    // window size from metrics and row count
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
    void connect(int input, int output);                  // output = Patch::none removes the cable
    void removeAllCables();
    void showJackMenu(Jack&);
    juce::String describeOutput(lmw::dsp::Out) const;     // "<output> → <inputs it feeds>"
    juce::String describeInput(lmw::dsp::In) const;   // "<input> ← <source> (normalled|cable)"

    LMWSynthProcessor& processor;
    LMWLookAndFeel lookAndFeel;
    Metrics m = normalMetrics;
    std::array<Fader, 18> faders;
    std::array<Choice, 2> choices;
    std::size_t usedFaders = 0, usedChoices = 0;
    std::vector<Group> groups;
    std::vector<std::unique_ptr<PanelHeader>> headers;    // one per group, same index
    std::optional<lmw::ui::RackLayout> rack;              // panel order per row (Spec S6.5)
    std::vector<juce::Rectangle<int>> rowBounds;          // from resized()
    int draggedPanel = -1;
    std::optional<DropTarget> drop;
    juce::TextButton resetLayoutButton{"Reset layout"};
    std::vector<std::unique_ptr<Jack>> jacks;
    std::vector<std::vector<Jack*>> groupJacks;           // sockets per panel, same index as groups
    std::optional<CableDrag> drag;
    std::uint32_t shownPatchVersion = 0;
    std::unique_ptr<CableLayer> cables;                   // added last: on top of everything
    juce::TextButton compactButton{"Compact"};
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips{this, 2000};   // hover texts appear after 2 s

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
