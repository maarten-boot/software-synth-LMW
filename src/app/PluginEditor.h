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
    void timerCallback() override { processor.updateKeyboardFromMidiInput(); }
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
    // Where a dragged panel would land; shown as a black bar (drop marker).
    struct DropTarget {
        int row = 0, index = 0;
        bool newRow = false;
        juce::Rectangle<int> marker;
    };

    // Layout sizes; Spec S6.4: normal or compact view.
    struct Metrics {
        int colW, gap, header, rowH, labelH, boxW, boxH, keysH, titleH;
        float labelFont, titleFont;
    };
    static constexpr Metrics normalMetrics {80, 10, 28, 230, 22, 72, 26, 90, 36, 15.0f, 15.0f};
    static constexpr Metrics compactMetrics{60,  6, 22, 170, 17, 56, 20, 70, 30, 13.0f, 13.0f};

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
    juce::TextButton compactButton{"Compact"};
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips{this, 2000};   // hover texts appear after 2 s

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
