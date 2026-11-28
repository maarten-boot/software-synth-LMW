// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChangesText.h"   // generated from architecture/Changes.md
#include "TermsView.h"
#include "SocketRing.h"
#include "PluginEditor.h"
#include "ThemeInk.h"
#include "ProcessMemory.h"
#include "AssistantView.h"
#include "AssistantSettings.h"
#include "KeyStore.h"
#if JUCE_LINUX || JUCE_MAC || JUCE_BSD
 #include <sys/stat.h>
#endif
#include "ThemeDialog.h"
#include "app/InstrumentPresets.h"
#include "app/Tutorial.h"
#include "ui/Theme.h"
#include "ui/CableEdit.h"
#include "LMWVersion.h"
#include "NinePService.h"

namespace {
using lmw::app::toJuce;                                 // Spec S6.29: kFg / kBg follow the theme (ThemeInk.h)
using lmw::app::kBg;
using lmw::app::kFg;
}

LMWLookAndFeel::LMWLookAndFeel() { applyTheme(); }

void LMWLookAndFeel::applyTheme() {
    using C = juce::LookAndFeel_V4::ColourScheme;
    setColourScheme(C{kBg, kBg, kBg, kFg, kFg, kFg, kBg, kFg, kFg});
    // windowBackground, widgetBackground, menuBackground, outline, defaultText,
    // defaultFill, highlightedText, highlightedFill, menuText
    setColour(juce::Slider::thumbColourId, kFg);
    setColour(juce::Slider::trackColourId, kFg);
    setColour(juce::Slider::backgroundColourId, kFg);
    setColour(juce::Slider::textBoxTextColourId, kFg);
    setColour(juce::Slider::textBoxBackgroundColourId, kBg);
    setColour(juce::Slider::textBoxOutlineColourId, kFg);
    setColour(juce::Label::textColourId, kFg);
    setColour(juce::ComboBox::textColourId, kFg);
    setColour(juce::ComboBox::backgroundColourId, kBg);
    setColour(juce::ComboBox::outlineColourId, kFg);
    setColour(juce::ComboBox::arrowColourId, kFg);
    setColour(juce::PopupMenu::backgroundColourId, kBg);
    setColour(juce::PopupMenu::textColourId, kFg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kFg);
    setColour(juce::PopupMenu::highlightedTextColourId, kBg);
    const auto& pal = lmw::ui::theme::current();                       // S6.29: a piano in every theme
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, toJuce(pal.whiteKey));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, toJuce(pal.blackKey));
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, toJuce(pal.keyText));
    // Pressed and hovered keys are drawn by LMWSynthEditor::Keyboard with the contrast palette.
    setColour(juce::TextEditor::backgroundColourId, kBg);              // typed values, Custom ratio
    setColour(juce::TextEditor::textColourId, kFg);
    setColour(juce::TextEditor::outlineColourId, kFg);
    setColour(juce::TextEditor::focusedOutlineColourId, kFg);
    setColour(juce::TextEditor::highlightColourId, kFg);
    setColour(juce::TextEditor::highlightedTextColourId, kBg);
    setColour(juce::CaretComponent::caretColourId, kFg);
    setColour(juce::AlertWindow::backgroundColourId, kBg);
    setColour(juce::AlertWindow::textColourId, kFg);
    setColour(juce::AlertWindow::outlineColourId, kFg);
    setColour(juce::MidiKeyboardComponent::textLabelColourId, toJuce(pal.keyText));
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, kBg);
    setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId, kFg);
    setColour(juce::TooltipWindow::backgroundColourId, kBg);           // hover texts: black on white
    setColour(juce::TooltipWindow::textColourId, kFg);
    setColour(juce::TooltipWindow::outlineColourId, kFg);
    setColour(juce::TextButton::buttonColourId, kBg);                  // off: black on white
    setColour(juce::TextButton::textColourOffId, kFg);
    setColour(juce::TextButton::buttonOnColourId, kFg);                // on: white on black
    setColour(juce::TextButton::textColourOnId, kBg);
}

void LMWLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      juce::Slider::SliderStyle style, juce::Slider& s) {
    if (style == juce::Slider::LinearHorizontal) {             // Spec S6.18: the cable faders in a socket menu
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float left = static_cast<float>(x), right = static_cast<float>(x + width);
        g.setColour(kFg);
        g.fillRect(juce::Rectangle<float>(left, cy - 1.0f, right - left, 2.0f));
        const float zero = s.getMinimum() < 0.0 ? static_cast<float>(s.getPositionOfValue(0.0)) : left;
        g.fillRect(juce::Rectangle<float>(std::min(zero, sliderPos), cy - 3.0f, std::fabs(sliderPos - zero), 6.0f));
        if (s.getMinimum() < 0.0) g.fillRect(juce::Rectangle<float>(zero - 1.0f, cy - 7.0f, 2.0f, 14.0f));   // the 0 mark
        const auto thumb = juce::Rectangle<float>(12.0f, std::min(22.0f, static_cast<float>(height))).withCentre({sliderPos, cy});
        g.fillRect(thumb);
        g.setColour(kBg);
        g.fillRect(juce::Rectangle<float>(sliderPos - 1.0f, thumb.getY() + 4.0f, 2.0f, thumb.getHeight() - 8.0f));
        if (s.hasKeyboardFocus(false)) {
            g.setColour(kFg);
            g.drawRect(juce::Rectangle<float>(left, static_cast<float>(y), right - left, static_cast<float>(height)).expanded(2.0f), 1.0f);
        }
        return;
    }
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, s);
        return;
    }
    // Empty track = thin line, filled part (value) = thick bar, thumb = black cap with a white centre line.
    // Distinguished by shape, not by colour, so every pair stays black/white (21:1).
    const float cx     = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float top    = static_cast<float>(y);
    const float bottom = static_cast<float>(y + height);
    g.setColour(kFg);
    g.fillRect(juce::Rectangle<float>(cx - 1.0f, top, 2.0f, bottom - top));
    if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0) {      // Spec S5.21, S6.17: a range through 0 fills from 0
        const float zero = static_cast<float>(s.getPositionOfValue(0.0));
        g.fillRect(juce::Rectangle<float>(cx - 3.0f, std::min(zero, sliderPos), 6.0f, std::fabs(zero - sliderPos)));
        g.fillRect(juce::Rectangle<float>(cx - 7.0f, zero - 1.0f, 14.0f, 2.0f));   // the 0 mark
    } else {
        g.fillRect(juce::Rectangle<float>(cx - 3.0f, sliderPos, 6.0f, bottom - sliderPos));
    }
    // Spec S6.17: scale ticks left of the track (majors longer), value texts at the majors right of it (normal
    // view); their spacing shows the curve. Black on white, under the thumb.
    if (const auto* scaled = dynamic_cast<const ScaleSlider*>(&s); scaled != nullptr && scaled->showTicks) {   // S6.20
        g.setFont(lmw::app::smallFont(9.0f));               // S6.29: bold in a dark theme
        const float textX = cx + 17.0f, textW = static_cast<float>(x + width) - textX + 2.0f;
        const bool roomForTexts = static_cast<float>(height) >= 60.0f;   // S5.28: a short fader (LFO Shape) keeps only the marks
        for (const auto& t : scaled->ticks) {
            const float ty = static_cast<float>(s.getPositionOfValue(t.value));
            const float len = t.major ? 6.0f : 3.0f;
            g.fillRect(juce::Rectangle<float>(cx - 17.0f - len, ty - 0.5f, len, 1.0f));
            if (t.major && scaled->tickLabels && roomForTexts && textW > 8.0f)
                g.drawText(juce::String(t.text), juce::Rectangle<float>(textX, ty - 5.0f, textW, 10.0f),
                           juce::Justification::centredLeft, false);
        }
    }

    const auto thumb = juce::Rectangle<float>(30.0f, 14.0f).withCentre({cx, sliderPos});
    g.fillRect(thumb);
    g.setColour(kBg);
    g.fillRect(juce::Rectangle<float>(thumb.getX() + 4.0f, sliderPos - 1.0f, thumb.getWidth() - 8.0f, 2.0f));
    if (s.hasKeyboardFocus(false)) {           // visible focus ring, black on white
        g.setColour(kFg);
        g.drawRect(juce::Rectangle<float>(static_cast<float>(x), top, static_cast<float>(width), bottom - top).expanded(2.0f), 1.0f);
    }
}

namespace {
const juce::Identifier kCompactProp{"uiCompact"};   // stored in the plugin/standalone state
const juce::Identifier kKeyboardProp{"uiKeyboard"}; // Spec S6.12: on-screen keyboard shown
const juce::Identifier kLayoutProp{"uiLayout"};     // rack layout (Spec S6.5), RackLayout::serialize()
constexpr int kMaxRows = 6;                       // Spec S6.21 (was 4): more rows, the rack scrolls
constexpr int kSmallestW = 1280, kSmallestH = 720;   // Spec S6.21: the smallest window

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
}

void LMWLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                  juce::ComboBox& box) {
    const auto r = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r, 3.0f, box.hasKeyboardFocus(true) ? 2.0f : 1.0f);
    const float cx = static_cast<float>(width) - 9.0f, cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;                                   // small down-arrow in the 16 px arrow area
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(1.5f));
}

void LMWLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(2, 1, box.getWidth() - 18, box.getHeight() - 2);
    label.setBorderSize(juce::BorderSize<int>(0, 3, 0, 0));
    label.setFont(getComboBoxFont(box));
}

LMWSynthEditor::LMWSynthEditor(LMWSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      settings(lmw::ui::parseConfig(configFile().loadFileAsString().toStdString())),
      palette(lmw::ui::theme::palette(settings.minRatio(), settings.colours())),
      keyboard(p.keyboardState(), palette) {
    shownOsDark = juce::Desktop::getInstance().isDarkModeActive();                  // Spec S6.29: before anything is drawn
    palette = lmw::ui::theme::palette(settings.minRatio(), settings.colours(shownOsDark));
    lmw::ui::theme::current() = palette;
    lookAndFeel.applyTheme();
    defaultLook.get().applyTheme();
    setLookAndFeel(&lookAndFeel);
    // Spec S6.21: the rack surface in a scrolling view; every rack component below is its child.
    rackArea = std::make_unique<RackArea>(*this);
    rackView.setViewedComponent(rackArea.get(), false);
    rackView.setScrollBarsShown(true, true);
    rackView.setScrollBarThickness(10);
    for (auto* bar : {&rackView.getVerticalScrollBar(), &rackView.getHorizontalScrollBar()}) {
        bar->setColour(juce::ScrollBar::thumbColourId, kFg);       // black on white (S6.2)
        bar->setColour(juce::ScrollBar::trackColourId, kBg);
    }
    addAndMakeVisible(rackView);
    juce::Desktop::getInstance().addFocusChangeListener(this);
    {   // Spec S6.24: the playing controls left of the keyboard
        namespace P = lmw::param;
        bendWheel.setRange(-1.0, 1.0);
        modWheelCtl.setRange(0.0, 1.0);
        bendWheel.setTooltip("Pitch bend: drag up or down; it springs back to the centre (MIDI pitch bend moves it too).\n"
                             "Range: the box on the right (semitones)");
        modWheelCtl.setTooltip("Mod wheel: vibrato (VCO FM + wheel x 1 semitone) and the keys.mod output (KEYS panel);\n"
                               "MIDI CC 1 moves it too");
        bendWheel.onValueChange = [this] { processor.requestWheels(static_cast<float>(bendWheel.getValue()), std::nullopt); };
        modWheelCtl.onValueChange = [this] { processor.requestWheels(std::nullopt, static_cast<float>(modWheelCtl.getValue())); };
        glideSlider.setTooltip("Glide (portamento): how long a new note takes to slide in from the last one; 0 = none");
        glideAttachment = std::make_unique<SliderAttachment>(processor.state(), P::glide, glideSlider);
        glideModeBox.addItemList({"Always", "Legato"}, 1);
        glideModeBox.setTooltip("Glide on every new note (Always), or only while another key is held (Legato)");
        glideModeAttachment = std::make_unique<ComboAttachment>(processor.state(), P::glideMode, glideModeBox);
        bendRangeBox.addItemList({juce::String::fromUTF8("\xc2\xb1" "1 st"), juce::String::fromUTF8("\xc2\xb1" "2 st"),
                                  juce::String::fromUTF8("\xc2\xb1" "7 st"), juce::String::fromUTF8("\xc2\xb1" "12 st")}, 1);
        bendRangeBox.setTooltip("Pitch bend range: how far the bend wheel (and MIDI pitch bend) bends at its ends");
        bendRangeAttachment = std::make_unique<ComboAttachment>(processor.state(), P::bendRange, bendRangeBox);
        for (auto* l : {&bendLabel, &modLabel, &glideLabel, &bendRangeLabel}) {
            l->setJustificationType(juce::Justification::centred);
            l->setColour(juce::Label::textColourId, kFg);
            addAndMakeVisible(*l);
        }
        for (juce::Component* c : {static_cast<juce::Component*>(&bendWheel), static_cast<juce::Component*>(&modWheelCtl),
                                   static_cast<juce::Component*>(&glideSlider), static_cast<juce::Component*>(&glideModeBox),
                                   static_cast<juce::Component*>(&bendRangeBox)})
            addAndMakeVisible(*c);
    }
    namespace P = lmw::param;
    using lmw::dsp::In;
    const juce::String arrow = juce::String::fromUTF8("\xe2\x86\x92");

    // Hover texts (Spec S6.1): where each input comes from and what the fader does to it.
    auto fmTip = [this](int n) {
        return [this, n] {
            const float wheel = processor.modWheel();
            const float fm = processor.state().getRawParameterValue(lmw::param::vcoFm[static_cast<std::size_t>(n)])->load();
            return describeInput(lmw::dsp::vcoFmIn(n)) + "\nVibrato depth = FM + mod wheel x 1 st"
                 + "\nNow: FM " + juce::String(fm, 1) + " st + wheel " + pct(wheel) + " = "
                 + juce::String(fm + wheel * lmw::dsp::modWheelFmSemitones, 1) + " st";
        };
    };
    auto inputTip = [this](In in, const juce::String& what) {
        return [this, in, what] { return describeInput(in) + "\n" + what; };
    };

    // Spec S6.1, S5.10: module panels in three rows (signal flow left to right, top to bottom):
    // VCO 1 | VCO 2 | VCO 3 | NOISE | LFO 1;  MIXER | VCF 1 | VCF 2 | FORMANT;  KEYS | ENV 1 | ENV 2 | LFO 2 | VCA | OUT.
    // Titles of the envelopes name their normalled destination (S5.2).
    std::array<std::size_t, 3> vcoPanel{};
    for (int n = 0; n < 3; ++n) {
        const auto i = static_cast<std::size_t>(n);
        vcoPanel[i] = groups.size();
        groups.push_back({"VCO " + juce::String(n + 1), 0,
            {addFader(P::vcoFm[i], "FM", fmTip(n)),                // above the FM input socket
             addFader(P::vcoPm[i], "PM", inputTip(lmw::dsp::vcoPmIn(n),   // Spec S5.20, above the PM socket
                      "Phase modulation (FM as on DX synths): index in radians for 1.0 at the PM input.\n"
                      "Patch another VCO here (it becomes the modulator), or this VCO's own Out (feedback).\n"
                      "0 = off; a sine carrier and modulator sound clearest")),
             addFader(P::vcoTune[i], "Tune", inputTip(lmw::dsp::vcoPitchIn(n),
                      "Detune, -7 to +7 semitones; Oct shifts by whole octaves")),
             addFader(P::vcoShape[i], "Shape", inputTip(lmw::dsp::vcoShapeIn(n),   // Spec S5.28, above its socket
                      "Shape of the wave: Square narrows to a pulse (PWM), Triangle leans to a saw, Saw doubles\n"
                      "(an octave up at 100 %), Sine bends brighter, Pulse narrows further. 0 = the plain wave.\n"
                      "Shape input: added to the fader (an LFO here gives pulse-width modulation)")),
             stacked(addChoice(P::vcoWave[i], "Wave", {"Sine", "Saw", "Square", "Triangle", "Pulse"},
                               "Waveform. Pulse: narrow (12.5 %), bright and nasal, good under FORMANT"),
                     addChoice(P::vcoOctave[i], "Oct", {"-2", "-1", "0", "+1", "+2"}, "Octave"))}, {}});
    }
    // Spec S5.15: NOISE — colour, and the sample & hold random source (Rate, Smooth); both shared.
    const std::size_t noisePanel = groups.size();
    groups.push_back({"NOISE", 0, {addChoice(P::noiseColour, "Colour", {"White", "Pink", "Brown"},
                                             "Noise colour: white (flat), pink (-3 dB/octave), brown (-6 dB/octave)"),
                                   addFader(P::noiseRate, "Rate", [] {
                                       return juce::String("Sample & hold: new random value (noise.rand) this many times per second");
                                   }),
                                   addFader(P::noiseSmooth, "Smooth", [] {
                                       return juce::String("0 = random steps; higher = glides from value to value");
                                   })}, {}});
    // Spec S5.12: LFO 1 = vibrato (normalled to the VCO FM inputs), LFO 2 = tremolo (normalled to vca.mod).
    const juce::String lfoShapeTip = "Shape of the wave: Triangle leans to a ramp, Square gets shorter (pulses), Saw doubles,\n"
                                     "Sine bends. 0 = the plain wave. Shape input: added to the fader (per-voice sources: newest note)";   // Spec S5.28
    // Spec S5.30: Polarity under the Delay fader, above the Out socket it changes (as OUT's Spread mode; under the
    // Wave box it left Shape no room). The box text says it: +-, +, -; the parameter's names are words.
    const juce::StringArray polarities{juce::String::fromUTF8("\xc2\xb1"), "+", juce::String::fromUTF8("\xe2\x88\x92")};
    const auto polarityTip = [](int n) {
        return "Polarity of lfo" + juce::String(n) + ".out, wherever it goes (also its normal connection):\n"
               + juce::String::fromUTF8("\xc2\xb1") + " = -1 to +1 (as before), + = 0 to +1 (only up), "
               + juce::String::fromUTF8("\xe2\x88\x92") + " = -1 to 0 (only down).\n"
               "+ helps where an input adds to a fader at its end, e.g. VCA CV with Initial 0, Shape from 0.";
    };
    const std::size_t lfoPanel = groups.size();
    groups.push_back({"LFO 1", 2, {boxOver(addChoice(P::lfoWave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),   // S5.28: row 3
                                           addFader(P::lfoShape, "Shape", inputTip(In::Lfo1Shape, lfoShapeTip))),
                                   addFader(P::lfoRate, "Rate", inputTip(In::Lfo1Rate,
                                       "Shared LFO 1 (vibrato): lfo1.out feeds vco1.fm, vco2.fm and vco3.fm\n"
                                       "(normalled); the FM faders set the depth.\n"
                                       "Rate input: +1.0 doubles the rate, -1.0 halves it (per-voice sources: newest note)")),
                                   withBox(addFader(P::lfoDelay, "Delay", [] {
                                       return juce::String("Delayed vibrato: after each note starts, LFO 1 has no effect for the first\n"
                                                           "half of this time, then fades in to full at this time (0 = at once).");
                                   }), addChoice(P::lfoPolarity, "Polarity", polarities, polarityTip(1)))}, {}});   // S5.30
    const std::size_t mixPanel = groups.size();
    groups.push_back({"MIXER", 0,   // user 2026-11-26: at the end of row 1 by default (after NOISE), FORMANT to row 2
        {addFader(P::mixVco[0], "VCO 1", inputTip(In::MixIn1, "Level of mixer input 1")),
         addFader(P::mixVco[1], "VCO 2", inputTip(In::MixIn2, "Level of mixer input 2")),
         addFader(P::mixVco[2], "VCO 3", inputTip(In::MixIn3, "Level of mixer input 3")),
         addFader(P::mixNoise, "Noise", inputTip(In::MixIn4, "Level of mixer input 4"))}, {}});
    std::array<std::size_t, 2> vcfPanel{};               // Spec S5.9, S5.11: VCF 1 -> VCF 2 in series (S5.17: or parallel)
    const auto routing = addChoice(P::vcfRouting, "Routing", {"Serial", "Parallel"},   // Spec S5.17, on VCF 2
                                   "VCF routing. Serial: mixer -> VCF 1 -> VCF 2.\n"
                                   "Parallel: the mixer feeds both VCFs; FORMANT gets their average (50 % each).");
    routingChoice = routing.choice;
    for (int n = 0; n < 2; ++n) {
        const auto i = static_cast<std::size_t>(n);
        const In in = n == 0 ? In::Vcf1In : In::Vcf2In, cut = n == 0 ? In::Vcf1Cutoff : In::Vcf2Cutoff,
                 res = n == 0 ? In::Vcf1Res : In::Vcf2Res;
        vcfPanel[i] = groups.size();
        groups.push_back({"VCF " + juce::String(n + 1), 1,
            {addFader(P::vcfCutoff[i], "Cutoff", inputTip(in, n == 0 ? "Cutoff frequency"
                          : "Cutoff frequency. At the top (24 dB, Low, Reso 0) VCF 2 is open: bypassed exactly.")),
             addFader(P::vcfRes[i], "Reso", inputTip(res, "Resonance; self-oscillates near the top.\n"
                                                      "A signal on the Res input (e.g. keys.vel) adds to it, up to the maximum.")),
             addFader(P::vcfEnv[i], "Env 2", inputTip(cut, "Cutoff modulation depth in octaves")),
             addFader(P::vcfKey[i], "Key", [] {
                 return juce::String("Key tracking: at 100 % the cutoff follows the keys one octave per octave\n"
                                     "(unchanged at C4); 0 % = same cutoff for every note");
             }),
             n == 0 ? stacked(addChoice(P::vcfSlope[i], "Slope", {"36 dB", "24 dB", "12 dB", "6 dB"},
                                        "Steepness. Bandpass and notch: 12 or 24 dB (6 dB acts as 12 dB, 36 dB as 24 dB)."),
                              addChoice(P::vcfType[i], "Type", {"Low", "High", "Band", "Notch"},
                                        "Lowpass, highpass, bandpass or notch"))
                    : stacked(addChoice(P::vcfSlope[i], "Slope", {"36 dB", "24 dB", "12 dB", "6 dB"},
                                        "Steepness. Bandpass and notch: 12 or 24 dB (6 dB acts as 12 dB, 36 dB as 24 dB)."),
                              addChoice(P::vcfType[i], "Type", {"Low", "High", "Band", "Notch"},
                                        "Lowpass, highpass, bandpass or notch"),
                              routing)}, {}});
    }
    const std::size_t keysPanel = groups.size();        // Spec S5.8; 2 columns for 3 sockets
    groups.push_back({"KEYS", 2, {addFader(P::voices, "Voices", [] {
                          return juce::String("Number of voices. Keyboard, computer keys and MIDI play the voices;\n"
                                              "the sockets below give each voice's pitch, gate and velocity.");
                      })}, {}, 2});
    const std::size_t env1Panel = groups.size();
    groups.push_back({"ENV 1 " + arrow + " VCA", 2,
        {addFader(P::env1Attack, "Attack"), addFader(P::env1Decay, "Decay"),
         addFader(P::env1Sustain, "Sustain"), addFader(P::env1Release, "Release")}, {}});
    const std::size_t env2Panel = groups.size();
    groups.push_back({"ENV 2 " + arrow + " VCF", 2,
        {addFader(P::env2Attack, "Attack"), addFader(P::env2Decay, "Decay"),
         addFader(P::env2Sustain, "Sustain"), addFader(P::env2Release, "Release")}, {}});
    const std::size_t lfo2Panel = groups.size();
    groups.push_back({"LFO 2", 2, {boxOver(addChoice(P::lfo2Wave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),
                                           addFader(P::lfo2Shape, "Shape", inputTip(In::Lfo2Shape, lfoShapeTip))),
                                   addFader(P::lfo2Rate, "Rate", inputTip(In::Lfo2Rate,
                                       "Shared LFO 2 (tremolo): lfo2.out feeds vca.mod (normalled);\n"
                                       "the VCA Tremolo fader sets the depth.\n"
                                       "Rate input: +1.0 doubles the rate, -1.0 halves it (per-voice sources: newest note)")),
                                   withBox(addFader(P::lfo2Delay, "Delay", [] {
                                       return juce::String("Delayed tremolo: after each note starts, LFO 2 has no effect for the first\n"
                                                           "half of this time, then fades in to full at this time (0 = at once).");
                                   }), addChoice(P::lfo2Polarity, "Polarity", polarities, polarityTip(2)))}, {}});   // S5.30
    const std::size_t vcaPanel = groups.size();
    groups.push_back({"VCA", 2, {addFader(P::vcaInitial, "Initial", [this] {
                         return describeInput(In::VcaIn) + "\n" + describeInput(In::VcaCv)
                              + "\nGain = Initial + CV (0..1). Above 0, notes keep sounding after release.";
                     }),
                     addFader(P::vcaTremolo, "Tremolo", inputTip(In::VcaMod,
                         "Tremolo depth: vca.mod swings the gain down by up to this much (0 = off)")),
                     addFader(P::vcaVelocity, "Velocity", inputTip(In::VcaVel,   // Spec S5.22
                         "How much the note velocity (vca.vel, normally keys.vel) sets the loudness:\n"
                         "100 % = soft notes quiet, 0 % = every note full"))}, {}});
    const std::size_t outPanel = groups.size();
    outGroup = static_cast<int>(outPanel);                // Spec S5.31: its limiter lamp
    groups.push_back({"OUT", 2, {addFader(P::gain, "Volume", inputTip(In::OutIn, "All voices summed, then soft limiter (per side)")),
        // Spec S5.25: each voice placed left-right before the sum.
        addFader(P::outPan, "Pan", [this] {
            return describeInput(In::OutPan) + "\nWhere the sound sits: -100 % left, 0 centre, +100 % right (equal power).\n"
                   "Pan input: added per voice, e.g. LFO 2 for auto-pan, noise.rand for a random place per note" + monoNote();
        }),
        withBox(addFader(P::outSpread, "Spread", [this] {
                    return juce::String("How far the notes spread around Pan: by pitch (Keys: low left, high right, as on a piano)\n"
                                        "or by voice (Voices: alternating; one voice stays in the centre). The box below chooses.") + monoNote();
                }),
                addChoice(P::spreadMode, "Spread mode", {"Keys", "Voices"},
                          "Spread by Keys (pitch: C4 in the centre, +-3 octaves to the sides) or by Voices (alternating left / right)"))}, {}});
    // Spec S5.16: FORMANT, after VCF 2 in row 2. Created last, so saved rack layouts keep their panel
    // numbers (S6.5: a stored layout without it gets it at the end of its default row).
    const std::size_t formantPanel = groups.size();
    groups.push_back({"FORMANT", 1,   // user 2026-11-26: after VCF 2 by default (S5.26: was at the end of row 1)
        // Voice first (user 2026-11-22): the In socket goes under it, on the left; Out stays right.
        {addChoice(P::formantVoice, "Voice", {"Bass", "Bari", "Tenor", "Alto", "Sopr", "Child"},
                   "Voice type: Bass, Baritone, Tenor, Alto, Soprano or Child (formants from low to high)"),
         addFader(P::formantVowel, "Vowel", inputTip(In::FormantVowel,
             "Vowel A, E, I, O, U, gliding in between.\nA signal on the Vowel input adds to it: 1.0 = the whole A to U span.")),
         addFader(P::formantRes, "Reso", inputTip(In::FormantRes,   // Spec S5.29: the Res input
             "Sharpness of the formants: 50 % = as sung, higher = narrower peaks.\n"
             "A signal on the Res input adds to it: 1.0 = the whole range.")),
         addFader(P::formantMix, "Mix", inputTip(In::FormantIn,
             "0 = off (the input passes unchanged), 100 % = only the formants"))}, {}});
    // Spec S5.26: MIX 2 after FORMANT, before the VCA: three inputs with level and pan; the voice is stereo from
    // here while an input is panned. Created last: saved layouts keep their panel numbers (it joins row 2).
    const std::size_t mix2Panel = groups.size();
    {
        std::vector<Item> items;
        for (int k = 0; k < 3; ++k) {
            const auto i = static_cast<std::size_t>(k);
            const auto in = static_cast<In>(static_cast<int>(In::Mix2In1) + k);
            items.push_back(addFader(P::mix2Level[i], "Level " + juce::String(k + 1),
                inputTip(in, k == 0 ? "Level of input 1 (normally FORMANT, so 100 % = as before)" : "Level of this input (0 % = off)")));
            items.push_back(addFader(P::mix2Pan[i], "Pan " + juce::String(k + 1), [k] {
                return "Where input " + juce::String(k + 1) + " sits: -100 % left, 0 centre, +100 % right (equal power).\n"
                       "While an input is off the centre the voice is stereo from here through the VCA to OUT.";
            }));
        }
        groups.push_back({"MIX 2", 1, std::move(items), {}});
    }
    // Spec S7.4, S7.6: Effects tab: SYNTH (out.out, the synth after OUT) | COMB | REVERB | MAIN (main.in, what
    // is heard); normalled SYNTH -> COMB -> REVERB -> MAIN, cables change the order.
    effectGroups.push_back({"SYNTH", 0, {}, {}, 1});
    effectGroups.push_back({"COMB", 0,
        {addFader(P::combFreq, "Freq", [] { return juce::String("Comb frequency: the resonances lie at its multiples (hollow, tube-like)"); }),
         addFader(P::combKey, "Key", [] {
             return juce::String("Key tracking: at 100 % the frequency follows the newest note one octave per octave (from C4)");
         }),
         addFader(P::combFeedback, "Feedback", [] {
             return juce::String("Positive: resonances at every multiple; negative: at the odd ones (clarinet-like). 0 = off");
         }),
         addFader(P::combDamping, "Damping", [] { return juce::String("Softens the high resonances"); }),
         addFader(P::combMix, "Mix", [] { return juce::String("Dry/wet: 0 = off (the input passes unchanged), 100 % = only the comb"); })}, {}});
    // Spec S7.11: ENSEMBLE between COMB and DELAY.
    effectGroups.push_back({"ENSEMBLE", 0,
        {addFader(P::ensembleDepth, "Depth", [] { return juce::String("How far the three chorus lines sweep: more = wider and more moving"); }),
         addFader(P::ensembleRate, "Rate", [] { return juce::String("Speed of the slow sweep; the fast shimmer runs 10 times faster"); }),
         addFader(P::ensembleShimmer, "Shimmer", [] {
             return juce::String("The fast sweep's share: 0 = a slow chorus, 100 % = the full string-machine shimmer");
         }),
         addFader(P::ensembleMix, "Mix", [] { return juce::String("Dry/wet: 0 = off (the input passes unchanged), 100 % = only the ensemble"); })}, {}});
    // Spec S7.8: DELAY between COMB and REVERB.
    effectGroups.push_back({"DELAY", 0,
        {addFader(P::delayTime, "Time", [] { return juce::String("Echo time; moving it while echoes ring bends their pitch (tape-like glide)"); }),
         addFader(P::delayFeedback, "Feedback", [] { return juce::String("How much of each echo comes back: more = more repeats"); }),
         addFader(P::delayDamping, "Damping", [] { return juce::String("Each repeat darker than the one before"); }),
         addFader(P::delaySpread, "Spread", [] { return juce::String("0 = each side echoes itself; 100 % = ping-pong between left and right"); }),
         addFader(P::delayMix, "Mix", [] { return juce::String("Dry/wet: 0 = off (the input passes unchanged), 100 % = only the echoes"); })}, {}});
    effectGroups.push_back({"REVERB", 0,
        // Mix on the right as on every other effect (user 2026-11-24; it was first).
        // Spec S7.15: Type (Classic = Freeverb as before, Hall = the new engine) and Pre-delay.
        {addChoice(P::reverbType, "Type", {"Classic", "Hall"},
                   "Reverb engine. Hall: smooth, dense tail without a metallic ring (new sounds).\n"
                   "Classic: the reverb of LMW-Synth 1.7 and before (older patches keep it)."),
         addFader(P::reverbSize, "Size", [] { return juce::String("Room size: longer decay when larger (Hall: about 0.3 s to 12 s)"); }),
         addFader(P::reverbDamping, "Damping", [] { return juce::String("High-frequency damping of the reflections: darker when higher"); }),
         addFader(P::reverbWidth, "Width", [] { return juce::String("Stereo width of the reverb: 0 = mono, 100 % = widest"); }),
         addFader(P::reverbPredelay, "Pre-delay", [] {
             return juce::String("A gap before the reverb starts (0-200 ms): voices and solo instruments stay clear");
         }),
         addFader(P::reverbMix, "Mix", [] { return juce::String("Dry/wet balance: 0 = off (no reverb at all), 100 % = only reverb"); })}, {}});
    effectGroups.push_back({"MAIN", 0, {}, {}, 1});
    for (const auto& grp : effectGroups)                     // Spec S7.5: COMB, DELAY and REVERB have presets
        if (const auto module = presetModuleOf(grp.title); module.isNotEmpty()) {
            auto& button = effectPresets[module];
            button = std::make_unique<PresetButton>(*this, module, grp.title);
            rackArea->addChildComponent(*button);
            effectComponents.push_back(button.get());
        }
    for (int e = 0; e < lmw::dsp::numEffects; ++e) {          // Spec S7.14: effect titles to move the effects
        effectHeaders.push_back(std::make_unique<EffectHeader>(*this, e));
        rackArea->addChildComponent(*effectHeaders.back());
        effectHeaders.back()->toBack();                      // under the preset buttons
        effectComponents.push_back(effectHeaders.back().get());
    }
    shownEffectOrder = processor.effectOrder();
    for (const auto& grp : effectGroups)
        for (const auto& item : grp.items)
            if (item.fader != nullptr) { effectComponents.push_back(&item.fader->slider); effectComponents.push_back(&item.fader->label);
                                         effectComponents.push_back(&item.fader->tag); }
            else for (auto* c : {item.choice, item.choice2, item.choice3})   // S7.15: REVERB Type
                if (c != nullptr) { effectComponents.push_back(&c->box); effectComponents.push_back(&c->label); }
    jassert(usedFaders == faders.size() && usedChoices == choices.size());

    // Spec S6.5, S6.16: rack. A row holds as many columns as fit on the widest screen (at least the widest
    // default row); panels can be moved into up to 4 rows. The layout is remembered.
    {
        std::vector<int> columns;
        int rowCount = 0;
        for (const auto& g : groups) rowCount = std::max(rowCount, g.row + 1);
        lmw::ui::RackLayout::Rows defaultRows(static_cast<std::size_t>(rowCount));
        for (std::size_t i = 0; i < groups.size(); ++i) {
            columns.push_back(groups[i].columns());
            defaultRows[static_cast<std::size_t>(groups[i].row)].push_back(static_cast<int>(i));
            headers.push_back(std::make_unique<PanelHeader>(*this, static_cast<int>(i)));
            rackArea->addAndMakeVisible(*headers.back());
        }
        // Spec S6.5 (user 2026-11-27, Phase 51b): the default order, by title. Only the default changes (Reset layout,
        // a first start); panel numbers, and so saved layouts, stay. A panel not named here keeps its group row.
        {
            const std::vector<std::vector<juce::String>> order{
                {"KEYS", "LFO 1", "NOISE", "VCO 1", "VCO 2", "VCO 3"},
                {"MIXER", "VCF 1", "VCF 2", "FORMANT"},
                {"MIX 2", "ENV 2", "LFO 2", "ENV 1", "VCA", "OUT"}};
            std::vector<bool> placed(groups.size(), false);
            lmw::ui::RackLayout::Rows rows(std::max(defaultRows.size(), order.size()));
            for (std::size_t r = 0; r < order.size(); ++r)
                for (const auto& title : order[r])
                    for (std::size_t i = 0; i < groups.size(); ++i)
                        if (!placed[i] && (groups[i].title == title || groups[i].title.startsWith(title + " "))) {
                            rows[r].push_back(static_cast<int>(i)); placed[i] = true; break;
                        }
            for (std::size_t i = 0; i < groups.size(); ++i)
                if (!placed[i]) rows[static_cast<std::size_t>(groups[i].row)].push_back(static_cast<int>(i));
            defaultRows = std::move(rows);
        }
        int maxColumns = 0;
        for (const auto& row : defaultRows) {
            int c = 0;
            for (int g : row) c += columns[static_cast<std::size_t>(g)];
            maxColumns = std::max(maxColumns, c);
        }
        defaultMaxColumns = maxColumns;
        // Compact columns are the narrowest: a layout saved in compact view is accepted; setCompact narrows it.
        rack.emplace(columns, defaultRows, std::max(maxColumns, screenColumns(compactMetrics)), kMaxRows);
        rack->parse(processor.state().state.getProperty(kLayoutProp).toString().toStdString());
    }
    // Spec S6.6: patch points per panel (since S5.15 noise.out and noise.rand are on the NOISE panel).
    {
        using lmw::dsp::Out;
        // `column`: the fader column the socket sits under; an input sits under the fader that sets
        // its amount (Spec S6.6). Sockets sharing a column split it evenly.
        struct PortDef { std::size_t group; int column; bool output; int port; const char* name; };
        const auto in  = [](In i)  { return static_cast<int>(i); };
        const auto out = [](Out o) { return static_cast<int>(o); };
        std::vector<PortDef> ports = {
            {keysPanel, 0, true, out(Out::KeysPitch), "Pitch"}, {keysPanel, 1, true, out(Out::KeysGate), "Gate"},
            {keysPanel, 1, true, out(Out::KeysVel), "Vel"},     {keysPanel, 1, true, out(Out::KeysMod), "Mod"},   // S6.24
            {mixPanel, 0, false, in(In::MixIn1), "In 1"},       {mixPanel, 1, false, in(In::MixIn2), "In 2"},
            {mixPanel, 2, false, in(In::MixIn3), "In 3"},       {mixPanel, 3, false, in(In::MixIn4), "In 4"},
            {mixPanel, 3, true, out(Out::Mix), "Out"},
            {noisePanel, 0, true, out(Out::Noise), "Out"},     {noisePanel, 2, true, out(Out::Rand), "Rand"},
            {vcfPanel[0], 0, false, in(In::Vcf1In), "In"},      {vcfPanel[0], 1, false, in(In::Vcf1Res), "Res"},
            {vcfPanel[0], 2, false, in(In::Vcf1Cutoff), "Cut"}, {vcfPanel[0], 4, true, out(Out::Vcf1), "Out"},
            {vcfPanel[1], 0, false, in(In::Vcf2In), "In"},      {vcfPanel[1], 1, false, in(In::Vcf2Res), "Res"},
            {vcfPanel[1], 2, false, in(In::Vcf2Cutoff), "Cut"}, {vcfPanel[1], 4, true, out(Out::Vcf2), "Out"},
            {vcaPanel, 0, false, in(In::VcaIn), "In"},          {vcaPanel, 0, false, in(In::VcaCv), "CV"},
            {vcaPanel, 1, false, in(In::VcaMod), "Mod"},
            {vcaPanel, 2, false, in(In::VcaVel), "Vel"},        {vcaPanel, 2, true, out(Out::Vca), "Out"},   // S5.22
            {lfoPanel, 0, false, in(In::Lfo1Shape), "Shape"},   // S5.28
            {lfoPanel, 1, false, in(In::Lfo1Rate), "Rate"},     {lfoPanel, 1, true, out(Out::Lfo1), "Out"},   // S5.19
            {lfo2Panel, 0, false, in(In::Lfo2Shape), "Shape"},
            {lfo2Panel, 1, false, in(In::Lfo2Rate), "Rate"},    {lfo2Panel, 1, true, out(Out::Lfo2), "Out"},
            {outPanel, 0, false, in(In::OutIn), "In"},          {outPanel, 1, false, in(In::OutPan), "Pan"},   // S5.25
            {env1Panel, 0, false, in(In::Env1Gate), "Gate"},    {env1Panel, 3, true, out(Out::Env1), "Out"},
            {env2Panel, 0, false, in(In::Env2Gate), "Gate"},    {env2Panel, 3, true, out(Out::Env2), "Out"},
            {formantPanel, 0, false, in(In::FormantIn), "In"}, {formantPanel, 1, false, in(In::FormantVowel), "Vowel"},
            {formantPanel, 2, false, in(In::FormantRes), "Res"},   // S5.29
            {formantPanel, 3, true, out(Out::Formant), "Out"},
            {mix2Panel, 0, false, in(In::Mix2In1), "In 1"},   {mix2Panel, 2, false, in(In::Mix2In2), "In 2"},   // S5.26
            {mix2Panel, 4, false, in(In::Mix2In3), "In 3"},   {mix2Panel, 5, true, out(Out::Mix2), "Out"},
        };
        for (int n = 0; n < 3; ++n) {                         // FM under FM, Pitch under Tune, Out last
            const auto g = vcoPanel[static_cast<std::size_t>(n)];
            ports.push_back({g, 0, false, in(lmw::dsp::vcoFmIn(n)), "FM"});
            ports.push_back({g, 1, false, in(lmw::dsp::vcoPmIn(n)), "PM"});      // Spec S5.20
            ports.push_back({g, 2, false, in(lmw::dsp::vcoPitchIn(n)), "Pitch"});
            ports.push_back({g, 3, false, in(lmw::dsp::vcoShapeIn(n)), "Shape"});   // Spec S5.28
            ports.push_back({g, 4, true, out(lmw::dsp::vcoOut(n)), "Out"});
        }
        groupJacks.resize(groups.size());
        for (const auto& d : ports) {
            jacks.push_back(std::make_unique<Jack>(*this, d.output, d.port, d.name));
            jacks.back()->column = d.column;
            groupJacks[d.group].push_back(jacks.back().get());
            rackArea->addAndMakeVisible(*jacks.back());
        }
        // Spec S7.6: sockets of the Effects tab (SYNTH, COMB, REVERB, MAIN); shown with that tab.
        const std::vector<PortDef> effectPorts = {
            {0, 0, true, out(Out::OutOut), "Out"},
            // Inputs on the left, outputs on the right (user 2026-11-22).
            {1, 0, false, in(In::CombIn), "In"}, {1, 4, true, out(Out::CombOut), "Out"},
            {2, 0, false, in(In::EnsembleIn), "In"}, {2, 3, true, out(Out::EnsembleOut), "Out"},   // S7.11
            {3, 0, false, in(In::DelayIn), "In"}, {3, 4, true, out(Out::DelayOut), "Out"},   // S7.8
            {4, 0, false, in(In::ReverbIn), "In"}, {4, 5, true, out(Out::ReverbOut), "Out"},   // S7.15: 6 columns
            {5, 0, false, in(In::MainIn), "In"},
        };
        effectGroupJacks.resize(effectGroups.size());
        for (const auto& d : effectPorts) {
            jacks.push_back(std::make_unique<Jack>(*this, d.output, d.port, d.name));
            jacks.back()->column = d.column;
            effectGroupJacks[d.group].push_back(jacks.back().get());
            rackArea->addChildComponent(*jacks.back());
            effectComponents.push_back(jacks.back().get());
        }
    }

    // Spec S7, S6.7, S6.8: Synth / Effects / Matrix / Schematic / Notes tabs, patch name, Load / Save.
    ++openEditors;
    juce::LookAndFeel::setDefaultLookAndFeel(&defaultLook.get());
    for (auto* tab : {&panelTab, &matrixTab, &schematicTab, &effectsTab, &notesTab}) {   // Spec S6.25: this order
        tab->setRadioGroupId(1);
        tab->setClickingTogglesState(true);
        addAndMakeVisible(*tab);
    }
    panelTab.setToggleState(true, juce::dontSendNotification);
    panelTab.setTooltip("The synthesizer: module panels and cables");
    effectsTab.setTooltip("Effects after the synthesizer: COMB, ENSEMBLE, DELAY, REVERB, and their matrix");
    notesTab.setTooltip("Free-text notes, saved with the patch");
    panelTab.onClick = [this] { if (panelTab.getToggleState()) showTab(0); };
    matrixTab.setTooltip("The synthesizer's patch as a pin matrix: every cable with its level (the effects' matrix is on the Effects tab)");
    effectsTab.onClick = [this] { if (effectsTab.getToggleState()) showTab(1); };
    matrixTab.onClick = [this] { if (matrixTab.getToggleState()) showTab(2); };
    notesTab.onClick = [this] { if (notesTab.getToggleState()) showTab(3); };
    schematicTab.setTooltip("The instrument as a schematic, with every connection");
    schematicTab.onClick = [this] { if (schematicTab.getToggleState()) showTab(4); };
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) {   // Spec S12.1: standalone only
        assistant = std::make_unique<AssistantView>(processor);
        addChildComponent(*assistant);
        assistantTab.setRadioGroupId(1);
        assistantTab.setClickingTogglesState(true);
        assistantTab.setTooltip("Ask an AI assistant about the synth, or describe a sound and let it make it");
        assistantTab.onClick = [this] { if (assistantTab.getToggleState()) showTab(5); };
        assistant->configure = [this](lmw::assistant::Settings& a) {           // Spec S12.6, S12.7
            a.key = assistantKey();
            a.model = settings.assistantModel.empty() ? lmw::assistant::defaultModel : settings.assistantModel;
            a.effort = settings.assistantEffort.empty() ? "medium" : settings.assistantEffort;
            a.limit = settings.assistantLimit.value_or(2.0);
            std::vector<lmw::assistant::Price> changed;                     // S12.7: price_<model> lines
            for (const auto& [model, text] : settings.assistantPrices)
                if (auto p = lmw::assistant::parsePrice(model, text)) changed.push_back(*p);
            a.prices = lmw::assistant::withPrices(changed);
        };
        assistant->openSettings = [this] { showAssistantSettings(); };
        assistant->needsNotice = [this] { return !settings.assistantNoticeSeen; };   // S12.8
        assistant->noticeAccepted = [this] { settings.assistantNoticeSeen = true; saveConfig(); };
        assistant->ready();
        addAndMakeVisible(assistantTab);
    }
    patchLabel.setTooltip("Patch name (the file name when saved or loaded)");
    loadLabel.setJustificationType(juce::Justification::centred);   // Spec S6.28
    loadLabel.setTitle("Load");
    addAndMakeVisible(loadLabel);
    // Spec S10.5: where the 9P server of this instance listens.
    titleButton.setTooltip("About LMW-Synth (click): version, licence, source.\n" + (processor.ninepEndpoints().empty()
        ? juce::String("9P server: not available")
        : "9P server (connect with a 9P client, e.g. 9p -a " + juce::String(processor.ninepEndpoints()).upToFirstOccurrenceOf(" ", false, false)
              + " ls):\n" + juce::String(processor.ninepEndpoints()).replace(" ", "\n")));
    titleButton.setButtonText("LMW-Synth " LMW_VERSION);  // Spec S6.10
    titleButton.setTitle("About LMW-Synth " LMW_VERSION);
    titleButton.onClick = [this] { showAbout(); };
    addAndMakeVisible(titleButton);
    addAndMakeVisible(patchLabel);
    menuButton.setTooltip("About LMW-Synth, contrast, and more (Spec S6.11)");
    menuButton.onClick = [this] { showMainMenu(); };
    addAndMakeVisible(menuButton);
    // Spec S6.14 (user 2026-11-24: "undo / redo should be directly available in the top menu"): two header
    // buttons left of Menu, greyed when there is nothing; the hover text names the step.
    undoButton.setButtonText(juce::String::fromUTF8("\xe2\x86\xb6 Undo"));
    redoButton.setButtonText(juce::String::fromUTF8("Redo \xe2\x86\xb7"));
    undoButton.onClick = [this] { undoStep(); updateUndoButtons(); };
    redoButton.onClick = [this] { redoStep(); updateUndoButtons(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    updateUndoButtons();
    loadButton.setTooltip("Load a patch file (parameters, cables, notes)");
    saveButton.setTooltip("Save parameters, cables and notes as a patch file");
    loadButton.onClick = [this] { loadPatch(); };
    instrumentButton.setTooltip("Whole-instrument presets: a complete sound (all modules, voices, effects, cables).\n"
                                "Your notes and the schematic layout stay.");
    instrumentButton.onClick = [this] { showInstrumentMenu(); };
    addAndMakeVisible(instrumentButton);
    saveButton.onClick = [this] { savePatch(); };
    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    notesEditor.setMultiLine(true, true);
    notesEditor.setReturnKeyStartsNewLine(true);
    notesEditor.setTabKeyUsedAsCharacter(false);
    notesEditor.setScrollbarsShown(true);
    notesEditor.setTitle("Patch notes");
    notesEditor.setTextToShowWhenEmpty("Notes for this patch: they are saved with it.", kFg);   // again in applyContrast (S6.29)
    notesEditor.onTextChange = [this] { processor.setNotes(notesEditor.getText()); };
    addChildComponent(notesEditor);
    // Spec S6.8: the Matrix tab edits the same patch as the cables (through connect(), which reports
    // an input that is full).
    matrix = std::make_unique<MatrixView>([this]() -> const lmw::dsp::Patch& { return processor.currentPatch(); },
                                          [this](int in, int out, float level) { return connect(in, out, level); });
    matrix->setPalette(palette);                            // unit colours in the headers (S6.9)
    matrix->onLevelDrag = [this](bool started) {            // Spec S6.14: a level drag is one step
        if (started) processor.beginStep("cable level");
        else processor.endStep();
    };
    addChildComponent(*matrix);
    // Spec S6.25: the effects' inputs and outputs in their own small matrix on the Effects tab (in the effects order).
    fxMatrix = std::make_unique<MatrixView>([this]() -> const lmw::dsp::Patch& { return processor.currentPatch(); },
                                            [this](int in, int out, float level) { return connect(in, out, level); });
    fxMatrix->setPorts(MatrixView::effectInputs(shownEffectOrder), MatrixView::effectOutputs(shownEffectOrder));
    fxMatrix->setTitle("Effects matrix");
    fxMatrix->setPalette(palette);
    fxMatrix->onLevelDrag = matrix->onLevelDrag;
    rackArea->addChildComponent(*fxMatrix);
    effectComponents.push_back(fxMatrix.get());
    rackArea->addAndMakeVisible(outLamp);                 // Spec S5.31: the limiter lamps (OUT on the Synth tab,
    rackArea->addAndMakeVisible(mainLamp);                // MAIN on the Effects tab)
    effectComponents.push_back(&mainLamp);
    schematic = std::make_unique<SchematicView>([this]() -> const lmw::dsp::Patch& { return processor.currentPatch(); },
        [this] {                                           // VCF Key faders (key-tracking links), LFO Delays (S5.24)
            auto& s = processor.state();
            return std::array<float, 4>{s.getRawParameterValue(lmw::param::vcfKey[0])->load(),
                                        s.getRawParameterValue(lmw::param::vcfKey[1])->load(),
                                        s.getRawParameterValue(lmw::param::lfoDelay)->load(),
                                        s.getRawParameterValue(lmw::param::lfo2Delay)->load()};
        },
        [this](int in, int out, float level) { return connect(in, out, level); },   // Phase 12b: editing
        [this](bool output, int port, juce::Rectangle<int> area) { showPortMenu(output, port, area); });
    schematic->setPalette(palette);
    schematic->setRings(settings.socketRings);           // Spec S6.20
    schematic->setLayout(processor.schematicLayout());     // Phase 12c: moved symbols, saved with the state
    schematic->onLayoutChange = [this](const juce::String& text) {
        const LMWSynthProcessor::ScopedStep step(processor, "Schematic layout");   // Spec S6.14
        processor.setSchematicLayout(text);
    };
    schematicView.setViewedComponent(schematic.get(), false);   // Spec S6.22
    schematicView.setScrollBarThickness(10);
    for (auto* bar : {&schematicView.getVerticalScrollBar(), &schematicView.getHorizontalScrollBar()}) {
        bar->setColour(juce::ScrollBar::thumbColourId, kFg);
        bar->setColour(juce::ScrollBar::trackColourId, kBg);
    }
    schematic->onSizeChange = [this] { layoutSchematic(); };
    updateUse(true);                                       // Spec S6.26: before the first paint
    addChildComponent(schematicView);
    for (auto* c : effectComponents) c->setVisible(false);  // Effects tab not shown at start

    resetLayoutButton.setTooltip("Put all module panels back in their original places "
                                 "(on the Schematic tab: all symbols; on the Effects tab: the effects order COMB, ENSEMBLE, DELAY, REVERB)");
    resetLayoutButton.onClick = [this] {
        if (currentTab == 4) { schematic->resetLayout(); return; }   // Spec S6.7: the schematic's blocks
        if (currentTab == 1) { resetEffectOrder(); return; }         // Spec S7.14 (user 2026-11-26): the effects order
        rack->reset();
        layoutChanged(-1);
    };
    addAndMakeVisible(resetLayoutButton);

    // Spec S6.4: compact view toggle, remembered in the saved state.
    compactButton.setClickingTogglesState(true);
    compactButton.setTooltip("Smaller margins, faders and value boxes");
    compactButton.onClick = [this] {
        processor.state().state.setProperty(kCompactProp, compactButton.getToggleState(), nullptr);
        settings.compact = compactButton.getToggleState();   // default for new windows (config file)
        saveConfig();
        setCompact(compactButton.getToggleState());
    };
    addAndMakeVisible(compactButton);


    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    // Spec S3.2, S6.1 (user 2026-11-13): scientific pitch names (MIDI 60 = C4, JUCE's default is C3),
    // the QWERTY 'a' plays C4 = MIDI 60 (JUCE's default octave 6 played MIDI 72), and the on-screen keys
    // are the piano range A0-C8 (MIDI 21-108); MIDI from a controller still plays all 128 notes.
    keyboard.setOctaveForMiddleC(4);
    keyboard.setKeyPressBaseOctave(5);
    keyboard.setAvailableRange(21, 108);
    addAndMakeVisible(keyboard);
    // Spec S6.12: hide / show the keyboard (also Menu and Ctrl+K); computer keys and MIDI keep playing.
    keyboardButton.setTooltip("Hide or show the on-screen keyboard (Ctrl+K). The computer keys keep playing.");
    keyboardButton.onClick = [this] { setKeyboardShown(!keyboardShown); };
    addAndMakeVisible(keyboardButton);
    setWantsKeyboardFocus(true);                 // the computer keys when the keyboard is hidden

    // Child widgets (e.g. slider value boxes) take their colours when created, before they had this
    // look-and-feel; re-send it so every child uses the black-on-white theme (Spec S6.2).
    sendLookAndFeelChange();

    // A saved state's view wins; a new window starts with the config file's default.
    const bool compact = processor.state().state.getProperty(kCompactProp, settings.compact.value_or(false));
    compactButton.setToggleState(compact, juce::dontSendNotification);
    keyboardShown = processor.state().state.getProperty(kKeyboardProp, settings.keyboard.value_or(true));
    keyboard.setVisible(keyboardShown);
    keyboardButton.setButtonText(juce::String::fromUTF8(keyboardShown ? "\xe2\x96\xbe keyboard" : "\xe2\x96\xb4 keyboard"));
    // Spec S6.16: resizable (the standalone window by its edges, a DAW's window by the corner), never below
    // the size the rack needs.
    setResizable(true, !standalone());
    setCompact(compact);                         // sets the window size

    cables = std::make_unique<CableLayer>(*this);   // last child of the rack: drawn above the panels
    rackArea->addAndMakeVisible(*cables);
    cables->setBounds(rackArea->getLocalBounds());
    shownPatchVersion = p.patchVersion();

    refreshPatchInfo();                          // patch name and notes from the saved state
    p.resetKeyboardDisplay();
    updatePresetLabels();                        // Spec S7.5: the title-bar preset labels from the start
    startTimerHz(30);                            // show external MIDI notes on the keyboard
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::Component>(&keyboard)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

LMWSynthEditor::~LMWSynthEditor() {
    stopTimer();
    juce::Desktop::getInstance().removeFocusChangeListener(this);
    if (auto* w = colourWindow.getComponent()) { w->setLookAndFeel(nullptr); delete w; }   // uses our look-and-feel
    for (auto* w : {assistantWindow.getComponent(), lessonWindow.getComponent()})
        if (w != nullptr) { w->setLookAndFeel(nullptr); delete w; }
    chooser.reset();
    setLookAndFeel(nullptr);
    if (--openEditors == 0 && &juce::LookAndFeel::getDefaultLookAndFeel() == &defaultLook.get())
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);   // the last LMW editor is closing
}

// Spec S6.14: the header's Undo / Redo follow the history (also changes made over 9P or by the Assistant).
void LMWSynthEditor::updateUndoButtons() {
    const auto& h = processor.steps();
    undoButton.setEnabled(h.canUndo());
    redoButton.setEnabled(h.canRedo());
    const auto key = [](bool shift) {
        return juce::KeyPress('z', juce::ModifierKeys::commandModifier | (shift ? juce::ModifierKeys::shiftModifier : 0), 0).getTextDescription();
    };
    undoButton.setTooltip(h.canUndo() ? "Undo " + juce::String::fromUTF8(h.undoName().c_str()) + " (" + key(false) + ")"
                                      : juce::String("Nothing to undo"));
    redoButton.setTooltip(h.canRedo() ? "Redo " + juce::String::fromUTF8(h.redoName().c_str()) + " (" + key(true) + ")"
                                      : juce::String("Nothing to redo"));
}

// Spec S5.31: "limit" and a dot, in the text colour: filled while lit, an outline otherwise.
void LMWSynthEditor::LimitLamp::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    const float d = std::min(b.getHeight() - 4.0f, 11.0f);
    auto dot = b.removeFromRight(d + 2.0f).withSizeKeepingCentre(d, d);
    g.setColour(lmw::app::kFg);
    g.setFont(lmw::app::smallFont(std::max(9.0f, b.getHeight() * 0.55f)));
    g.drawText("limit", b.reduced(1.0f, 0.0f), juce::Justification::centredRight, false);
    if (lit) g.fillEllipse(dot);
    else g.drawEllipse(dot.reduced(0.75f), 1.5f);
}

void LMWSynthEditor::timerCallback() {
    processor.updateKeyboardFromMidiInput();
    // Spec S6.24: the wheels follow MIDI (and 9P) while they are not being dragged.
    if (!bendWheel.isMouseButtonDown()) bendWheel.setValue(processor.pitchBend(), juce::dontSendNotification);
    if (!modWheelCtl.isMouseButtonDown()) modWheelCtl.setValue(processor.modWheel(), juce::dontSendNotification);
    keyboard.followHeldKeys();                           // Spec S3.2: held computer keys across focus changes
    updateUse();                                         // Spec S6.26
    {                                                    // Spec S5.31: the limiter lamps
        const auto o = processor.outLamp(), mn = processor.mainLamp();
        outLamp.show(o.lit, o.db);
        mainLamp.show(mn.lit, mn.db);
    }
    if (const int o = processor.effectOrder(); o != shownEffectOrder) {   // Spec S7.14: moved here, over 9P, undo, a patch
        shownEffectOrder = o;
        resized();
        repaint();
    }
    if (processor.patchFileVersion() != shownPatchFile || (!notesEditor.hasKeyboardFocus(true)
            && notesEditor.getText() != processor.notes()))   // patch loaded, or state restored by the host
        refreshPatchInfo();
    if (schematic) schematic->poll(processor.schematicLayout());   // Key faders; layout from a loaded patch
    if (++presetLabelTick >= 8) { presetLabelTick = 0; updatePresetLabels(); updatePatchLabel(); updateUndoButtons(); updateLoadLabel(); }   // S7.5, S7.7, S6.28: ~4/s
    if (++systemThemeTick >= 30) {                       // Spec S6.29: Follow system, about once a second
        systemThemeTick = 0;
        if (settings.theme == lmw::ui::theme::ThemePreset::System && juce::Desktop::getInstance().isDarkModeActive() != shownOsDark)
            applyContrast();
    }
    if (processor.patchVersion() != shownPatchVersion || processor.vcfRouting() != shownRouting) {   // cables or routing changed
        shownPatchVersion = processor.patchVersion();
        shownRouting = processor.vcfRouting();
        if (schematic) schematic->repaint();
        if (matrix) matrix->refresh();                        // its level slider (S6.8)
        if (fxMatrix) fxMatrix->refresh();                    // S6.25
        repaint();
    }
}

// A closed popup menu leaves keyboard focus nowhere, so computer-keyboard notes (S3.2) went silent until the
// on-screen keyboard was clicked (user 2026-11-21). Focus goes back to what had it, else to the keyboard.
juce::Component::SafePointer<juce::Component> LMWSynthEditor::focusNow() {
    return juce::Component::SafePointer<juce::Component>(juce::Component::getCurrentlyFocusedComponent());
}
void LMWSynthEditor::restoreFocus(juce::Component::SafePointer<juce::Component> c) {
    if (c != nullptr && c->isShowing()) c->grabKeyboardFocus();
    else if (keyboard.isShowing()) keyboard.grabKeyboardFocus();
}

// Spec S3.2: computer-key notes across focus changes inside LMW-Synth (see PluginEditor.h).
bool LMWSynthEditor::Keyboard::anyNoteKeyDown() {
    for (const char c : std::string_view("awsedftgyhujkolp;"))   // JUCE's default note keys
        if (juce::KeyPress(c).isCurrentlyDown()) return true;
    return false;
}

bool LMWSynthEditor::Keyboard::keyStateChanged(bool isKeyDown) {
    const bool used = juce::MidiKeyboardComponent::keyStateChanged(isKeyDown);
    held = anyNoteKeyDown();
    return used;
}

void LMWSynthEditor::Keyboard::focusLost(FocusChangeType cause) {
    const bool typing = dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr;
    if (held && !typing && juce::Process::isForegroundProcess()) return;   // a button or menu of ours: keep playing
    held = false;
    juce::MidiKeyboardComponent::focusLost(cause);       // releases the notes (JUCE)
}

void LMWSynthEditor::Keyboard::followHeldKeys() {
    if (!held || hasKeyboardFocus(false)) return;
    const bool typing = dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr;
    if (typing || !juce::Process::isForegroundProcess()) {
        held = false;
        juce::MidiKeyboardComponent::focusLost(focusChangedDirectly);   // release them all
        return;
    }
    keyStateChanged(false);                               // a key released while the focus was elsewhere
}

bool LMWSynthEditor::isTypingText() {
    return dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr;
}

bool LMWSynthEditor::keyPressed(const juce::KeyPress& key) {
    if (!isTypingText()) {                                    // Spec S6.14; text boxes keep their own undo
        const auto mods = key.getModifiers();
        const int code = key.getKeyCode();
        const bool command = mods.isCommandDown() && !mods.isAltDown();
        if (command && (code == 'z' || code == 'Z') && !mods.isShiftDown()) { undoStep(); return true; }
        if (command && (((code == 'z' || code == 'Z') && mods.isShiftDown()) || ((code == 'y' || code == 'Y') && !mods.isShiftDown()))) {
            redoStep();
            return true;
        }
    }
    if (key == juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0)) {   // Spec S6.12: Ctrl+K (Cmd+K)
        setKeyboardShown(!keyboardShown);
        return true;
    }
    // Spec S6.21: Home / End show the top-left / bottom-right of the rack (keys a control does not use itself).
    if (!isTypingText() && rackView.isVisible() && !key.getModifiers().isAnyModifierKeyDown()
        && (key.isKeyCode(juce::KeyPress::homeKey) || key.isKeyCode(juce::KeyPress::endKey))) {
        rackView.setViewPosition(key.isKeyCode(juce::KeyPress::homeKey) ? juce::Point<int>()
                                 : juce::Point<int>(rackArea->getWidth(), rackArea->getHeight()));
        return true;
    }
    return !isTypingText() && keyboard.keyPressed(key);
}

void LMWSynthEditor::setKeyboardShown(bool shown) {
    const bool hadFocus = keyboard.hasKeyboardFocus(true);
    keyboardShown = shown;
    processor.state().state.setProperty(kKeyboardProp, shown, nullptr);
    settings.keyboard = shown;                    // default for new windows (config file)
    saveConfig();
    keyboard.setVisible(shown);
    keyboardButton.setButtonText(juce::String::fromUTF8(shown ? "\xe2\x96\xbe keyboard" : "\xe2\x96\xb4 keyboard"));
    updateSize(!standalone());
    if (hadFocus || !shown) grabKeyboardFocus();  // the computer keys keep playing
    if (shown && hadFocus) keyboard.grabKeyboardFocus();
}

bool LMWSynthEditor::keyStateChanged(bool isKeyDown) {
    return !isTypingText() && keyboard.keyStateChanged(isKeyDown);
}

// Spec S5.25 (2026-11-26): with a mono output (one active output channel in the audio settings, or a mono track
// in a DAW) the host gives one channel and there is nothing to pan: Pan and Spread say so in their hover text.
// Spec S6.20: show or hide the fader scale marks and the socket rings (the layout stays; hover texts keep them).
void LMWSynthEditor::applyAids() {
    for (auto& f : faders) {
        f.slider.showTicks = settings.showScales;
        f.tag.setText(settings.showScales ? f.tagText : juce::String(), juce::dontSendNotification);
        f.slider.repaint();
    }
    if (schematic) schematic->setRings(settings.socketRings);
    for (auto& j : jacks) j->repaint();
    repaint();
}

juce::String LMWSynthEditor::monoNote() const {
    return processor.getMainBusNumOutputChannels() < 2
        ? juce::String("\nThe output is mono now (one output channel in the audio settings or the host): Pan and Spread do nothing.")
        : juce::String();
}

// Spec S5.24: the LFO 1 / LFO 2 Delay faders, for the hover notes (Schematic.h lfoDelayNote).
lmw::ui::schematic::LfoDelays LMWSynthEditor::lfoDelays() const {
    auto& s = processor.state();
    return {s.getRawParameterValue(lmw::param::lfoDelay)->load(), s.getRawParameterValue(lmw::param::lfo2Delay)->load()};
}

juce::String LMWSynthEditor::describeInput(lmw::dsp::In in) const {
    const auto delayNote = lmw::ui::schematic::lfoDelayNote(processor.currentPatch(), in, lfoDelays());
    const auto text = describeInputOnly(in);
    return delayNote.empty() ? text : text + "\n" + juce::String::fromUTF8(delayNote.c_str());
}

juce::String LMWSynthEditor::describeInputOnly(lmw::dsp::In in) const {
    const auto& patch = processor.currentPatch();
    const auto idx = [](auto e) { return static_cast<std::size_t>(e); };
    const juce::String inName(lmw::dsp::inputNames[idx(in)].data(), lmw::dsp::inputNames[idx(in)].size());
    const juce::String arrow = juce::String::fromUTF8("\xe2\x86\x92");
    // Signal flow reads left to right: source → input (Spec S6.1). Several cables: "a + b (50 %) → in".
    if (!patch.isPatched(in)) {
        const auto ns = patch.normals(in);
        if (ns.n == 0 || ns.s[0].source == lmw::dsp::noSource) return inName + ": not connected (reads 0); patch a cable into it";
        juce::StringArray srcs;                               // S5.17: two sources at 50 % each in Parallel
        for (int k = 0; k < ns.n; ++k) {
            const auto& x = ns.s[static_cast<std::size_t>(k)];
            srcs.add(juce::String(lmw::dsp::outputNames[idx(x.source)].data(), lmw::dsp::outputNames[idx(x.source)].size())
                     + (x.level != 1.0f ? " (" + juce::String(juce::roundToInt(x.level * 100.0f)) + " %)" : juce::String()));
        }
        return srcs.joinIntoString(" + ") + " " + arrow + " " + inName + " (normalled)";
    }
    juce::StringArray sources;
    for (int k = 0; k < patch.pinCount(in); ++k) {
        const auto& pin = patch.pin(in, k);
        const auto n = lmw::dsp::outputNames[static_cast<std::size_t>(pin.source)];
        sources.add(juce::String(n.data(), n.size())
                    + (pin.level != 1.0f ? " (" + juce::String(juce::roundToInt(pin.level * 100.0f)) + " %)" : juce::String()));
    }
    const auto replaced = lmw::dsp::replacedNote(patch, in);   // Spec S6.27
    return sources.joinIntoString(" + ") + " " + arrow + " " + inName + (patch.pinCount(in) > 1 ? " (cables, summed)" : " (cable)")
         + (replaced.empty() ? juce::String() : "\n" + juce::String::fromUTF8(replaced.c_str()));
}

LMWSynthEditor::Item LMWSynthEditor::addFader(const char* paramId, const juce::String& text,
                                              std::function<juce::String()> tip) {
    if (usedFaders >= faders.size()) { std::fprintf(stderr, "LMW-Synth: faders[] too small\n"); std::abort(); }   // found 2026-11-27
    Fader& k = faders[usedFaders++];
    k.slider.setSliderSnapsToMousePosition(false);   // drag moves relative; a click does not jump the value
    // Spec S6.1, S6.17: every fader's hover text ends with its scale (linear, linear around 0, logarithmic or
    // curved, with the value at half way); ticks beside the track and a tag under the name show it too.
    juce::String scale;
    if (const auto s = processor.scaleOf(paramId)) {
        namespace fs = lmw::ui::scale;
        std::optional<double> centre;
        if (s->centre) centre = *s->centre;
        const auto* p = processor.state().getParameter(paramId);
        scale = juce::String(fs::describe(s->kind, p->getText(0.5f, 32).toStdString()));
        k.slider.ticks = fs::ticks(s->kind, s->unit, s->lo, s->hi, centre);
        k.tagText = juce::String::fromUTF8(fs::tag(s->kind).data(), static_cast<int>(fs::tag(s->kind).size()));
        k.tag.setText(settings.showScales ? k.tagText : juce::String(), juce::dontSendNotification);   // S6.20
        k.slider.showTicks = settings.showScales;
    }
    k.slider.tip = [inner = std::move(tip), scale] {
        const auto t = inner ? inner() : juce::String();
        return t.isEmpty() ? scale : scale.isEmpty() ? t : t + "\n" + scale;
    };
    k.attachment = std::make_unique<SliderAttachment>(processor.state(), paramId, k.slider);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.tag.setJustificationType(juce::Justification::centred);
    k.tag.setInterceptsMouseClicks(false, false);
    rackArea->addAndMakeVisible(k.slider);
    rackArea->addAndMakeVisible(k.label);
    rackArea->addAndMakeVisible(k.tag);
    return {&k, nullptr};
}

LMWSynthEditor::Item LMWSynthEditor::addChoice(const char* paramId, const juce::String& text,
                                               const juce::StringArray& options, const juce::String& tip) {
    if (usedChoices >= choices.size()) { std::fprintf(stderr, "LMW-Synth: choices[] too small\n"); std::abort(); }
    Choice& c = choices[usedChoices++];
    c.box.addItemList(options, 1);
    c.box.setTooltip(tip);
    c.attachment = std::make_unique<ComboAttachment>(processor.state(), paramId, c.box);
    c.label.setText(text, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    rackArea->addAndMakeVisible(c.box);
    rackArea->addAndMakeVisible(c.label);
    return {nullptr, &c, nullptr};
}

void LMWSynthEditor::setCompact(bool compact) {
    m = compact ? compactMetrics : normalMetrics;
    if (routingChoice != nullptr) {                  // Spec S5.17: short texts where the box is narrow
        routingChoice->box.changeItemText(1, compact ? "Ser" : "Serial");
        routingChoice->box.changeItemText(2, compact ? "Par" : "Parallel");
        // The box shows the old text until reselected; the selection comes from the parameter.
        const int id = processor.vcfRouting() == lmw::dsp::VcfRouting::Parallel ? 2 : 1;
        routingChoice->box.setSelectedId(0, juce::dontSendNotification);
        routingChoice->box.setSelectedId(id, juce::dontSendNotification);
    }
    for (std::size_t i = 0; i < usedFaders; ++i) {
        faders[i].slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, m.boxW, m.boxH);
        faders[i].label.setFont(juce::FontOptions(m.labelFont));
        faders[i].tag.setFont(lmw::app::smallFont(static_cast<float>(tagH()) - 1.0f));   // Spec S6.17, S6.29
        faders[i].tag.setBorderSize({});
        faders[i].slider.tickLabels = m.colW >= 80;              // value texts only in normal view
    }
    for (std::size_t i = 0; i < usedChoices; ++i) choices[i].label.setFont(juce::FontOptions(m.labelFont));
    notesEditor.applyFontToAllText(juce::FontOptions(m.labelFont + 3.0f));   // Spec S7: notes, readable size
    notesEditor.setFont(juce::FontOptions(m.labelFont + 3.0f));
    if (rack) rack->setMaxColumns(std::max(defaultMaxColumns, screenColumns(m)));
    if (schematic) schematic->setCompact(compact);         // Spec S6.7: smaller in Compact too
    updateSize(!standalone());                   // S6.16: the standalone keeps its size, a plugin fits the view
}

bool LMWSynthEditor::standalone() const {
    return processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
}

int LMWSynthEditor::screenColumns(const Metrics& metrics) const {
    int widest = 0;
    for (const auto& d : juce::Desktop::getInstance().getDisplays().displays)
        widest = std::max(widest, d.userBounds.toNearestInt().getWidth());
    // Room for the margins and up to six gaps between the panels of a row.
    return widest > 0 ? (widest - 2 * (metrics.gap + 2) - 6 * metrics.gap) / metrics.colW : 0;
}

void LMWSynthEditor::updateSize(bool shrink) {
    // Width of the widest row, at least that of the default layout, so moving panels never makes
    // the window narrower (Spec S6.5).
    int width = 0;
    for (const auto* rows : {&rack->rows(), &rack->defaultRows()})
        for (const auto& row : *rows)
            width = std::max(width, rack->rowColumns(row) * m.colW + (static_cast<int>(row.size()) - 1) * m.gap);
    const int rows = static_cast<int>(rack->rows().size());
    const int pad = m.gap + 2;
    // title, gap, rows with gaps between, gap, keyboard
    // Spec S6.21: the size that shows the whole rack (a first window, "shrink"); the window may be smaller
    // (down to 1280 x 720): the rack then scrolls in its view.
    const int fitW = pad + width + pad;
    const int fitH = pad + m.titleH + m.gap + rows * m.rowH + (rows - 1) * m.gap + m.gap + keyStripH()
                   + (keyboardShown ? m.keysH : 0) + pad;
    const int minW = std::min(fitW, kSmallestW), minH = std::min(fitH, kSmallestH);
    setResizeLimits(minW, minH, 32768, 32768);
   #if JUCE_LINUX
    // The standalone window (S6.16) takes its limits from these (JUCE's decorator constrainer), but on Linux the
    // window manager hears of a new minimum only with new bounds (equal bounds are not sent): when the
    // minimum changed, resize by one pixel and back.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != static_cast<juce::Component*>(this)
                                            && juce::Point<int>(minW, minH) != sentMinimum && top->getPeer() != nullptr) {
        sentMinimum = {minW, minH};
        const auto b = top->getBounds();
        top->setBounds(b.withHeight(b.getHeight() + 1));
        top->setBounds(b);
    }
   #endif
    const bool first = getWidth() == 0 || getHeight() == 0;
    setSize(shrink || first ? fitW : std::max(getWidth(), minW), shrink || first ? fitH : std::max(getHeight(), minH));
    resized();
    repaint();
}

// ---- Spec S6.5: rack (movable module panels) ----

LMWSynthEditor::PanelHeader::PanelHeader(LMWSynthEditor& e, int panelIndex) : editor(e), panel(panelIndex) {
    const auto& title = editor.groups[static_cast<std::size_t>(panel)].title;
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTitle(title + " panel");
    setDescription("Drag to move. Arrow keys move the panel; Enter opens the move menu.");
    baseTip = "Drag to move this panel.\nArrow keys (when selected) or right-click: move menu.";
    setTooltip(baseTip);
    const auto module = presetModuleOf(title);
    if (module.isNotEmpty()) {
        presets = std::make_unique<PresetButton>(editor, module, title);
        addAndMakeVisible(*presets);
    }
}

void LMWSynthEditor::PanelHeader::resized() {
    if (presets) presets->setBounds(getLocalBounds().withTrimmedBottom(1).removeFromRight(presets->preferredWidth()).reduced(2));
}

void LMWSynthEditor::PanelHeader::paint(juce::Graphics& g) {
    // Spec S6.9: the title bar in the unit's colour (raised to the contrast preset; white at Max).
    const auto& title = editor.groups[static_cast<std::size_t>(panel)].title;
    g.fillAll(toJuce(editor.palette.unitColour(lmw::ui::theme::unitOf(title.toStdString()))));
    g.setColour(kFg);
    if (editor.draggedPanel == panel) g.drawRect(getLocalBounds(), 2);   // keep the thick drag frame visible
    const juce::FontOptions titleFont(editor.m.titleFont, juce::Font::bold);
    g.setFont(titleFont);
    g.drawText(title, getLocalBounds(), juce::Justification::centred);
    // Spec S7.5: the module's preset ("Flute", "Flute*" when changed since) in the space left of the title.
    if (const auto label = editor.presetLabel(presetModuleOf(title)); label.isNotEmpty()) {
        const int titleWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont, title));
        const int room = (getWidth() - titleWidth) / 2 - 10;
        if (room > 16) {
            g.setFont(juce::FontOptions(editor.m.titleFont * 0.85f));
            g.drawText(label, juce::Rectangle<int>(5, 0, room, getHeight() - 1), juce::Justification::centredLeft, true);
        }
    }
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
    if (hasKeyboardFocus(false)) g.drawRect(getLocalBounds().withTrimmedBottom(1), 2);   // focus ring
}

void LMWSynthEditor::PanelHeader::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) editor.showPanelMenu(panel);
}

void LMWSynthEditor::PanelHeader::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || (!dragging && e.getDistanceFromDragStart() < 4)) return;
    dragging = true;
    editor.autoScrollRack(e.getEventRelativeTo(editor.rackArea.get()).getPosition());   // S6.21
    editor.dragPanel(panel, e.getEventRelativeTo(editor.rackArea.get()).getPosition());
}

void LMWSynthEditor::PanelHeader::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.dropPanel(panel, e.getEventRelativeTo(editor.rackArea.get()).getPosition());
}

bool LMWSynthEditor::PanelHeader::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && dragging) { dragging = false; editor.cancelDrag(); return true; }
    if (key.getModifiers().isAnyModifierKeyDown() && !key.getModifiers().isShiftDown()) return false;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey)  { editor.movePanel(panel, 0); return true; }
    if (code == juce::KeyPress::rightKey) { editor.movePanel(panel, 1); return true; }
    if (code == juce::KeyPress::upKey)    { editor.movePanel(panel, 2); return true; }
    if (code == juce::KeyPress::downKey)  { editor.movePanel(panel, 3); return true; }
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())) {
        editor.showPanelMenu(panel);
        return true;
    }
    return false;                        // e.g. QWERTY note keys: handled by the editor
}

// Spec S7.14: moving the effects.
LMWSynthEditor::EffectHeader::EffectHeader(LMWSynthEditor& e, int effectIndex) : editor(e), effect(effectIndex) {
    const auto& title = editor.effectGroups[static_cast<std::size_t>(effect + 1)].title;
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    setTitle(title + " panel");
    setDescription("Drag left or right to move the effect in the chain. Left and Right arrow keys move it; Enter opens the move menu.");
    baseTip = "Drag left or right to move " + title + " in the effects chain (the connections follow).\n"
              "Arrow keys (when selected) or right-click: move menu.";
    setTooltip(baseTip);
}

void LMWSynthEditor::EffectHeader::paint(juce::Graphics& g) {   // the tab draws the title; here only the frames
    g.setColour(kFg);
    if (editor.draggedEffect == effect || hasKeyboardFocus(false)) g.drawRect(getLocalBounds(), 2);
}

void LMWSynthEditor::EffectHeader::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) editor.showEffectMenu(effect);
}

void LMWSynthEditor::EffectHeader::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || (!dragging && e.getDistanceFromDragStart() < 4)) return;
    dragging = true;
    editor.autoScrollRack(e.getEventRelativeTo(editor.rackArea.get()).getPosition());
    editor.dragEffect(effect, e.getEventRelativeTo(editor.rackArea.get()).getPosition());
}

void LMWSynthEditor::EffectHeader::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.dropEffect(effect, e.getEventRelativeTo(editor.rackArea.get()).getPosition());
}

bool LMWSynthEditor::EffectHeader::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && dragging) {
        dragging = false;
        editor.draggedEffect = -1;
        editor.effectDrop.reset();
        editor.rackArea->repaint();
        repaint();
        return true;
    }
    if (key.getModifiers().isAnyModifierKeyDown() && !key.getModifiers().isShiftDown()) return false;
    const int code = key.getKeyCode();
    const int at = editor.effectPosition(effect);
    if (code == juce::KeyPress::leftKey)  { editor.moveEffect(effect, at - 1); return true; }
    if (code == juce::KeyPress::rightKey) { editor.moveEffect(effect, at + 1); return true; }
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())) {
        editor.showEffectMenu(effect);
        return true;
    }
    return false;                        // e.g. QWERTY note keys: handled by the editor
}

std::vector<int> LMWSynthEditor::effectPanelOrder() const {
    std::vector<int> v{0};                                   // SYNTH
    for (const int e : lmw::dsp::effectOrder(shownEffectOrder)) v.push_back(e + 1);
    v.push_back(lmw::dsp::numEffects + 1);                    // MAIN
    return v;
}

int LMWSynthEditor::effectPosition(int effect) const {
    const auto o = lmw::dsp::effectOrder(shownEffectOrder);
    for (int p = 0; p < lmw::dsp::numEffects; ++p) if (o[static_cast<std::size_t>(p)] == effect) return p;
    return 0;
}

// The position the dragged effect would take at `where` (rack coordinates): before the first other effect whose
// middle is right of it in its row (a later row counts as right); nothing when that is where it is.
std::optional<int> LMWSynthEditor::findEffectDrop(int effect, juce::Point<int> where, juce::Rectangle<int>* marker) const {
    const auto order = lmw::dsp::effectOrder(shownEffectOrder);
    int position = 0;
    juce::Rectangle<int> left = effectGroups.front().bounds;  // the panel the marker goes right of (SYNTH first)
    for (const int e : order) {
        if (e == effect) continue;
        const auto& b = effectGroups[static_cast<std::size_t>(e + 1)].bounds;
        const bool before = where.y >= b.getBottom() + m.gap
                         || (where.y >= b.getY() - m.gap && where.x > b.getCentreX());
        if (!before) break;
        ++position;
        left = b;
    }
    if (position == effectPosition(effect)) return std::nullopt;
    if (marker != nullptr) *marker = {left.getRight() + m.gap / 2 - 2, left.getY(), 4, left.getHeight()};
    return position;
}

void LMWSynthEditor::dragEffect(int effect, juce::Point<int> where) {
    draggedEffect = effect;
    juce::Rectangle<int> marker;
    effectDrop.reset();
    if (findEffectDrop(effect, where, &marker)) effectDrop = marker;
    rackArea->repaint();
    effectHeaders[static_cast<std::size_t>(effect)]->repaint();
}

void LMWSynthEditor::dropEffect(int effect, juce::Point<int> where) {
    const auto position = findEffectDrop(effect, where, nullptr);
    draggedEffect = -1;
    effectDrop.reset();
    rackArea->repaint();
    effectHeaders[static_cast<std::size_t>(effect)]->repaint();
    if (position) moveEffect(effect, *position);
}

bool LMWSynthEditor::moveEffect(int effect, int position) {
    if (position < 0 || position >= lmw::dsp::numEffects || position == effectPosition(effect)) return false;
    auto order = lmw::dsp::effectOrder(shownEffectOrder);
    std::vector<int> v(order.begin(), order.end());
    v.erase(std::find(v.begin(), v.end(), effect));
    v.insert(v.begin() + position, effect);
    std::copy(v.begin(), v.end(), order.begin());
    processor.setEffectOrder(lmw::dsp::effectOrderIndex(order));   // one undo step, the Schematic follows
    shownEffectOrder = processor.effectOrder();
    resized();
    repaint();
    if (auto& h = *effectHeaders[static_cast<std::size_t>(effect)]; h.isShowing()) h.grabKeyboardFocus();
    return true;
}

void LMWSynthEditor::resetEffectOrder() {                  // one undo step; the Schematic blocks go back too
    processor.setEffectOrder(0);
    shownEffectOrder = processor.effectOrder();
    resized();
    repaint();
}

void LMWSynthEditor::showEffectMenu(int effect) {
    const int at = effectPosition(effect);
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);                         // black on white (Spec S6.2)
    menu.addSectionHeader(effectGroups[static_cast<std::size_t>(effect + 1)].title);
    if (at > 0) menu.addItem(1, "Move left");                  // only possible moves (greyed items: < 4.5:1)
    if (at < lmw::dsp::numEffects - 1) menu.addItem(2, "Move right");
    if (shownEffectOrder != 0) { menu.addSeparator(); menu.addItem(3, "Reset order"); }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(effectHeaders[static_cast<std::size_t>(effect)].get()),
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), effect, at, focus = focusNow()](int result) {
                           if (safe == nullptr) return;
                           safe->restoreFocus(focus);
                           if (result == 1) safe->moveEffect(effect, at - 1);
                           else if (result == 2) safe->moveEffect(effect, at + 1);
                           else if (result == 3) safe->resetEffectOrder();
                       });
}

std::optional<LMWSynthEditor::DropTarget> LMWSynthEditor::findDrop(int panel, juce::Point<int> p) const {
    if (rowBounds.empty()) return std::nullopt;
    const auto& rows = rack->rows();
    const int nRows = static_cast<int>(rows.size());
    const auto& first = rowBounds.front();
    DropTarget t;
    if (p.y < first.getY()) {                                   // above the first row: new top row
        t.newRow = true;
        t.row = 0;
    } else if (p.y >= rowBounds.back().getBottom()) {           // below the last row: new bottom row
        t.newRow = true;
        t.row = nRows;
    } else {
        for (int r = 0; r < nRows; ++r)
            if (p.y < rowBounds[static_cast<std::size_t>(r)].getBottom() + m.gap) { t.row = r; break; }
    }
    if (t.newRow) {
        const int y = t.row == 0 ? first.getY() - m.gap / 2 : rowBounds.back().getBottom() + m.gap / 2;
        t.marker = {first.getX(), y - 2, rackArea->getWidth() - 2 * first.getX(), 4};
    } else {
        const auto& rowArea = rowBounds[static_cast<std::size_t>(t.row)];
        int x = rowArea.getX() - m.gap / 2;
        for (int q : rows[static_cast<std::size_t>(t.row)]) {    // insert before the first panel right of p
            if (q == panel) continue;
            const auto& b = groups[static_cast<std::size_t>(q)].bounds;
            if (p.x <= b.getCentreX()) break;
            ++t.index;
            x = b.getRight() + m.gap / 2;
        }
        t.marker = {x - 2, rowArea.getY(), 4, rowArea.getHeight()};
    }
    const auto result = rack->preview(panel, t.row, t.index, t.newRow);
    if (!result || *result == rows) return std::nullopt;        // not allowed (row full) or no change
    return t;
}

void LMWSynthEditor::dragPanel(int panel, juce::Point<int> where) {
    draggedPanel = panel;
    drop = findDrop(panel, where);
    repaint();
}

void LMWSynthEditor::dropPanel(int panel, juce::Point<int> where) {
    const auto t = findDrop(panel, where);
    draggedPanel = -1;
    drop.reset();
    if (t && rack->move(panel, t->row, t->index, t->newRow)) layoutChanged(panel);
    else repaint();
}

void LMWSynthEditor::cancelDrag() {
    draggedPanel = -1;
    drop.reset();
    repaint();
}

bool LMWSynthEditor::movePanel(int panel, int direction) {
    bool moved = false;
    switch (direction) {
        case 0: moved = rack->moveLeft(panel); break;
        case 1: moved = rack->moveRight(panel); break;
        case 2: moved = rack->moveUp(panel); break;
        case 3: moved = rack->moveDown(panel); break;
        default: break;
    }
    if (moved) layoutChanged(panel);
    return moved;
}

void LMWSynthEditor::showPanelMenu(int panel) {
    const auto can = [this, panel](int direction) {
        auto copy = *rack;
        switch (direction) {
            case 0: return copy.moveLeft(panel);
            case 1: return copy.moveRight(panel);
            case 2: return copy.moveUp(panel);
            default: return copy.moveDown(panel);
        }
    };
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);              // black on white like the panel (Spec S6.2)
    menu.addSectionHeader(groups[static_cast<std::size_t>(panel)].title);
    // Only possible moves are listed: greyed-out items would fall below the 4.5:1 contrast minimum.
    const char* names[] = {"Move left", "Move right", "Move up", "Move down"};
    for (int i = 0; i < 4; ++i)
        if (can(i)) menu.addItem(i + 1, names[i]);
    menu.addSeparator();
    menu.addItem(5, "Reset layout");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(headers[static_cast<std::size_t>(panel)].get()),
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), panel, focus = focusNow()](int result) {
                           if (safe == nullptr) return;
                           safe->restoreFocus(focus);
                           if (result >= 1 && result <= 4) {
                               safe->movePanel(panel, result - 1);
                           } else if (result == 5) {
                               safe->rack->reset();
                               safe->layoutChanged(panel);
                           }
                       });
}

void LMWSynthEditor::layoutChanged(int focusPanel) {
    processor.state().state.setProperty(kLayoutProp, juce::String(rack->serialize()), nullptr);
    updateSize();
    if (focusPanel >= 0) {
        auto& h = *headers[static_cast<std::size_t>(focusPanel)];
        if (h.isShowing()) h.grabKeyboardFocus();
    }
}

void LMWSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kFg);
    const int pad = m.gap + 2;
    g.setFont(juce::FontOptions(m.titleH * 0.6f, juce::Font::bold));
    const float lineY = static_cast<float>(pad + m.titleH + m.gap / 2);   // between title row and modules
    g.drawLine(static_cast<float>(pad), lineY, static_cast<float>(getWidth() - pad), lineY, 1.0f);
    if (currentTab == 0 || currentTab == 1) {                // Spec S6.2: light grey around the rack view too
        const int top = juce::roundToInt(lineY) + 1;
        g.setColour(toJuce(palette.rack));
        g.fillRect(0, top, getWidth(), keyboardButton.getY() - 1 - top);   // down to the keyboard strip (S6.12)
    }
}

// Spec S6.21: the rack surface (rack coordinates; it scrolls in rackView). Spec S6.2: light grey around the
// panels, panels white; S6.16: rack rails at the row pitch over the whole surface.
void LMWSynthEditor::paintRack(juce::Graphics& g) {
    const auto& area = *rackArea;
    g.fillAll(toJuce(palette.rack));
    {
        int y = currentTab == 0 ? (rowBounds.empty() ? 0 : rowBounds.front().getY())
                                : (effectGroups.empty() ? 0 : effectGroups.front().bounds.getY());
        g.setColour(toJuce(palette.cable));
        for (y += m.rowH + m.gap / 2; y < area.getHeight(); y += m.rowH + m.gap)
            g.fillRect(0, y - 1, area.getWidth(), 2);
    }
    // Spec S6.30: with Panel gradient, a coloured unit's panel fades from (about) its title colour under the title
    // bar to the page at the line above the sockets; the socket row stays the page colour.
    const auto fillPanel = [&](const Group& grp, bool jacks) {
        g.setColour(kBg);
        g.fillRect(grp.bounds);
        if (!settings.panelGradient) return;
        const auto top = palette.gradientTop(lmw::ui::theme::unitOf(grp.title.toStdString()));
        if (!top) return;
        const auto& b = grp.bounds;
        const int y0 = b.getY() + m.header - 2, y1 = jacks ? b.getBottom() - 3 - m.jackH : b.getBottom();
        if (y1 <= y0) return;
        g.setGradientFill(juce::ColourGradient(toJuce(*top), 0.0f, static_cast<float>(y0), kBg, 0.0f, static_cast<float>(y1), false));
        g.fillRect(b.getX(), y0, b.getWidth(), y1 - y0);
    };
    if (currentTab == 0) for (std::size_t i = 0; i < groups.size(); ++i) fillPanel(groups[i], !groupJacks[i].empty());
    else for (const auto& grp : effectGroups) fillPanel(grp, true);
    g.setColour(kFg);

    if (currentTab == 1) {                                   // Effects tab: fixed panels, titles drawn here
        g.setFont(juce::FontOptions(m.titleFont, juce::Font::bold));
        for (const auto& grp : effectGroups) {
            const auto& b = grp.bounds;
            g.setColour(toJuce(palette.unitColour(lmw::ui::theme::unitOf(grp.title.toStdString()))));   // S6.9
            g.fillRect(b.withHeight(m.header - 3));
            g.setColour(kFg);
            drawPanelFrame(g, b, effectGroupJacks[static_cast<std::size_t>(&grp - effectGroups.data())], 1);   // S6.26
            g.drawText(grp.title, b.getX(), b.getY(), b.getWidth(), m.header - 3, juce::Justification::centred);
            if (const auto label = presetLabel(presetModuleOf(grp.title)); label.isNotEmpty()) {   // Spec S7.5
                const juce::FontOptions titleFont(m.titleFont, juce::Font::bold);
                const int room = (b.getWidth() - juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleFont, grp.title))) / 2 - 10;
                if (room > 16) {
                    g.setFont(juce::FontOptions(m.titleFont * 0.85f));
                    g.drawText(label, b.getX() + 5, b.getY(), room, m.header - 3, juce::Justification::centredLeft, true);
                    g.setFont(titleFont);
                }
            }
            g.drawHorizontalLine(b.getY() + m.header - 3, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));
            g.drawHorizontalLine(b.getBottom() - 3 - m.jackH, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));   // S7.6
        }
        if (effectDrop) { g.setColour(kFg); g.fillRect(*effectDrop); }   // S7.14: where the dragged effect lands
    }
    if (currentTab != 0) return;                             // other tabs: no rack panel frames
    for (std::size_t i = 0; i < groups.size(); ++i) {        // titles are drawn by the PanelHeaders
        const auto& b = groups[i].bounds;
        drawPanelFrame(g, b, groupJacks[i], static_cast<int>(i) == draggedPanel ? 3 : 1);   // S6.26: dashed without effect
        if (!groupJacks[i].empty())                          // line above the patch points
            g.drawHorizontalLine(b.getBottom() - 3 - m.jackH, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));
    }
    if (drop) g.fillRect(drop->marker);                     // drop marker (Spec S6.5)
}

// Sockets per fader column (Spec S6.6); sockets sharing a column split it evenly.
void LMWSynthEditor::layoutJacks(const std::vector<Jack*>& js, int columns, juce::Rectangle<int> jackRow) const {
    for (int c = 0; c < columns; ++c) {
        std::vector<Jack*> col;
        for (auto* j : js) if (j->column == c) col.push_back(j);
        const int left = jackRow.getX() + c * m.colW, n = static_cast<int>(col.size());
        for (int k = 0; k < n; ++k)
            col[static_cast<std::size_t>(k)]->setBounds(left + m.colW * k / n, jackRow.getY(),
                                                        m.colW * (k + 1) / n - m.colW * k / n, jackRow.getHeight());
    }
}

void LMWSynthEditor::layoutItem(const Item& item, juce::Rectangle<int> column) const {
    column.reduce(m.colW >= 80 ? 4 : 2, 0);
    if (item.fader != nullptr && item.choice != nullptr && item.boxOnTop) {   // S5.28: the box with its label, then the fader
        item.choice->label.setBounds(column.removeFromTop(m.labelH));
        item.choice->box.setBounds(column.removeFromTop(m.boxH + 2).reduced(2, 0));
        column.removeFromTop(m.gap / 2);
    } else if (item.fader != nullptr && item.choice != nullptr) {   // S5.25: a box under the fader, its text says it
        item.choice->label.setBounds({});
        item.choice->box.setBounds(column.removeFromBottom(m.boxH + 2).reduced(2, 0));
        column.removeFromBottom(m.gap / 2);
    }
    if (item.fader != nullptr) {
        item.fader->label.setBounds(column.removeFromTop(m.labelH));
        item.fader->tag.setBounds(column.removeFromTop(tagH()));   // Spec S6.17 (also kept free without a tag)
        item.fader->slider.setBounds(column);
    } else if (item.choice != nullptr) {
        column.reduce(2, 0);
        item.choice->label.setBounds(column.removeFromTop(m.labelH));
        item.choice->box.setBounds(column.removeFromTop(m.boxH + 2));
        if (item.choice2 != nullptr) {
            column.removeFromTop(m.gap);
            item.choice2->label.setBounds(column.removeFromTop(m.labelH));
            item.choice2->box.setBounds(column.removeFromTop(m.boxH + 2));
        }
        if (item.choice3 != nullptr) {                       // no label: the box text says it (Serial / Parallel)
            column.removeFromTop(m.gap / 2);
            item.choice3->label.setBounds({});
            item.choice3->box.setBounds(column.removeFromTop(m.boxH + 2));
        }
    }
}

void LMWSynthEditor::resized() {
    if (!rack) return;
    const int pad = m.gap + 2;
    auto area = getLocalBounds().reduced(pad);
    auto title = area.removeFromTop(m.titleH);
    // Compact, or a window narrower than the full header (S6.21: down to 1280 px): shorter texts and buttons.
    const bool wide = m.colW >= 80 && getWidth() >= 1520;
    compactButton.setBounds(title.removeFromRight(wide ? 90 : 78).reduced(0, 2));
    title.removeFromRight(m.gap);
    resetLayoutButton.setButtonText(wide ? "Reset layout" : "Reset");
    resetLayoutButton.setBounds(title.removeFromRight(wide ? 110 : 56).reduced(0, 2));
    title.removeFromRight(m.gap);
    saveButton.setBounds(title.removeFromRight(wide ? 76 : 64).reduced(0, 2));
    title.removeFromRight(m.gap / 2);
    loadButton.setBounds(title.removeFromRight(wide ? 76 : 64).reduced(0, 2));
    title.removeFromRight(m.gap / 2);
    instrumentButton.setBounds(title.removeFromRight(wide ? 76 : 64).reduced(0, 2));   // Spec S7.7
    title.removeFromRight(m.gap / 2);
    menuButton.setBounds(title.removeFromRight(wide ? 70 : 60).reduced(0, 2));       // Spec S6.11
    title.removeFromRight(m.gap / 2);
    // Spec S6.14: "↶ Undo" / "Redo ↷"; compact view only the arrows (the hover text names the step).
    undoButton.setButtonText(juce::String::fromUTF8(wide ? "\xe2\x86\xb6 Undo" : "\xe2\x86\xb6"));
    redoButton.setButtonText(juce::String::fromUTF8(wide ? "Redo \xe2\x86\xb7" : "\xe2\x86\xb7"));
    redoButton.setBounds(title.removeFromRight(wide ? 76 : 34).reduced(0, 2));
    title.removeFromRight(m.gap / 2);
    undoButton.setBounds(title.removeFromRight(wide ? 76 : 34).reduced(0, 2));
    titleButton.fontHeight = m.titleH * 0.6f;
    titleButton.setBounds(title.removeFromLeft(titleTextWidth()).reduced(0, 2));
    title.removeFromLeft(m.gap);
    const int tabW = wide ? 70 : 60;
    panelTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));       // Spec S6.25: Synth, Matrix, Schematic, Effects
    matrixTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));
    schematicTab.setBounds(title.removeFromLeft(tabW + (wide ? 22 : 18)).reduced(0, 2));
    effectsTab.setBounds(title.removeFromLeft(tabW + (wide ? 10 : 8)).reduced(0, 2));
    notesTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));
    if (assistant) assistantTab.setBounds(title.removeFromLeft(tabW + (wide ? 22 : 18)).reduced(0, 2));   // Spec S12
    title.removeFromLeft(m.gap);
    loadLabel.setBounds(title.removeFromRight(wide ? 90 : 76).reduced(0, 3));   // Spec S6.28: left of Undo
    title.removeFromRight(m.gap / 2);
    patchLabel.setBounds(title);
    area.removeFromTop(m.gap);
    // Spec S6.12, S6.16: the 88 keys (52 white) at their normal width — as wide as the default rack layout (in
    // compact view at least 16 px, then the keyboard scrolls) — at the right; the free space on the left is
    // kept for later controls (user 2026-11-24: a mod wheel, glissando / portamento).
    // Spec S6.24: the keys at their normal width again (28 px, compact 21 px), the playing controls on the left.
    for (juce::Component* c : {static_cast<juce::Component*>(&bendWheel), static_cast<juce::Component*>(&modWheelCtl),
                               static_cast<juce::Component*>(&glideSlider), static_cast<juce::Component*>(&glideModeBox),
                               static_cast<juce::Component*>(&bendRangeBox), static_cast<juce::Component*>(&bendLabel),
                               static_cast<juce::Component*>(&modLabel), static_cast<juce::Component*>(&glideLabel),
                               static_cast<juce::Component*>(&bendRangeLabel)})
        c->setVisible(keyboardShown);
    if (keyboardShown) {
        const float keyW = m.colW >= 80 ? 28.0f : 21.0f;
        auto keys = area.removeFromBottom(m.keysH);
        keyboard.setBounds(keys.removeFromRight(std::min(keys.getWidth(), juce::roundToInt(keyW * 52.0f))));
        keyboard.setKeyWidth(keyW);
        layoutPlayStrip(keys.withTrimmedRight(m.gap));
    }
    {
        auto strip = area.removeFromBottom(keyStripH());
        keyboardButton.setBounds(strip.removeFromRight(m.colW >= 80 ? 96 : 84).reduced(0, 1));
    }
    area.removeFromBottom(m.gap);
    notesEditor.setBounds(area);                             // Notes tab: the area of the panels
    if (matrix) { matrix->setBounds(area); matrix->setFontSize(m.labelFont - 2.0f); }   // Matrix tab
    schematicView.setBounds(area);                          // Schematic tab (S6.22: in its view)
    layoutSchematic();
    if (assistant) { assistant->setBounds(area); assistant->setFontSize(m.labelFont + 3.0f); }   // Assistant tab
    // Spec S6.21: the rack in rackView at `area`; the rack surface is as large as the rows need (at least the view),
    // laid out in its own coordinates from (0, 0). A scroll bar appears where it is larger than the view.
    rackView.setBounds(area);
    {
        int needW = 0, effectsW = 0;
        for (const auto& row : rack->rows())
            needW = std::max(needW, rack->rowColumns(row) * m.colW + (static_cast<int>(row.size()) - 1) * m.gap);
        for (const auto& grp : effectGroups) effectsW = std::max(effectsW, grp.columns() * m.colW);   // wraps: one panel at least
        needW = std::max(needW, effectsW);
        const int rows = std::max(1, static_cast<int>(rack->rows().size()));
        int needH = rows * m.rowH + (rows - 1) * m.gap;
        if (fxMatrix) {                                     // Spec S6.25: the Effects tab — its panel rows and the matrix
            fxMatrix->setFontSize(m.labelFont - 2.0f);
            const auto fx = fxMatrix->preferredSize(5.5f, 2.2f);
            int effectRows = 1, x = 0;
            for (const int gi : effectPanelOrder()) {
                const int w = effectGroups[static_cast<std::size_t>(gi)].columns() * m.colW;
                if (x > 0 && x + w > std::max(area.getWidth(), effectsW)) { ++effectRows; x = 0; }
                x += w + m.gap;
            }
            const bool besides = std::max(area.getWidth(), effectsW) - x >= fx.x && fx.y <= m.rowH;   // right of the last panel
            needH = std::max(needH, besides ? effectRows * m.rowH + (effectRows - 1) * m.gap : effectRows * (m.rowH + m.gap) + fx.y);
            needW = std::max(needW, fx.x);
        }
        const int bar = rackView.getScrollBarThickness();
        const bool horz = needW > area.getWidth();
        const bool vert = needH > area.getHeight() - (horz ? bar : 0);
        const bool horz2 = needW > area.getWidth() - (vert ? bar : 0);
        rackArea->setSize(std::max(needW, area.getWidth() - (vert ? bar : 0)), std::max(needH, area.getHeight() - (horz2 ? bar : 0)));
    }
    const auto rackRect = rackArea->getLocalBounds();
    const int viewW = area.getWidth();
    const auto effectsMinW = [this] { int w = 0; for (const auto& grp : effectGroups) w = std::max(w, grp.columns() * m.colW); return w; };
    {   // Effects tab: panels side by side; one that does not fit the view's width starts a second row (S7.11)
        auto row = rackRect.withHeight(m.rowH).withWidth(std::max(viewW, effectsMinW()));
        for (const int gi : effectPanelOrder()) {           // Spec S7.14: in the chain's order
            auto& grp = effectGroups[static_cast<std::size_t>(gi)];
            const int w = grp.columns() * m.colW;
            if (w > row.getWidth() && row.getX() > rackRect.getX())
                row = rackRect.withHeight(m.rowH).withWidth(viewW).withY(row.getY() + m.rowH + m.gap);
            grp.bounds = row.removeFromLeft(w);
            row.removeFromLeft(m.gap);
            if (const auto it = effectPresets.find(presetModuleOf(grp.title)); it != effectPresets.end())   // Spec S7.5
                it->second->setBounds(grp.bounds.withHeight(m.header - 3).reduced(1)
                                          .removeFromRight(it->second->preferredWidth()).reduced(2));
            if (gi >= 1 && gi <= lmw::dsp::numEffects)        // S7.14: the title, left of a preset button
                effectHeaders[static_cast<std::size_t>(gi - 1)]->setBounds(grp.bounds.withHeight(m.header - 3));
            if (grp.title == "MAIN")                          // Spec S5.31: its lamp under the title (the title fills the bar)
                mainLamp.setBounds(grp.bounds.getCentreX() - lampWidth() / 2, grp.bounds.getY() + m.header + 4, lampWidth(), m.header - 6);
            auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
            auto jackRow = inner.removeFromBottom(m.jackH).withTrimmedTop(2);   // same fader height as on the Synth tab
            layoutJacks(effectGroupJacks[static_cast<std::size_t>(&grp - effectGroups.data())], grp.columns(), jackRow);
            for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
        }
        if (fxMatrix) {                                     // Spec S6.25: right of the last panel, else below
            fxMatrix->setPorts(MatrixView::effectInputs(shownEffectOrder), MatrixView::effectOutputs(shownEffectOrder));
            const auto fx = fxMatrix->preferredSize(5.5f, 2.2f);
            if (row.getWidth() >= fx.x && fx.y <= m.rowH)  // right of the last panel when it fits, else below
                fxMatrix->setBounds(row.getX(), row.getY(), fx.x, fx.y);
            else
                fxMatrix->setBounds(rackRect.getX(), row.getY() + m.rowH + m.gap, fx.x, fx.y);
        }
    }

    // Spec S6.5: panels in the order of the rack layout.
    rowBounds.clear();
    auto rackRows = rackRect;
    for (const auto& rowPanels : rack->rows()) {
        if (!rowBounds.empty()) rackRows.removeFromTop(m.gap);
        auto row = rackRows.removeFromTop(m.rowH);
        rowBounds.push_back(row);
        for (int index : rowPanels) {
            auto& grp = groups[static_cast<std::size_t>(index)];
            grp.bounds = row.removeFromLeft(grp.columns() * m.colW);
            row.removeFromLeft(m.gap);
            headers[static_cast<std::size_t>(index)]->setBounds(grp.bounds.getX() + 1, grp.bounds.getY() + 1,
                                                                 grp.bounds.getWidth() - 2, m.header - 4);
            if (index == outGroup)                           // Spec S5.31: the lamp at the right of OUT's title bar
                outLamp.setBounds(grp.bounds.getRight() - 4 - lampWidth(), grp.bounds.getY() + 2, lampWidth(), m.header - 6);
            auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
            auto jackRow = inner.removeFromBottom(m.jackH).withTrimmedTop(2);   // Spec S6.6
            layoutJacks(groupJacks[static_cast<std::size_t>(index)], grp.columns(), jackRow);
            for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
        }
    }
    if (cables) cables->setBounds(rackArea->getLocalBounds());
}

// ---- Spec S6.6: patch points and cables ----

namespace {
juce::String portName(bool output, int port) {
    const auto n = output ? lmw::dsp::outputNames[static_cast<std::size_t>(port)]
                          : lmw::dsp::inputNames[static_cast<std::size_t>(port)];
    return juce::String(n.data(), n.size());
}
}

LMWSynthEditor::Jack::Jack(LMWSynthEditor& e, bool isOutput, int portIndex, juce::String shortName)
    : output(isOutput), port(portIndex), editor(e), name(std::move(shortName)) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTitle((output ? "Output " : "Input ") + portName(output, port));
}

// Spec S6.19: 2 px smaller than before, so the type ring (1 px gap, 2 px line) fits above the label.
juce::Rectangle<float> LMWSynthEditor::Jack::socket() const {
    const float d = editor.m.colW >= 80 ? 14.0f : 12.0f;
    return juce::Rectangle<float>(d, d).withCentre({getWidth() * 0.5f, 1.0f + 3.0f + d * 0.5f});
}

juce::Point<float> LMWSynthEditor::Jack::centreInEditor() const {
    return editor.rackArea->getLocalPoint(this, socket().getCentre());   // S6.21: rack coordinates
}

void LMWSynthEditor::Jack::paint(juce::Graphics& g) {
    const auto c = socket();
    const auto& patch = editor.processor.currentPatch();
    // Spec S6.15: the selected socket (keyboard focus, under the mouse, drop target) and its label on a box in
    // the highlight colour with a 2 px black edge; at Max 21:1 the fill is white and the edge 3 px.
    if (hasKeyboardFocus(false) || editor.hoverJack == this || (editor.drag && editor.drag->target == this)) {
        const auto& h = editor.palette.highlight;
        const bool edgeOnly = h == editor.palette.background;   // Max: no fill to see (S6.29: either page)
        const auto box = getLocalBounds().toFloat();
        g.setColour(toJuce(h));
        g.fillRect(box);
        g.setColour(kFg);
        g.drawRect(box, edgeOnly ? 3.0f : 2.0f);
    }
    if (output) {                                     // black socket, white ring
        g.setColour(kFg);
        g.fillEllipse(c);
        g.setColour(kBg);
        g.drawEllipse(c.reduced(3.5f), 1.5f);
    } else {                                          // white socket, black ring; black centre = cable
        g.setColour(kBg);
        g.fillEllipse(c);
        g.setColour(kFg);
        g.drawEllipse(c.reduced(1.0f), 2.0f);
        if (patch.isPatched(static_cast<lmw::dsp::In>(port))) g.fillEllipse(c.reduced(4.0f));
    }
    if (editor.settings.socketRings) {   // Spec S6.19: the type ring (audio solid, CV double, gate dashed); S6.20: on / off
        const auto t = output ? lmw::dsp::typeOf(static_cast<lmw::dsp::Out>(port)) : lmw::dsp::typeOf(static_cast<lmw::dsp::In>(port));
        const auto col = editor.palette.socket[static_cast<std::size_t>(t)];
        lmw::ui::drawTypeRing(g, c.getCentre(), c.getWidth() * 0.5f + 2.0f, 2.0f, t, toJuce(col));
    }
    g.setColour(kFg);
    // Label shrinks to fit a narrow slot (several sockets under one fader column).
    float size = editor.m.colW >= 80 ? 12.0f : 11.0f;
    juce::Font font{lmw::app::smallFont(size)};            // S6.29: bold in a dark theme
    while (size > 8.0f && juce::GlyphArrangement::getStringWidth(font, name) > static_cast<float>(getWidth() - 2)) {
        size -= 0.5f;
        font = juce::Font(lmw::app::smallFont(size));
    }
    g.setFont(font);
    g.drawText(name, getLocalBounds().withTrimmedTop(static_cast<int>(c.getBottom()) + 4), juce::Justification::centredTop, false);   // below the ring
}

juce::String LMWSynthEditor::Jack::getTooltip() {
    if (output)
        return lmw::ui::typeLine(false, port) + "\n" + editor.describeOutput(static_cast<lmw::dsp::Out>(port))
             + "\nDrag to an input to patch a cable. Enter: cable menu.";
    const auto in = static_cast<lmw::dsp::In>(port);
    return lmw::ui::typeLine(true, port) + "\n" + editor.describeInput(in) + "\nDrag to an output to add a cable (up to 4 per input)"
         + (editor.processor.currentPatch().isPatched(in) ? "; drag the newest cable away to remove it. Delete: remove all." : ".")
         + "\nEnter: cable menu.";
}

void LMWSynthEditor::Jack::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) editor.showPortMenu(output, port, getScreenBounds());
}

void LMWSynthEditor::Jack::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || (!dragging && e.getDistanceFromDragStart() < 3)) return;
    dragging = true;
    editor.autoScrollRack(e.getEventRelativeTo(editor.rackArea.get()).getPosition());   // S6.21
    editor.cableDrag(*this, e.getEventRelativeTo(editor.rackArea.get()).position);
}

void LMWSynthEditor::Jack::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.cableDrop(*this, e.getEventRelativeTo(editor.rackArea.get()).position);
}

bool LMWSynthEditor::Jack::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && dragging) { dragging = false; editor.cancelCableDrag(); return true; }
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())) {
        editor.showPortMenu(output, port, getScreenBounds());
        return true;
    }
    if (!output && (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)) {
        editor.connect(port, lmw::dsp::Patch::none);
        return true;
    }
    return false;                        // e.g. QWERTY note keys: handled by the editor
}

void LMWSynthEditor::CableLayer::paint(juce::Graphics& g) {
    using lmw::dsp::In;
    // A cable hangs down a little between its ends; drawn with a white edge so it stays visible where it
    // crosses black faders (Spec S6.2). Grey at rest; black, 1 px thicker and on top when one of its ends
    // is the socket under the mouse or with keyboard focus (user 2026-11-22), and while it is dragged.
    // Phase 21a: at rest see-through — black in one transparency layer at cableAlpha() (looks like the
    // cable grey on white); from AAA up the cable grey itself, opaque.
    const float alpha = editor.palette.cableAlpha();
    const auto rest = toJuce(editor.palette.cableDrawn());   // S6.13: the theme's cable colour
    const auto draw = [&g](juce::Point<float> a, juce::Point<float> b, juce::Colour colour, bool hot) {
        const float sag = 15.0f + 0.2f * a.getDistanceFrom(b);   // the middle hangs lowest
        juce::Path path;
        path.startNewSubPath(a);
        path.quadraticTo((a + b) * 0.5f + juce::Point<float>(0.0f, sag), b);
        const float w = hot ? 4.0f : 3.0f;
        const juce::PathStrokeType edge(w + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const juce::PathStrokeType core(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(kBg);
        g.strokePath(path, edge);
        g.setColour(colour);
        g.strokePath(path, core);
        for (auto p : {a, b}) g.fillEllipse(juce::Rectangle<float>(w + 6.0f, w + 6.0f).withCentre(p));   // plugs
    };
    const auto* focused = dynamic_cast<const Jack*>(juce::Component::getCurrentlyFocusedComponent());
    const auto isHot = [&](const Jack* j) { return j != nullptr && (j == editor.hoverJack || j == focused); };
    struct Line { juce::Point<float> a, b; };
    std::vector<Line> hot, calm;
    const auto& patch = editor.processor.currentPatch();
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {                // one cable per pin (Spec S5.14)
        const auto in = static_cast<In>(i);
        for (int k = 0; k < patch.pinCount(in); ++k) {
            const int src = patch.pin(in, k).source;
            if (editor.drag && editor.drag->detachedInput == i && editor.drag->detachedSource == src) continue;
            auto* from = editor.jackFor(true, src);
            auto* to = editor.jackFor(false, i);
            if (from == nullptr || to == nullptr || !from->isVisible() || !to->isVisible()) continue;   // this tab's sockets
            if (isHot(from) || isHot(to)) hot.push_back({from->centreInEditor(), to->centreInEditor()});
            else calm.push_back({from->centreInEditor(), to->centreInEditor()});
        }
    }
    if (!calm.empty()) {
        if (alpha < 1.0f) g.beginTransparencyLayer(alpha);
        for (const auto& l : calm) draw(l.a, l.b, rest, false);
        if (alpha < 1.0f) g.endTransparencyLayer();
    }
    for (const auto& l : hot) draw(l.a, l.b, kFg, true);
    if (const auto& d = editor.drag)
        draw(d->fixed->centreInEditor(), d->target != nullptr ? d->target->centreInEditor() : d->loose, kFg, true);
}

// The cables of the socket under the mouse or with keyboard focus are drawn black (user 2026-11-22).
void LMWSynthEditor::Jack::focusGained(FocusChangeType) { repaint(); if (editor.cables) editor.cables->repaint(); }
void LMWSynthEditor::Jack::focusLost(FocusChangeType) { repaint(); if (editor.cables) editor.cables->repaint(); }
void LMWSynthEditor::Jack::mouseEnter(const juce::MouseEvent&) {
    editor.hoverJack = this;
    repaint();                                                    // S6.15 highlight
    if (editor.cables) editor.cables->repaint();
}
void LMWSynthEditor::Jack::mouseExit(const juce::MouseEvent&) {
    if (editor.hoverJack == this) editor.hoverJack = nullptr;
    repaint();
    if (editor.cables) editor.cables->repaint();
}

LMWSynthEditor::Jack* LMWSynthEditor::jackFor(bool output, int port) const {
    for (const auto& j : jacks)
        if (j->output == output && j->port == port) return j.get();
    return nullptr;
}

LMWSynthEditor::Jack* LMWSynthEditor::findJack(juce::Point<float> where, const Jack& from) const {
    for (const auto& j : jacks)
        if (j->output != from.output && j->isVisible() && rackArea->getLocalArea(j.get(), j->getLocalBounds()).toFloat().expanded(4.0f).contains(where))
            return j.get();
    return nullptr;
}

void LMWSynthEditor::cableDrag(Jack& from, juce::Point<float> where) {
    using lmw::dsp::In;
    if (!drag) {                                         // same rules as the Schematic tab (CableEdit.h)
        const auto g = lmw::ui::cables::grab(processor.currentPatch(), {from.output, from.port});
        CableDrag d;
        d.detachedInput = g.detachedInput;
        d.detachedSource = g.detachedSource;
        d.detachedLevel = g.detachedLevel;
        d.fixed = jackFor(g.fixed.output, g.fixed.index);
        if (d.fixed == nullptr) return;
        drag = d;
    }
    drag->loose = where;
    if (auto* t = findJack(where, *drag->fixed); t != drag->target) {
        if (drag->target != nullptr) drag->target->repaint();
        drag->target = t;
        if (t != nullptr) t->repaint();
    }
    cables->repaint();
}

void LMWSynthEditor::cableDrop(Jack& from, juce::Point<float> where) {
    if (!drag) return;
    cableDrag(from, where);
    const CableDrag d = *drag;
    drag.reset();
    namespace ce = lmw::ui::cables;
    ce::Grab g;
    g.fixed = {d.fixed->output, d.fixed->port};
    g.detachedInput = d.detachedInput;
    g.detachedSource = d.detachedSource;
    g.detachedLevel = d.detachedLevel;
    const auto target = d.target != nullptr ? std::optional<ce::PortRef>(ce::PortRef{d.target->output, d.target->port}) : std::nullopt;
    for (const auto& op : ce::drop(g, target))
        if (!connect(op.input, op.output, op.level)) break;            // e.g. a full input: a moved cable stays
    repaint();
}

void LMWSynthEditor::cancelCableDrag() {
    drag.reset();
    repaint();
}

bool LMWSynthEditor::connect(int input, int output, float level) {
    const bool adding = output != lmw::dsp::Patch::none && !lmw::dsp::removes(level);   // a level, or removing
    if (adding && !lmw::dsp::canConnect(static_cast<lmw::dsp::In>(input), static_cast<lmw::dsp::Out>(output))) {   // Spec S7.6
        showError("Not possible", "Effect sockets (Effects tab) connect only to effect sockets, synth sockets only to synth sockets.");
        return false;
    }
    if (adding && level < lmw::dsp::minLevel(static_cast<lmw::dsp::In>(input))) {   // Spec S5.21
        showError("Not possible", portName(false, input) + " is an audio or gate input: its cables take 0 to 100 %. "
                                  "Only modulation inputs take a negative (inverted) level.");
        return false;
    }
    const juce::String inName(lmw::dsp::inputNames[static_cast<std::size_t>(input)].data());
    cableStep(adding ? "cable " + inName + juce::String::fromUTF8(" \xe2\x86\x90 ")
                           + juce::String(lmw::dsp::outputNames[static_cast<std::size_t>(output)].data())
                     : "remove cable " + inName, adding);
    const bool ok = processor.requestPatch(static_cast<lmw::dsp::In>(input), static_cast<std::int8_t>(output), level);
    if (!ok && adding)
        showError("Input full", portName(false, input) + " already has " + juce::String(lmw::dsp::Patch::maxPins)
                                + " cables. Remove one first.");
    shownPatchVersion = processor.patchVersion();
    if (matrix) matrix->refresh();
    if (fxMatrix) fxMatrix->refresh();
    repaint();
    return ok;
}

void LMWSynthEditor::cableStep(const juce::String& name, bool adding) {
    if (cableStepOpen) {                                     // the same event: one step, named for the new cable
        if (adding) processor.renameStep(name.toStdString());
        return;
    }
    processor.beginStep(name.toStdString());
    cableStepOpen = true;
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<LMWSynthEditor>(this)] {
        if (safe == nullptr || !safe->cableStepOpen) return;
        safe->cableStepOpen = false;
        safe->processor.endStep();
    });
}

// Spec S6.14: undo / redo one step; the panels, labels and cables follow.
void LMWSynthEditor::undoStep() {
    if (!processor.undo()) return;
    refreshPatchInfo();
    updatePresetLabels();
    if (matrix) matrix->refresh();
    if (fxMatrix) fxMatrix->refresh();
    repaint();
}

void LMWSynthEditor::redoStep() {
    if (!processor.redo()) return;
    refreshPatchInfo();
    updatePresetLabels();
    if (matrix) matrix->refresh();
    if (fxMatrix) fxMatrix->refresh();
    repaint();
}

void LMWSynthEditor::removeAllCables() {
    const LMWSynthProcessor::ScopedStep step(processor, "remove all cables");
    for (int i = 0; i < lmw::dsp::numInputs; ++i)
        if (processor.currentPatch().isPatched(static_cast<lmw::dsp::In>(i))) connect(i, lmw::dsp::Patch::none);
}

juce::String LMWSynthEditor::describeOutput(lmw::dsp::Out out) const {
    const auto& patch = processor.currentPatch();
    juce::StringArray fed;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        if (!patch.isPatched(in)) {
            if (const float n = patch.normalLevel(in, out); n > 0.0f)   // Spec S5.17: 50 % each when parallel
                fed.add(portName(false, i) + (n < 1.0f ? " (normalled " + juce::String(juce::roundToInt(n * 100.0f)) + " %)" : " (normalled)"));
        } else if (patch.hasPin(in, out)) {
            const float l = patch.level(in, out);
            fed.add(portName(false, i) + (l != 1.0f ? " (cable " + juce::String(juce::roundToInt(l * 100.0f)) + " %)" : " (cable)"));
        }
    }
    const auto delayNote = lmw::ui::schematic::lfoDelayOutputNote(out, lfoDelays());   // S5.24
    return portName(true, static_cast<int>(out)) + " " + juce::String::fromUTF8("\xe2\x86\x92") + " "
         + (fed.isEmpty() ? juce::String("(not connected)") : fed.joinIntoString(", "))
         + (delayNote.empty() ? juce::String() : "\n" + juce::String::fromUTF8(delayNote.c_str()));
}

namespace {
// Spec S6.18 (Phase 25b): one connection of a socket as a row at the bottom of its menu — the output name, a
// horizontal fader for the level, the input name, the level as text. A modulation input's fader runs -100..+100 %
// (S5.21), others 0..100 %; a gate input has no level (on/off). The row follows the patch (10 times a second), so
// it shows changes made elsewhere and a normalled connection that became a cable.
class CableRow final : public juce::PopupMenu::CustomComponent, private juce::Timer {
public:
    struct Link {
        std::function<const lmw::dsp::Patch&()> patch;
        std::function<void(float)> set;                  // the new level (makes the cable(s) when normalled)
        std::function<void(bool)> drag;                  // drag started / ended (one undo step)
        juce::Colour sourceFill = kBg, targetFill = kBg;  // the units' colours (S6.9, user 2026-11-24)
    };
    CableRow(int input, int output, Link l) : juce::PopupMenu::CustomComponent(false), in(input), out(output), link(std::move(l)) {
        const auto inName = lmw::dsp::inputNames[static_cast<std::size_t>(in)];
        const auto outName = lmw::dsp::outputNames[static_cast<std::size_t>(out)];
        source.setText(juce::String(outName.data(), outName.size()), juce::dontSendNotification);
        target.setText(juce::String(inName.data(), inName.size()), juce::dontSendNotification);
        for (auto* lab : {&source, &target, &value}) {
            lab->setColour(juce::Label::textColourId, kFg);
            lab->setFont(juce::FontOptions(14.0f));
            lab->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*lab);
        }
        source.setColour(juce::Label::backgroundColourId, link.sourceFill);   // black text on the unit colour (S6.3)
        target.setColour(juce::Label::backgroundColourId, link.targetFill);
        source.setJustificationType(juce::Justification::centredRight);
        value.setJustificationType(juce::Justification::centredRight);
        const auto i = static_cast<lmw::dsp::In>(in);
        gate = i == lmw::dsp::In::Env1Gate || i == lmw::dsp::In::Env2Gate;
        fader.setSliderStyle(juce::Slider::LinearHorizontal);
        fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange(lmw::dsp::bipolar(i) ? -100.0 : 0.0, 100.0, 1.0);
        fader.setDoubleClickReturnValue(true, 100.0);
        fader.setTitle("Level " + source.getText() + " to " + target.getText());
        fader.setTooltip(lmw::dsp::bipolar(i) ? "Cable level, -100 to +100 % (below 0 the source is inverted). Double-click: 100 %."
                                              : "Cable level, 0 to 100 %. Double-click: 100 %.");
        fader.onDragStart = [this] { dragging = true; if (link.drag) link.drag(true); };
        fader.onDragEnd = [this] { dragging = false; if (link.drag) link.drag(false); };
        fader.onValueChange = [this] {
            if (syncing) return;
            link.set(static_cast<float>(fader.getValue()) / 100.0f);
            sync();
        };
        if (!gate) addAndMakeVisible(fader);
        sync();
        startTimerHz(10);
    }
    void getIdealSize(int& w, int& h) override { w = 470; h = 30; }
    void resized() override {
        auto r = getLocalBounds().reduced(6, 3);
        source.setBounds(r.removeFromLeft(100));
        value.setBounds(r.removeFromRight(54));
        target.setBounds(r.removeFromRight(gate ? 260 : 160));
        fader.setBounds(r.reduced(8, 0));
    }
    void paint(juce::Graphics& g) override { g.fillAll(kBg); }
private:
    void timerCallback() override { if (!dragging) sync(); }
    void sync() {
        const auto& p = link.patch();
        const auto i = static_cast<lmw::dsp::In>(in);
        const auto o = static_cast<lmw::dsp::Out>(out);
        const bool cable = p.hasPin(i, o);
        const float level = cable ? p.level(i, o) : p.isPatched(i) ? 0.0f : p.normalLevel(i, o);
        const auto inName = lmw::dsp::inputNames[static_cast<std::size_t>(in)];
        target.setText(juce::String(inName.data(), inName.size()) + (cable ? "" : p.isPatched(i) ? " (removed)" : " (normalled)")
                           + (gate ? " (on/off)" : ""), juce::dontSendNotification);
        value.setText(gate ? juce::String(cable || !p.isPatched(i) ? "on" : "off")
                           : juce::String(juce::roundToInt(level * 100.0f)) + " %", juce::dontSendNotification);
        const juce::ScopedValueSetter<bool> s(syncing, true);
        fader.setValue(std::round(level * 100.0f), juce::dontSendNotification);
    }
    int in, out;
    Link link;
    bool gate = false, dragging = false, syncing = false;
    juce::Label source, target, value;
    juce::Slider fader;
};
}

// Spec S6.18: the level of one connection; a normalled input's normal connection becomes cables first (all its
// normal sources at their normal levels, e.g. formant.in in Parallel: both VCFs at 50 %), then this one is set.
void LMWSynthEditor::setCableLevel(int input, int output, float level) {
    const auto in = static_cast<lmw::dsp::In>(input);
    const auto& p = processor.currentPatch();
    if (!p.isPatched(in)) {
        const auto ns = p.normals(in);
        for (int k = 0; k < ns.n; ++k)
            if (const auto& x = ns.s[static_cast<std::size_t>(k)]; x.source != lmw::dsp::noSource)
                connect(input, static_cast<int>(x.source), x.level);
    }
    connect(input, output, level);
}

// Spec S6.18: a level typed for one connection (Enter on its row in the socket menu); a normalled one becomes cables.
void LMWSynthEditor::typeCableLevel(int input, int output) {
    using lmw::dsp::In;
    const auto in = static_cast<In>(input);
    const auto& p = processor.currentPatch();
    const float now = p.hasPin(in, static_cast<lmw::dsp::Out>(output)) ? p.level(in, static_cast<lmw::dsp::Out>(output))
                    : p.isPatched(in) ? 0.0f : p.normalLevel(in, static_cast<lmw::dsp::Out>(output));
    const bool inverts = lmw::dsp::bipolar(in);
    auto* w = new juce::AlertWindow("Cable level", portName(true, output) + " " + juce::String::fromUTF8("\xe2\x86\x92") + " "
                                        + portName(false, input) + (inverts ? "\nLevel in percent, -100 to 100 (below 0: inverted):"
                                                                            : "\nLevel in percent, 0 to 100:"),
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("level", juce::String(juce::roundToInt(now * 100.0f)));
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), w, input, output, inverts](int result) {
            if (safe == nullptr || result != 1) return;
            const auto text = w->getTextEditorContents("level").trim().trimCharactersAtEnd("% ");
            if (text.isEmpty() || !text.containsOnly("-0123456789.")) return;
            const float l = juce::jlimit(inverts ? -1.0f : 0.0f, 1.0f, static_cast<float>(text.getDoubleValue()) / 100.0f);
            const LMWSynthProcessor::ScopedStep step(safe->processor, "cable level");   // Spec S6.14
            safe->setCableLevel(input, output, l);
        }), true);
    if (auto* field = w->getTextEditor("level")) { field->grabKeyboardFocus(); field->selectAll(); }
}

void LMWSynthEditor::showPortMenu(bool output, int port, juce::Rectangle<int> screenArea) {
    using lmw::dsp::In;
    using lmw::dsp::Out;
    const auto& patch = processor.currentPatch();
    bool anyCable = false;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) anyCable = anyCable || patch.isPatched(static_cast<In>(i));

    juce::PopupMenu menu, sub;
    menu.setLookAndFeel(&lookAndFeel);               // black on white (Spec S6.2)
    sub.setLookAndFeel(&lookAndFeel);
    if (!output) {
        const auto in = static_cast<In>(port);
        menu.addSectionHeader(describeInput(in));
        const bool full = patch.pinCount(in) >= lmw::dsp::Patch::maxPins;
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) {           // only possible items (no greyed ones, contrast)
            const bool cabled = patch.hasPin(in, static_cast<Out>(o));
            if (!lmw::dsp::canConnect(in, static_cast<Out>(o))) continue;   // Spec S7.6: the other domain
            if (cabled || !full) sub.addItem(100 + o, portName(true, o), true, cabled);
        }
        menu.addSubMenu("Connect from (ticked = cable; choose again to remove)", sub);
        if (patch.isPatched(in)) menu.addItem(1, patch.pinCount(in) > 1 ? "Remove all cables here" : "Remove cable");
    } else {
        const auto out = static_cast<Out>(port);
        bool feeds = false;
        menu.addSectionHeader("Output " + portName(true, port));
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto in = static_cast<In>(i);
            const bool cabled = patch.hasPin(in, out);
            feeds = feeds || cabled;
            if (!lmw::dsp::canConnect(in, out)) continue;           // Spec S7.6: the other domain
            if (cabled || patch.pinCount(in) < lmw::dsp::Patch::maxPins) sub.addItem(200 + i, portName(false, i), true, cabled);
        }
        menu.addSubMenu("Connect to (ticked = cable; choose again to remove)", sub);
        if (feeds) menu.addItem(2, "Remove cables from here");
    }
    if (anyCable) {
        menu.addSeparator();
        menu.addItem(3, "Remove all cables");
    }
    if (currentTab == 4 && !schematic->isLayoutDefault()) {   // Phase 12c
        menu.addSeparator();
        menu.addItem(4, "Reset schematic layout");
    }
    // Spec S6.18: a fader row per connection of this socket — its cables, or the normalled connections they replace.
    {
        std::vector<std::pair<int, int>> rows;                // (input, output)
        const auto addInput = [&](In in, std::optional<Out> only) {
            if (patch.isPatched(in)) {
                for (int k = 0; k < patch.pinCount(in); ++k) {
                    const auto o = static_cast<Out>(patch.pin(in, k).source);
                    if (!only || o == *only) rows.push_back({static_cast<int>(in), static_cast<int>(o)});
                }
            } else {
                const auto ns = patch.normals(in);
                for (int k = 0; k < ns.n; ++k) {
                    const auto o = ns.s[static_cast<std::size_t>(k)].source;
                    if (o != lmw::dsp::noSource && (!only || o == *only)) rows.push_back({static_cast<int>(in), static_cast<int>(o)});
                }
            }
        };
        if (!output) addInput(static_cast<In>(port), std::nullopt);
        else for (int i = 0; i < lmw::dsp::numInputs; ++i) addInput(static_cast<In>(i), static_cast<Out>(port));
        if (!rows.empty()) {
            menu.addSeparator();
            menu.addSectionHeader(rows.size() == 1 ? "Level" : "Levels");
            const auto safe = juce::Component::SafePointer<LMWSynthEditor>(this);
            // Without a mouse (a menu does not pass keys to its rows): Type a level ▸ one item per connection.
            juce::PopupMenu typed;
            typed.setLookAndFeel(&lookAndFeel);
            for (const auto& [in, out] : rows)
                typed.addItem(1000 + in * 100 + out, portName(true, out) + " " + juce::String::fromUTF8("\xe2\x86\x92") + " " + portName(false, in));
            menu.addSubMenu("Type a level", typed);
            for (const auto& [in, out] : rows) {
                CableRow::Link link;
                link.patch = [safe]() -> const lmw::dsp::Patch& {
                    static const lmw::dsp::Patch none;               // the editor closed while the menu was open
                    return safe != nullptr ? safe->processor.currentPatch() : none;
                };
                link.set = [safe, in = in, out = out](float level) { if (safe != nullptr) safe->setCableLevel(in, out, level); };
                // User 2026-11-24: the names on their unit's colour, as in the panel titles and the Matrix (S6.9).
                link.sourceFill = toJuce(palette.unitColour(lmw::ui::theme::unitOf(lmw::dsp::outputNames[static_cast<std::size_t>(out)])));
                link.targetFill = toJuce(palette.unitColour(lmw::ui::theme::unitOf(lmw::dsp::inputNames[static_cast<std::size_t>(in)])));
                link.drag = [safe](bool started) {               // Spec S6.14: one drag, one step
                    if (safe == nullptr) return;
                    if (started) safe->processor.beginStep("cable level");
                    else safe->processor.endStep();
                };
                menu.addCustomItem(5000 + in * 100 + out, std::make_unique<CableRow>(in, out, std::move(link)), nullptr,
                                   "Level " + portName(true, out) + " to " + portName(false, in));
            }
        }
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea),
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), output, port, focus = focusNow()](int r) {
        if (safe != nullptr) safe->restoreFocus(focus);
        if (safe == nullptr || r == 0) return;
        const auto& p = safe->processor.currentPatch();
        if (r == 1) {
            safe->connect(port, lmw::dsp::Patch::none);
        } else if (r == 2) {
            for (int i = 0; i < lmw::dsp::numInputs; ++i)
                if (p.hasPin(static_cast<In>(i), static_cast<lmw::dsp::Out>(port))) safe->connect(i, port, lmw::dsp::removePin);
        } else if (r == 4) {
            safe->schematic->resetLayout();
        } else if (r == 3) {
            safe->removeAllCables();
        } else if (!output && r >= 100 && r < 100 + lmw::dsp::numOutputs) {    // toggle that cable
            const bool cabled = p.hasPin(static_cast<In>(port), static_cast<lmw::dsp::Out>(r - 100));
            safe->connect(port, r - 100, cabled ? lmw::dsp::removePin : 1.0f);
        } else if (output && r >= 200 && r < 200 + lmw::dsp::numInputs) {
            const bool cabled = p.hasPin(static_cast<In>(r - 200), static_cast<lmw::dsp::Out>(port));
            safe->connect(r - 200, port, cabled ? lmw::dsp::removePin : 1.0f);
        } else if (r >= 1000 && r < 5000) {                    // Spec S6.18: Type a level ▸ a connection
            safe->typeCableLevel((r - 1000) / 100, (r - 1000) % 100);
        }
    });
}

// ---- Spec S6.3: contrast settings and config file ----

juce::File LMWSynthEditor::configFile() {
    const auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    return base.getChildFile("Application Support/LMW-Synth/config.ini");
#else
    return base.getChildFile("LMW-Synth/config.ini");   // Linux: ~/.config/..., Windows: %APPDATA%\\...
#endif
}

void LMWSynthEditor::saveConfig() const {
    const auto file = configFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::String(lmw::ui::writeConfig(settings)));
#if JUCE_LINUX || JUCE_MAC || JUCE_BSD
    // Spec S12.6: with the API key in it, the file is the user's only.
    if (!settings.assistantKey.empty()) ::chmod(file.getFullPathName().toRawUTF8(), 0600);
#endif
}

// ---- Spec S12.6: the Assistant settings ----

std::string LMWSynthEditor::assistantKey() {
    const bool keychain = settings.assistantStore == "keychain"
                          || (settings.assistantStore.empty() && lmw::keystore::available());
    if (!keychain) return settings.assistantKey;
    if (!keyCache) {
        std::string error;
        keyCache = lmw::keystore::load(error).value_or(std::string());
    }
    return *keyCache;
}

void LMWSynthEditor::showAssistantSettings() {
    if (assistantWindow != nullptr) { assistantWindow->toFront(true); return; }
    AssistantSettings::Values v;
    const bool hasKeychain = lmw::keystore::available();
    v.store = settings.assistantStore.empty() ? (hasKeychain ? "keychain" : "file") : settings.assistantStore;
    v.key = assistantKey();
    if (!settings.assistantModel.empty()) v.model = settings.assistantModel;
    if (!settings.assistantEffort.empty()) v.effort = settings.assistantEffort;
    v.limit = settings.assistantLimit.value_or(2.0);
    auto dialog = std::make_unique<AssistantSettings>(v, hasKeychain, AssistantView::privacyText());
    juce::Component::SafePointer<LMWSynthEditor> safe(this);
    const auto close = [safe] {
        if (safe != nullptr && safe->assistantWindow != nullptr) safe->assistantWindow->exitModalState(0);
    };
    dialog->onCancel = close;
    dialog->onHowTo = [safe] { if (safe != nullptr) safe->showLesson(12); };
    dialog->onOk = [safe, close](const AssistantSettings::Values& n) {
        if (safe == nullptr) return;
        auto& s = safe->settings;
        std::string error;
        if (n.store == "keychain") {
            const bool ok = n.key.empty() ? lmw::keystore::clear(error) : lmw::keystore::save(n.key, error);
            if (!ok) {                                     // keep the dialog open: the musician can choose the file
                juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                                                 .withTitle("Key not saved").withMessage(juce::String::fromUTF8(error.c_str())
                                                     + "\n\nChoose \"the settings file\" to keep the key there instead.")
                                                 .withButton("OK", 1).withAssociatedComponent(safe->assistantWindow.getComponent()),
                                             nullptr);
                return;
            }
            s.assistantKey.clear();
        } else {
            s.assistantKey = n.key;
            if (lmw::keystore::available()) lmw::keystore::clear(error);   // not in two places
        }
        s.assistantStore = n.store;
        s.assistantModel = n.model;
        s.assistantEffort = n.effort;
        s.assistantLimit = n.limit;
        safe->keyCache = n.key;
        safe->saveConfig();
        if (safe->assistant) safe->assistant->ready();
        close();
    };
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Assistant settings";
    o.dialogBackgroundColour = lmw::app::kBg;
    o.content.setOwned(dialog.release());
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = false;
    assistantWindow = o.launchAsync();
    if (assistantWindow != nullptr) assistantWindow->setLookAndFeel(&lookAndFeel);
}

// A tutorial lesson as text in its own window (Spec S12.6: "How to get a key"); the notes stay as they are.
void LMWSynthEditor::showLesson(int number) {
    if (lessonWindow != nullptr) { lessonWindow->toFront(true); return; }
    auto text = std::make_unique<juce::TextEditor>();
    text->setMultiLine(true, true);
    text->setReadOnly(true);
    text->setCaretVisible(false);
    text->setScrollbarsShown(true);
    text->setFont(juce::FontOptions(16.0f));
    text->setColour(juce::TextEditor::backgroundColourId, lmw::app::kBg);
    text->setColour(juce::TextEditor::textColourId, lmw::app::kFg);
    text->setText(juce::String::fromUTF8(lmw::tutorial::plain(lmw::tutorial::section(number)).c_str()), false);
    text->setTitle("Tutorial lesson " + juce::String(number));
    text->setSize(760, 560);
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = juce::String::fromUTF8(lmw::tutorial::title(number).c_str());
    o.dialogBackgroundColour = lmw::app::kBg;
    o.content.setOwned(text.release());
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = true;
    lessonWindow = o.launchAsync();
    if (lessonWindow != nullptr) lessonWindow->setLookAndFeel(&lookAndFeel);
}

// Spec S6.11 (user 2026-11-24: "add a change history to the options menu", what's new per version): Changes.md
// of this build as text in its own window, newest first; the headings and list marks as plain text.
void LMWSynthEditor::showChanges() {
    if (changesWindow != nullptr) { changesWindow->toFront(true); return; }
    std::string md;
    for (const char* part : lmw::changes::textParts) md += part;
    if (const auto first = md.find("\n## "); first != std::string::npos) md.erase(0, first + 1);   // no file title / intro
    auto text = std::make_unique<juce::TextEditor>();
    text->setMultiLine(true, true);
    text->setReadOnly(true);
    text->setCaretVisible(false);
    text->setScrollbarsShown(true);
    text->setFont(juce::FontOptions(16.0f));
    text->setColour(juce::TextEditor::backgroundColourId, lmw::app::kBg);
    text->setColour(juce::TextEditor::textColourId, lmw::app::kFg);
    text->setText(juce::String::fromUTF8(lmw::tutorial::plain(md).c_str()), false);
    text->setTitle("Change history");
    text->setSize(820, 600);
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "LMW-Synth change history";
    o.dialogBackgroundColour = lmw::app::kBg;
    o.content.setOwned(text.release());
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = true;
    changesWindow = o.launchAsync();
    if (changesWindow != nullptr) changesWindow->setLookAndFeel(&lookAndFeel);
}

// Spec S7.10 (Phase 27b): the Terms window — Find, "N of M terms", Show all, the terms by topic, See also.
void LMWSynthEditor::showTerms() {
    if (termsWindow != nullptr) { termsWindow->toFront(true); return; }
    auto view = std::make_unique<TermsView>();
    auto* raw = view.get();
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "LMW-Synth terms";
    o.dialogBackgroundColour = lmw::app::kBg;
    o.content.setOwned(view.release());
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = true;
    termsWindow = o.launchAsync();
    if (termsWindow == nullptr) return;
    termsWindow->setLookAndFeel(&lookAndFeel);
    termsWindow->setResizeLimits(420, 320, 4000, 4000);
    juce::Component::SafePointer<TermsView> safe(raw);
    juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->findField().grabKeyboardFocus(); });
}

// Spec S6.11: the menu (header button; the same items in the standalone window's Options menu).
void LMWSynthEditor::addMenuItems(juce::PopupMenu& m) const {
    {                                                      // Spec S6.14: named, greyed when there is nothing
        const auto& h = processor.steps();
        juce::PopupMenu::Item undo(h.canUndo() ? "Undo " + juce::String::fromUTF8(h.undoName().c_str()) : juce::String("Undo"));
        undo.itemID = menuFirstId + 51;
        undo.isEnabled = h.canUndo();
        undo.shortcutKeyDescription = juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0).getTextDescriptionWithIcons();
        m.addItem(undo);
        juce::PopupMenu::Item redo(h.canRedo() ? "Redo " + juce::String::fromUTF8(h.redoName().c_str()) : juce::String("Redo"));
        redo.itemID = menuFirstId + 52;
        redo.isEnabled = h.canRedo();
        redo.shortcutKeyDescription = juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
                                          .getTextDescriptionWithIcons();
        m.addItem(redo);
        m.addSeparator();
    }
    m.addItem(menuFirstId + 1, "About LMW-Synth...");
    m.addItem(menuFirstId + 2, "Change history...");          // what changed in each version
    m.addItem(menuFirstId + 3, "Terms...");                   // Spec S7.10: the words LMW-Synth uses
    juce::PopupMenu contrast;                              // Spec S6.3, ticked: the current one
    const auto c = settings.contrast;
    contrast.addItem(menuFirstId + 11, "AA 4.5:1", true, c == lmw::ui::ContrastPreset::AA);
    contrast.addItem(menuFirstId + 12, "AAA 7:1", true, c == lmw::ui::ContrastPreset::AAA);
    contrast.addItem(menuFirstId + 13, "Max 21:1", true, c == lmw::ui::ContrastPreset::Max);
    char text[48];
    if (c == lmw::ui::ContrastPreset::Custom) std::snprintf(text, sizeof text, "Custom %.1f:1...", settings.customRatio);
    else std::snprintf(text, sizeof text, "Custom...");
    contrast.addItem(menuFirstId + 14, text, true, c == lmw::ui::ContrastPreset::Custom);
    m.addSubMenu("Contrast", contrast);
    juce::PopupMenu theme;                                 // Spec S6.13
    using lmw::ui::theme::ThemePreset;
    for (int t = 0; t < 4; ++t) {
        const auto n = lmw::ui::theme::themeNames[static_cast<std::size_t>(t)];
        theme.addItem(menuFirstId + 31 + t, juce::String(n.data(), n.size()), true, static_cast<int>(settings.theme) == t);
    }
    theme.addSeparator();                                  // Spec S6.29: the dark themes, and following the system
    for (int t = 4; t < 7; ++t) {
        const auto n = lmw::ui::theme::themeLabels[static_cast<std::size_t>(t)];
        theme.addItem(menuFirstId + 57 + t, juce::String(n.data(), n.size()), true, static_cast<int>(settings.theme) == t);
    }
    if (settings.theme == ThemePreset::Custom && settings.themeName.empty()) theme.addItem(menuFirstId + 35, "Custom", true, true);
    const auto saved = themeFiles();                       // Spec S6.13: the user's named themes
    if (!saved.isEmpty()) {
        theme.addSectionHeader("Your themes");
        for (int i = 0; i < saved.size() && i < 100; ++i) {
            const auto n = saved[i].getFileNameWithoutExtension();
            theme.addItem(menuFirstId + 100 + i, n, true,
                          settings.theme == ThemePreset::Custom && juce::String(settings.themeName) == n);
        }
    }
    theme.addSeparator();
    theme.addItem(menuFirstId + 40, "Panel gradient", true, settings.panelGradient);   // Spec S6.30
    theme.addSeparator();
    theme.addItem(menuFirstId + 36, "Edit colours...");
    theme.addItem(menuFirstId + 38, "Save theme as...");
    if (!saved.isEmpty()) {
        juce::PopupMenu del;
        for (int i = 0; i < saved.size() && i < 100; ++i) del.addItem(menuFirstId + 200 + i, saved[i].getFileNameWithoutExtension());
        theme.addSubMenu("Delete theme", del);
    }
    if (themeFolder().isDirectory()) theme.addItem(menuFirstId + 39, "Show themes folder");
    if (settings.theme != ThemePreset::Default) theme.addItem(menuFirstId + 37, "Reset to default");
    m.addSubMenu("Theme", theme);
    juce::PopupMenu::Item keys("Show keyboard");          // Spec S6.12
    keys.itemID = menuFirstId + 21;
    keys.isTicked = keyboardShown;
    keys.shortcutKeyDescription = juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0).getTextDescriptionWithIcons();
    m.addItem(keys);
    m.addItem(menuFirstId + 22, "Show fader scales", true, settings.showScales);   // Spec S6.20: beginner aids
    m.addItem(menuFirstId + 23, "Show socket rings", true, settings.socketRings);
    if (assistant) m.addItem(menuFirstId + 41, "Assistant settings...");   // Spec S12.6 (standalone)
}

void LMWSynthEditor::handleMenuItem(int id) {
    switch (id - menuFirstId) {
        case 1: showAbout(); break;
        case 2: showChanges(); break;
        case 3: showTerms(); break;
        case 11: case 12: case 13:
            settings.contrast = static_cast<lmw::ui::ContrastPreset>(id - menuFirstId - 11);
            applyContrast();
            break;
        case 14: askCustomRatio(); break;
        case 21: setKeyboardShown(!keyboardShown); break;
        case 22: settings.showScales = !settings.showScales; saveConfig(); applyAids(); break;   // Spec S6.20
        case 23: settings.socketRings = !settings.socketRings; saveConfig(); applyAids(); break;
        case 40: settings.panelGradient = !settings.panelGradient; saveConfig(); repaint(); if (rackArea) rackArea->repaint(); break;   // S6.30
        case 51: undoStep(); break;
        case 52: redoStep(); break;
        case 41: showAssistantSettings(); break;
        case 31: case 32: case 33: case 34:                // Spec S6.13: a theme preset
        case 61: case 62: case 63:                         // S6.29: Dark, Dark high contrast, Follow system
            settings.theme = static_cast<lmw::ui::theme::ThemePreset>(id - menuFirstId - (id - menuFirstId >= 61 ? 57 : 31));
            settings.themeName.clear();
            applyContrast();
            break;
        case 36: editColours(); break;
        case 37:
            settings.theme = lmw::ui::theme::ThemePreset::Default;
            settings.custom = {};
            settings.customOpacity.reset();
            settings.themeName.clear();
            applyContrast();
            break;
        case 38: saveThemeAs(); break;
        case 39: themeFolder().revealToUser(); break;
        default:
            if (id >= menuFirstId + 100 && id < menuFirstId + 300) {        // a saved theme: use / delete
                const auto saved = themeFiles();
                const int i = (id - menuFirstId) % 100;
                if (i >= saved.size()) break;
                const auto f = saved[i];
                if (id < menuFirstId + 200) { applySavedTheme(f); break; }
                juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                    .withTitle("Delete theme").withMessage("Delete the theme \"" + f.getFileNameWithoutExtension() + "\"?")
                    .withButton("Delete", 1).withButton("Cancel", 0).withAssociatedComponent(this),
                    [safe = juce::Component::SafePointer<LMWSynthEditor>(this), f](int r) {
                        if (r != 1 || safe == nullptr) return;
                        f.deleteFile();
                        if (juce::String(safe->settings.themeName) == f.getFileNameWithoutExtension()) {
                            safe->settings.themeName.clear();   // the colours stay, as Custom
                            safe->applyContrast();
                        }
                    });
            }
            break;
    }
}

// Spec S6.13: an edit makes the theme Custom, starting from the colours shown; it is no longer a saved theme.
void LMWSynthEditor::makeThemeCustom() {
    auto& s = settings;
    if (s.theme != lmw::ui::theme::ThemePreset::Custom) {
        const auto now = s.colours(shownOsDark);
        s.customDark = now.dark;                             // Spec S6.29: Custom keeps a dark base
        for (std::size_t i = 0; i < s.custom.size(); ++i) s.custom[i] = lmw::ui::theme::itemColour(now, i);
        s.customOpacity = now.cableOpacity;
        s.theme = lmw::ui::theme::ThemePreset::Custom;
    }
    s.themeName.clear();
}

// Spec S6.13: saved themes, Documents/LMW-Synth Presets/THEME/<name>.lmwtheme, by name.
juce::File LMWSynthEditor::themeFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LMW-Synth Presets").getChildFile("THEME");
}
juce::Array<juce::File> LMWSynthEditor::themeFiles() {
    auto files = themeFolder().findChildFiles(juce::File::findFiles, false, "*.lmwtheme");
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });
    return files;
}
void LMWSynthEditor::applySavedTheme(const juce::File& f) {
    const auto c = lmw::ui::readTheme(f.loadFileAsString().toStdString());
    settings.theme = lmw::ui::theme::ThemePreset::Custom;
    for (std::size_t i = 0; i < settings.custom.size(); ++i) settings.custom[i] = lmw::ui::theme::itemColour(c, i);
    settings.customOpacity = c.cableOpacity;
    settings.customDark = c.dark;                          // Spec S6.29
    settings.themeName = f.getFileNameWithoutExtension().toStdString();
    applyContrast();
}
void LMWSynthEditor::saveThemeAs() {
    auto* w = new juce::AlertWindow("Save theme", "Name for the current colours (Theme > Your themes):",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&lookAndFeel);
    w->addTextEditor("name", juce::String(settings.themeName), "Name");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), w](int result) {
            if (safe == nullptr || result != 1) return;
            const auto name = w->getTextEditorContents("name").trim();
            if (name.isEmpty()) return;
            const auto file = themeFolder().getChildFile(juce::String::fromUTF8(lmw::presets::fileName(name.toStdString()).c_str())
                                                         + ".lmwtheme");
            auto write = [safe, file, name] {
                if (safe == nullptr) return;
                file.getParentDirectory().createDirectory();
                if (!file.replaceWithText(juce::String::fromUTF8(lmw::ui::writeTheme(name.toStdString(), safe->settings.colours()).c_str()))) {
                    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                        .withTitle("Save theme").withMessage("Cannot write " + file.getFullPathName()).withButton("OK", 1), nullptr);
                    return;
                }
                safe->makeThemeCustom();
                safe->settings.themeName = file.getFileNameWithoutExtension().toStdString();
                safe->applyContrast();
            };
            if (!file.existsAsFile()) { write(); return; }
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                .withTitle("Save theme").withMessage("A theme \"" + file.getFileNameWithoutExtension() + "\" exists. Replace it?")
                .withButton("Replace", 1).withButton("Cancel", 0).withAssociatedComponent(safe.getComponent()),
                [write](int r) { if (r == 1) write(); });
        }), true);
    if (auto* field = w->getTextEditor("name")) { field->grabKeyboardFocus(); field->selectAll(); }   // type at once
}

// Spec S6.13: Edit colours... — a change makes the theme Custom (starting from the colours shown) and
// applies at once.
void LMWSynthEditor::editColours() {
    if (colourWindow != nullptr) { colourWindow->toFront(true); return; }
    auto dialog = std::make_unique<ThemeDialog>(settings.colours(shownOsDark), settings.minRatio());
    dialog->onChange = [safe = juce::Component::SafePointer<LMWSynthEditor>(this)](std::size_t item, std::optional<lmw::ui::Rgb> c) {
        if (safe == nullptr) return;
        safe->makeThemeCustom();
        safe->settings.custom[item] = c ? *c : lmw::ui::theme::itemColour(lmw::ui::theme::presetColours(   // Reset: its base's colour
            safe->settings.customDark ? lmw::ui::theme::ThemePreset::Dark : lmw::ui::theme::ThemePreset::Default), item);
        safe->applyContrast();
    };
    dialog->onOpacity = [safe = juce::Component::SafePointer<LMWSynthEditor>(this)](float o) {
        if (safe == nullptr) return;
        safe->makeThemeCustom();
        safe->settings.customOpacity = o;
        safe->applyContrast();
    };
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Theme colours";
    o.dialogBackgroundColour = lmw::app::kBg;
    o.content.setOwned(dialog.release());
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = false;
    colourWindow = o.launchAsync();
    if (colourWindow != nullptr) colourWindow->setLookAndFeel(&lookAndFeel);
}

void LMWSynthEditor::showMainMenu() {
    juce::PopupMenu m;
    if (addWindowItems) { addWindowItems(m); m.addSeparator(); }
    addMenuItems(m);
    const auto back = focusNow();
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
                    [safe = juce::Component::SafePointer<LMWSynthEditor>(this), back](int result) {
                        if (safe == nullptr) return;
                        safe->restoreFocus(back);          // computer keys keep playing (S3.2)
                        if (result <= 0) return;
                        if (result >= menuFirstId) { safe->handleMenuItem(result); return; }
                        if (auto chosen = safe->windowItemChosen) chosen(result);   // a copy: Reset deletes this editor
                    });
}

void LMWSynthEditor::applyContrast() {
    shownOsDark = juce::Desktop::getInstance().isDarkModeActive();                  // S6.29: Follow system
    palette = lmw::ui::theme::palette(settings.minRatio(), settings.colours(shownOsDark));   // S6.3, S6.13, S6.29
    const juce::Colour oldFg = kFg, oldBg = kBg, oldQuiet = lmw::app::kQuiet;
    lmw::ui::theme::current() = palette;                    // S6.29: kFg / kBg and the widgets follow
    defaultLook.get().applyTheme();
    lookAndFeel.applyTheme();
    lmw::app::retintDesktop({{{oldFg, kFg}, {oldBg, kBg}, {oldQuiet, lmw::app::kQuiet}}});
    if (auto* top = getTopLevelComponent()) top->sendLookAndFeelChange();
    else sendLookAndFeelChange();
    resized();                                              // S6.29: the small fonts (tags) of this theme
    notesEditor.setTextToShowWhenEmpty("Notes for this patch: they are saved with it.", kFg);
    saveConfig();
    keyboard.repaint();
    if (matrix) matrix->setPalette(palette);
    if (fxMatrix) fxMatrix->setPalette(palette);
    if (schematic) schematic->setPalette(palette);
    for (auto& h : headers) h->repaint();
    repaint();
}

void LMWSynthEditor::askCustomRatio() {
    auto* w = new juce::AlertWindow("Custom contrast",
                                    "Minimum contrast ratio for every colour pair, 4.5 (AA) to 21 (black/white):",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel(&lookAndFeel);
    w->addTextEditor("ratio", juce::String(settings.customRatio, 1), "Ratio");
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), w](int result) {
            if (safe == nullptr) return;
            if (result == 1) {
                const double r = w->getTextEditorContents("ratio").getDoubleValue();
                if (r > 0.0) {
                    safe->settings.contrast = lmw::ui::ContrastPreset::Custom;
                    safe->settings.customRatio = lmw::ui::minimumRatio(lmw::ui::ContrastPreset::Custom, r);
                }
            }
            safe->applyContrast();
        }), true);
    if (auto* field = w->getTextEditor("ratio")) { field->grabKeyboardFocus(); field->selectAll(); }   // type at once
}

void LMWSynthEditor::Keyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                             bool isOver, juce::Colour lineColour, juce::Colour textColour) {
    if (isDown) {
        g.setColour(toJuce(pal.whiteKeyDown));
        g.fillRect(area);
        textColour = toJuce(lmw::ui::theme::textOn(pal.whiteKeyDown, pal.keyText, pal.whiteKey));   // as Palette::pairs (S6.29)
    } else if (isOver) {
        g.setColour(toJuce(pal.keyText));                // S6.29: dark on the light grey keys in Dark too
        g.drawRect(area.reduced(2.0f), 1.0f);
    }
    // Base class: note name and the separator lines (no fill: not down, not over).
    juce::MidiKeyboardComponent::drawWhiteNote(note, g, area, false, false, lineColour, textColour);
}

void LMWSynthEditor::Keyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                             bool isOver, juce::Colour noteFillColour) {
    g.setColour(isDown ? toJuce(pal.blackKeyDown) : noteFillColour);
    g.fillRect(area);
    if (isDown) {
        g.setColour(toJuce(pal.blackKey));              // keeps its edge where it meets white keys (S6.29)
        g.drawRect(area, 1.0f);
    } else if (isOver) {
        g.setColour(toJuce(pal.whiteKey));               // S6.29: the white keys' colour (the page is near-black in Dark)
        g.drawRect(area.reduced(2.0f), 1.0f);
    }
}

void LMWSynthEditor::Keyboard::drawUpDownButton(juce::Graphics& g, int w, int h, bool isMouseOver, bool isButtonPressed,
                                                bool movesOctavesUp) {
    g.fillAll(isButtonPressed ? kFg : kBg);
    juce::Path arrow;
    arrow.addTriangle(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f);
    if (!movesOctavesUp) arrow.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi, 0.5f, 0.5f));
    g.setColour(isButtonPressed ? kBg : kFg);
    g.fillPath(arrow, arrow.getTransformToScaleToFit(1.0f, 1.0f, static_cast<float>(w) - 2.0f, static_cast<float>(h) - 2.0f, true));
    if (isMouseOver && !isButtonPressed) {
        g.setColour(kFg);
        g.drawRect(0, 0, w, h, 1);
    }
}

// ---- Spec S6.2: standalone window title bar and buttons ----

void LMWLookAndFeel::drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h,
                                                int titleSpaceX, int titleSpaceW, const juce::Image* icon,
                                                bool drawTitleTextOnLeft) {
    LookAndFeel_V4::drawDocumentWindowTitleBar(window, g, w, h, titleSpaceX, titleSpaceW, icon, drawTitleTextOnLeft);
    g.setColour(kFg);
    g.fillRect(0, h - 1, w, 1);                       // separates the title bar from the panel
}

namespace {
// A title-bar button: black glyph on white; hover = outline, pressed = inverted (no colour tints).
class WindowButton final : public juce::Button {
public:
    WindowButton(const juce::String& name, juce::Path glyph) : juce::Button(name), shape(std::move(glyph)) {
        setTitle(name);
    }
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(down ? kFg : kBg);
        g.fillRect(r);
        if (over || hasKeyboardFocus(false)) { g.setColour(kFg); g.drawRect(r, 1.0f); }
        const auto glyphArea = r.reduced(r.getHeight() * 0.28f);
        g.setColour(down ? kBg : kFg);
        g.strokePath(shape, juce::PathStrokeType(1.6f),
                     shape.getTransformToScaleToFit(glyphArea, true));
    }
private:
    juce::Path shape;
};
}

juce::Button* LMWLookAndFeel::createDocumentWindowButton(int buttonType) {
    juce::Path p;
    if (buttonType == juce::DocumentWindow::closeButton) {
        p.startNewSubPath(0, 0); p.lineTo(1, 1); p.startNewSubPath(1, 0); p.lineTo(0, 1);
        return new WindowButton("Close", p);
    }
    if (buttonType == juce::DocumentWindow::minimiseButton) {
        p.startNewSubPath(0, 1); p.lineTo(1, 1); p.startNewSubPath(0, 0); p.lineTo(0, 0);   // keeps 1:1 box
        return new WindowButton("Minimise", p);
    }
    p.addRectangle(0.0f, 0.0f, 1.0f, 1.0f);           // maximise
    return new WindowButton("Maximise", p);
}

void LMWLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                       bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                       const juce::String& text, const juce::String& shortcutKeyText,
                                       const juce::Drawable* icon, const juce::Colour* textColour) {
    if (isSeparator) {
        g.setColour(kFg);
        g.fillRect(area.reduced(5, 0).withY(area.getCentreY()).withHeight(1));
        return;
    }
    LookAndFeel_V4::drawPopupMenuItem(g, area, false, isActive, isHighlighted, isTicked, hasSubMenu, text,
                                      shortcutKeyText, icon, textColour);
}

// ---- Spec S7: tabs, notes and patch files ----

int LMWSynthEditor::titleTextWidth() const {      // "LMW-Synth 0.1.0" in the title font, plus the label's border
    return juce::roundToInt(juce::GlyphArrangement::getStringWidth(titleButton.font(), titleButton.getButtonText())) + 12;
}

void LMWSynthEditor::TitleButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    g.setColour(kFg);
    g.setFont(font());
    const auto text = getLocalBounds().reduced(4, 0);
    g.drawText(getButtonText(), text, juce::Justification::centredLeft, false);
    if (highlighted || down) {                               // underline: it can be clicked
        const int width = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font(), getButtonText()));
        g.fillRect(text.getX(), getHeight() / 2 + juce::roundToInt(fontHeight * 0.55f), width, down ? 2 : 1);
    }
    if (hasKeyboardFocus(false)) g.drawRect(getLocalBounds(), 2);
}

// Spec S6.10: the About dialog (black on white like every alert): version, commit, licence, source, JUCE, 9P.
void LMWSynthEditor::showAbout() {
    const juce::String format = juce::AudioProcessor::getWrapperTypeDescription(processor.wrapperType);
    const auto endpoints = juce::String(processor.ninepEndpoints());
    juce::String text;
    text << "Version " LMW_VERSION " (commit " LMW_COMMIT "), " << format << "\n"
         << "A semi-modular software synthesizer.\n\n"
         << "Licence: GNU Affero General Public License, version 3 or later (AGPL-3.0-or-later). "
            "You may use, change and share it under that licence; it comes without any warranty.\n\n"
         << "Source: " << NinePService::sourceUrl << "\n"
         << "Built with " << juce::SystemStats::getJUCEVersion() << " (AGPL-3.0).\n"
         << "9P server: " << (endpoints.isEmpty() ? juce::String("not available") : endpoints.replace(" ", ", "));
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::NoIcon)
                                     .withTitle("About LMW-Synth").withMessage(text).withButton("OK", 1)
                                     .withAssociatedComponent(this), nullptr);
}

void LMWSynthEditor::showTab(int tab) {
    if (currentTab == 0 || currentTab == 1) tabScroll[static_cast<std::size_t>(currentTab)] = rackView.getViewPosition();   // S6.21
    currentTab = tab;
    const juce::Component* alwaysShown[] = {&titleButton, &instrumentButton, &panelTab, &effectsTab, &matrixTab, &schematicTab, &notesTab, &patchLabel, &loadLabel, &loadButton,
                                            &saveButton, &menuButton, &undoButton, &redoButton, &resetLayoutButton, &compactButton, &keyboard, &keyboardButton, &assistantTab,
                                            // Spec S6.24 (found 2026-11-26: they vanished on the other tabs): the playing controls follow
                                            // the keyboard (shown / hidden in resized()), not the tab
                                            &bendWheel, &modWheelCtl, &glideSlider, &glideModeBox, &bendRangeBox,
                                            &bendLabel, &modLabel, &glideLabel, &bendRangeLabel};
    for (auto* child : getChildren()) {
        if (child == &notesEditor || child == matrix.get() || child == schematic.get() || child == assistant.get()
            || child == &rackView || child == &schematicView || std::find(std::begin(alwaysShown), std::end(alwaysShown), child) != std::end(alwaysShown))
            continue;
        child->setVisible(false);
    }
    rackView.setVisible(tab == 0 || tab == 1);               // Spec S6.21: the rack scrolls in its view
    for (auto* child : rackArea->getChildren()) {
        if (child == cables.get()) continue;
        const bool effect = std::find(effectComponents.begin(), effectComponents.end(), child) != effectComponents.end();
        child->setVisible(effect ? tab == 1 : tab == 0);   // hidden controls take no Tab focus either
    }
    if (cables) cables->setVisible(tab == 0 || tab == 1);   // Spec S7.6: cables on the Effects tab too
    matrix->setVisible(tab == 2);
    schematicView.setVisible(tab == 4);
    notesEditor.setVisible(tab == 3);
    if (assistant) assistant->setVisible(tab == 5);
    resetLayoutButton.setEnabled(true);
    if (tab == 2) matrix->grabKeyboardFocus();
    if (tab == 3) notesEditor.grabKeyboardFocus();
    if (tab == 4) schematic->grabKeyboardFocus();
    if (tab == 5 && assistant) assistant->focusInput();
    if (tab == 0 || tab == 1) rackView.setViewPosition(tabScroll[static_cast<std::size_t>(tab)]);   // S6.21
    rackArea->repaint();
    repaint();
}

// Spec S6.22: the Schematic surface is its drawing (with a margin) or the view, whichever is larger: centred
// when it fits, scroll bars when it does not.
void LMWSynthEditor::layoutSchematic() {
    if (!schematic) return;
    const auto need = schematic->drawingSize();
    const auto area = schematicView.getBounds();
    const int bar = schematicView.getScrollBarThickness();
    const bool horz = need.x > area.getWidth();
    const bool vert = need.y > area.getHeight() - (horz ? bar : 0);
    const bool horz2 = need.x > area.getWidth() - (vert ? bar : 0);
    schematic->setSize(std::max(need.x, area.getWidth() - (vert ? bar : 0)), std::max(need.y, area.getHeight() - (horz2 ? bar : 0)));
}

// ---- Spec S6.24 (Phase 41): playing controls left of the keyboard ----

LMWSynthEditor::Wheel::Wheel(bool springsBack) : juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox), springBack(springsBack) {
    setTitle(springsBack ? "Pitch bend wheel" : "Mod wheel");
    setVelocityBasedMode(false);
}

void LMWSynthEditor::Wheel::paint(juce::Graphics& g) {
    // A wheel seen from the front: a rounded black frame, grey ridges, the position as a thick black bar (S6.2).
    const auto r = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(kBg);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(lmw::app::kQuiet);                             // ridges (decoration only; S6.29: either theme)
    for (float y = r.getY() + 6.0f; y < r.getBottom() - 4.0f; y += 6.0f)
        g.drawHorizontalLine(juce::roundToInt(y), r.getX() + 6.0f, r.getRight() - 6.0f);
    const double p = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    const float y = r.getBottom() - 6.0f - static_cast<float>(p) * (r.getHeight() - 12.0f);
    g.setColour(kFg);
    if (springBack) g.fillRect(r.getX() + 2.0f, r.getCentreY() - 0.5f, r.getWidth() - 4.0f, 1.0f);   // the centre
    g.fillRoundedRectangle(r.getX() + 3.0f, y - 3.0f, r.getWidth() - 6.0f, 6.0f, 2.0f);
    g.drawRoundedRectangle(r, 6.0f, hasKeyboardFocus(false) ? 3.0f : 1.5f);
}

void LMWSynthEditor::Wheel::mouseUp(const juce::MouseEvent& e) {
    juce::Slider::mouseUp(e);
    if (springBack) setValue(0.0, juce::sendNotificationSync);   // a bend wheel returns to the centre
}

void LMWSynthEditor::layoutPlayStrip(juce::Rectangle<int> a) {
    const int labelH = m.colW >= 80 ? 16 : 13, wheelW = m.colW >= 80 ? 34 : 28, gap = m.gap;
    const auto column = [&](int w) { auto c = a.removeFromLeft(w); a.removeFromLeft(gap); return c; };
    for (auto [wheel, label] : {std::pair<juce::Component*, juce::Label*>{&bendWheel, &bendLabel}, {&modWheelCtl, &modLabel}}) {
        auto c = column(wheelW + 10);
        label->setBounds(c.removeFromTop(labelH));
        wheel->setBounds(c.withSizeKeepingCentre(wheelW, c.getHeight()));
    }
    {   // Glide: a horizontal slider with its time on the right, the mode box below
        auto c = column(m.colW >= 80 ? 170 : 140);
        glideLabel.setBounds(c.removeFromTop(labelH));
        glideSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, m.colW >= 80 ? 64 : 54, m.boxH - 4);
        glideSlider.setBounds(c.removeFromTop(m.boxH));
        c.removeFromTop(4);
        glideModeBox.setBounds(c.removeFromTop(m.boxH).withWidth(m.colW >= 80 ? 90 : 76));
    }
    {
        auto c = column(m.colW >= 80 ? 72 : 60);
        bendRangeLabel.setBounds(c.removeFromTop(labelH));
        bendRangeBox.setBounds(c.removeFromTop(m.boxH));
    }
}

// ---- Spec S6.21 (Phase 38): panning the rack ----

void LMWSynthEditor::RackArea::mouseDown(const juce::MouseEvent& e) {
    // Only on the empty surface (the grey between and beside the panels): a hand that moves the view.
    panning = e.eventComponent == this && !e.mods.isPopupMenu();
    viewAtStart = editor.rackView.getViewPosition();
    if (panning) setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void LMWSynthEditor::RackArea::mouseDrag(const juce::MouseEvent& e) {
    if (!panning) return;
    // The view moves the other way than the hand; offsets on screen, as the surface itself moves under the mouse.
    const auto moved = e.getScreenPosition() - e.getMouseDownScreenPosition();
    editor.rackView.setViewPosition(viewAtStart - moved);
}

void LMWSynthEditor::RackArea::mouseUp(const juce::MouseEvent&) {
    panning = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void LMWSynthEditor::scrollIntoView(const juce::Component& c) {
    if (!rackView.isVisible() || !rackArea->isParentOf(&c)) return;
    const auto r = rackArea->getLocalArea(&c, c.getLocalBounds());
    const auto view = rackView.getViewArea();
    if (view.contains(r)) return;
    auto pos = view.getPosition();
    if (r.getX() < view.getX()) pos.x = r.getX() - m.gap;
    else if (r.getRight() > view.getRight()) pos.x = r.getRight() - view.getWidth() + m.gap;
    if (r.getY() < view.getY()) pos.y = r.getY() - m.gap;
    else if (r.getBottom() > view.getBottom()) pos.y = r.getBottom() - view.getHeight() + m.gap;
    rackView.setViewPosition(pos);
}

void LMWSynthEditor::globalFocusChanged(juce::Component* focused) {
    if (focused != nullptr && rackArea != nullptr && rackArea->isParentOf(focused)) scrollIntoView(*focused);
}

void LMWSynthEditor::autoScrollRack(juce::Point<int> inRack) {
    const auto inView = inRack - rackView.getViewPosition();
    rackView.autoScroll(inView.x, inView.y, 24, 12);
}

void LMWSynthEditor::refreshPatchInfo() {
    shownPatchFile = processor.patchFileVersion();
    const auto name = processor.patchName();
    juce::ignoreUnused(name);
    updatePatchLabel();
    if (notesEditor.getText() != processor.notes()) notesEditor.setText(processor.notes(), false);
}

// ---- Spec S7.5: module presets ----

LMWSynthEditor::PresetButton::PresetButton(LMWSynthEditor& e, juce::String m, juce::String t)
    : juce::Button(t + " presets"), module(std::move(m)), panelTitle(std::move(t)), editor(e) {
    setTitle(panelTitle + " presets");
    setDescription("Opens the preset menu of this panel.");
    setTooltip("Presets for " + panelTitle + ": instrument starting points, Default, your own; Save as preset...");
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
}

int LMWSynthEditor::PresetButton::preferredWidth() const {
    return juce::roundToInt(juce::GlyphArrangement::getStringWidth(juce::FontOptions(editor.m.titleFont * 0.85f), "Presets")
                            + editor.m.titleFont * 1.6f) + 4;
}

// Black text, a black frame and a black triangle on the title colour (no fill: the bar shows through);
// pressed: white on black. Hover or focus: a thicker frame.
void LMWSynthEditor::PresetButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto b = getLocalBounds();
    if (down) g.fillAll(kFg);
    g.setColour(down ? kBg : kFg);
    g.drawRect(b, highlighted || hasKeyboardFocus(false) ? 2 : 1);
    const float h = editor.m.titleFont * 0.85f;
    auto area = b.toFloat().reduced(4.0f, 0.0f);
    const auto arrowArea = area.removeFromRight(h * 0.8f);
    g.setFont(juce::FontOptions(h));
    g.drawText("Presets", area, juce::Justification::centred, false);
    juce::Path triangle;
    const float cx = arrowArea.getCentreX(), cy = arrowArea.getCentreY(), w = h * 0.3f;
    triangle.addTriangle(cx - w, cy - w * 0.5f, cx + w, cy - w * 0.5f, cx, cy + w * 0.6f);
    g.fillPath(triangle);
}

void LMWSynthEditor::PresetButton::clicked() { editor.showPresetMenu(*this); }

juce::String LMWSynthEditor::PresetButton::getTooltip() {
    const auto label = editor.presetLabel(module);
    return (label.isEmpty() ? juce::String() : "Preset: " + label + (label.endsWith("*") ? " (changed since)" : "") + "\n")
         + juce::Button::getTooltip();
}

juce::String LMWSynthEditor::presetModuleOf(const juce::String& panelTitle) {
    const auto words = juce::StringArray::fromTokens(panelTitle, " ", "");
    if (words.isEmpty()) return {};
    const auto first = words[0].toLowerCase();
    const auto module = words.size() > 1 && words[1].containsOnly("0123456789") ? first + words[1] : first;
    return lmw::presets::kindOf(module.toStdString()) ? module : juce::String();
}

juce::File LMWSynthEditor::presetFolder(lmw::presets::Kind kind) {
    const auto k = lmw::presets::folderName(kind);
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LMW-Synth Presets")
        .getChildFile(juce::String(k.data(), k.size()));
}

void LMWSynthEditor::showPresetMenu(PresetButton& button) {
    const auto kind = lmw::presets::kindOf(button.module.toStdString());
    if (!kind) return;
    const auto& builtIn = lmw::presets::builtIn(*kind);
    const auto folder = presetFolder(*kind);
    auto own = folder.findChildFiles(juce::File::findFiles, false, "*.lmwpreset");
    std::sort(own.begin(), own.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);              // black on white (Spec S6.2); no greyed-out items
    menu.addSectionHeader(button.panelTitle + " presets");
    menu.addItem(1, "Default");
    for (std::size_t i = 0; i < builtIn.size(); ++i)
        menu.addItem(100 + static_cast<int>(i), juce::String::fromUTF8(builtIn[i].name.c_str()));
    if (!own.isEmpty()) {
        menu.addSectionHeader("Your presets");
        for (int i = 0; i < own.size(); ++i) menu.addItem(1000 + i, own[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem(2, "Save as preset...");
    if (folder.isDirectory()) menu.addItem(3, "Show presets folder");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&button),
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), module = button.module, title = button.panelTitle,
         kind = *kind, own, folder, focus = focusNow()](int result) {
            if (safe != nullptr) safe->restoreFocus(focus);
            if (safe == nullptr || result == 0) return;
            const auto& list = lmw::presets::builtIn(kind);
            if (result == 1) {
                safe->applyDefaults(module);
            } else if (result == 2) {
                safe->savePresetAs(module, title);
            } else if (result == 3) {
                folder.revealToUser();
            } else if (result >= 100 && result < 100 + static_cast<int>(list.size())) {
                safe->applyPreset(module, list[static_cast<std::size_t>(result - 100)]);
            } else if (result >= 1000 && result < 1000 + own.size()) {
                const auto file = own[result - 1000];
                safe->processor.renameStep("preset " + file.getFileNameWithoutExtension().toStdString());
                std::string error;
                auto preset = lmw::presets::parse(kind, file.loadFileAsString().toStdString(), &error);
                if (!preset) { safe->showError("Cannot use this preset", file.getFileName() + ": " + juce::String(error)); return; }
                safe->applyPreset(module, *preset);
            }
        });
}

namespace {
// "vcf1" -> "VCF 1", "delay" -> "DELAY": the panel title, for undo step names (S6.14).
juce::String moduleTitle(const juce::String& module) {
    auto t = module.toUpperCase();
    if (t.length() > 1 && juce::CharacterFunctions::isDigit(t.getLastCharacter()))
        t = t.dropLastCharacters(1) + " " + juce::String::charToString(t.getLastCharacter());
    return t;
}
} // namespace

// Sets the module's parameters from the preset's texts, as a patch file would (S7.1): numbers in the
// parameter's unit, clipped to its range; choices by name. Keys the preset lacks stay as they are.
void LMWSynthEditor::applyPreset(const juce::String& module, const lmw::presets::Preset& preset) {
    const LMWSynthProcessor::ScopedStep step(processor, moduleTitle(module) + " preset " + juce::String::fromUTF8(preset.name.c_str()));
    juce::StringArray bad;
    for (const auto& [key, value] : preset.values) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(processor.state().getParameter(module + "." + juce::String(key)));
        const auto text = juce::String::fromUTF8(value.c_str());
        const auto normalised = r != nullptr ? LMWSynthProcessor::parseValue(*r, text) : std::nullopt;
        if (!normalised) { bad.add(juce::String(key) + " = " + text); continue; }
        processor.rememberText(*r, value);              // reads back as in the preset (S10.10)
        LMWSynthProcessor::setNormalised(*r, *normalised);
    }
    juce::StringArray values;                           // Spec S7.5: remembered for the title-bar label
    for (const auto& [key, value] : preset.values) values.add(juce::String(key) + "=" + juce::String::fromUTF8(value.c_str()));
    processor.setPresetRecord(module, juce::String::fromUTF8(preset.name.c_str()), values.joinIntoString(";"));
    updatePresetLabels();
    if (!bad.isEmpty())
        showError("Preset partly used", "These values were not understood and were left as they were:\n" + bad.joinIntoString("\n"));
}

void LMWSynthEditor::applyDefaults(const juce::String& module) {
    const auto kind = lmw::presets::kindOf(module.toStdString());
    if (!kind) return;
    const LMWSynthProcessor::ScopedStep step(processor, moduleTitle(module) + " Default");
    for (auto key : lmw::presets::keys(*kind))
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(processor.state().getParameter(module + "." + juce::String(key.data(), key.size()))))
            LMWSynthProcessor::setNormalised(*r, r->getDefaultValue());
    processor.setPresetRecord(module, "Default", {});
    updatePresetLabels();
}

namespace {
// Whether parameter r stands at the value the text sets (as a patch file would set it).
bool atText(juce::RangedAudioParameter& r, const juce::String& text) {
    const auto n = LMWSynthProcessor::parseValue(r, text);
    if (!n) return false;
    const auto& range = r.getNormalisableRange();
    const float want = range.convertFrom0to1(*n), have = range.convertFrom0to1(r.getValue());
    return std::fabs(want - have) <= 1e-6f * (range.end - range.start);
}
}

bool LMWSynthEditor::moduleMatches(const juce::String& module, const lmw::presets::Preset& preset) const {
    for (const auto& [key, value] : preset.values) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(processor.state().getParameter(module + "." + juce::String(key)));
        if (r != nullptr && !atText(*r, juce::String::fromUTF8(value.c_str()))) return false;
    }
    return true;
}

bool LMWSynthEditor::moduleAtDefaults(const juce::String& module) const {
    const auto kind = lmw::presets::kindOf(module.toStdString());
    if (!kind) return false;
    for (auto key : lmw::presets::keys(*kind))
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(processor.state().getParameter(module + "." + juce::String(key.data(), key.size())))) {
            const auto& range = r->getNormalisableRange();
            if (std::fabs(range.convertFrom0to1(r->getDefaultValue()) - range.convertFrom0to1(r->getValue()))
                > 1e-6f * (range.end - range.start))
                return false;
        }
    return true;
}

// The last chosen preset, with "*" once a value differs from what it set. Without one (new instance,
// loaded patch): a built-in preset the module matches exactly, else "Default" / "Default*".
juce::String LMWSynthEditor::computePresetLabel(const juce::String& module) const {
    const auto kind = lmw::presets::kindOf(module.toStdString());
    if (!kind) return {};
    const auto name = processor.presetName(module);
    if (name.isNotEmpty() && name != "Default") {
        lmw::presets::Preset p;
        for (const auto& pair : juce::StringArray::fromTokens(processor.presetValues(module), ";", ""))
            if (pair.contains("="))
                p.values.emplace_back(pair.upToFirstOccurrenceOf("=", false, false).toStdString(),
                                      pair.fromFirstOccurrenceOf("=", false, false).toStdString());
        return moduleMatches(module, p) ? name : name + "*";
    }
    if (name.isEmpty())
        for (const auto& p : lmw::presets::builtIn(*kind))
            if (moduleMatches(module, p)) return juce::String::fromUTF8(p.name.c_str());
    return moduleAtDefaults(module) ? juce::String("Default") : juce::String("Default*");
}

void LMWSynthEditor::updatePresetLabels() {
    for (std::size_t i = 0; i < groups.size(); ++i)
        if (const auto module = presetModuleOf(groups[i].title); module.isNotEmpty())
            if (auto label = computePresetLabel(module); presetLabels[module] != label) {
                presetLabels[module] = label;
                headers[i]->repaint();
            }
    for (const auto& grp : effectGroups)
        if (const auto module = presetModuleOf(grp.title); module.isNotEmpty())
            if (auto label = computePresetLabel(module); presetLabels[module] != label) {
                presetLabels[module] = label;
                repaint(grp.bounds.withHeight(m.header));
            }
}

juce::String LMWSynthEditor::presetLabel(const juce::String& module) const {
    const auto it = presetLabels.find(module);
    return it != presetLabels.end() ? it->second : juce::String();
}

void LMWSynthEditor::savePresetAs(const juce::String& module, const juce::String& panelTitle) {
    const auto kind = lmw::presets::kindOf(module.toStdString());
    if (!kind) return;
    const auto k = lmw::presets::folderName(*kind);
    auto* w = new juce::AlertWindow("Save " + panelTitle + " preset",
                                    "Name of the preset (it can be used on every " + juce::String(k.data(), k.size()) + " panel):",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", {});
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), w, module, kind = *kind](int result) {
            if (safe == nullptr || result != 1) return;
            const auto name = w->getTextEditorContents("name").trim();
            if (name.isEmpty()) return;
            const auto file = presetFolder(kind).getChildFile(juce::String::fromUTF8(lmw::presets::fileName(name.toStdString()).c_str())
                                                              + ".lmwpreset");
            if (!file.existsAsFile()) { safe->writePreset(module, name, file); return; }
            juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::NoIcon).withTitle("Replace preset?")
                    .withMessage("A preset \"" + file.getFileNameWithoutExtension() + "\" exists. Replace it?")
                    .withButton("Replace", 1).withButton("Cancel", 0).withAssociatedComponent(safe.getComponent()),
                [safe, module, name, file](int answer) { if (safe != nullptr && answer == 1) safe->writePreset(module, name, file); });
        }), true);
    if (auto* field = w->getTextEditor("name")) field->grabKeyboardFocus();   // type the name at once
}

void LMWSynthEditor::writePreset(const juce::String& module, const juce::String& name, const juce::File& file) {
    const auto kind = lmw::presets::kindOf(module.toStdString());
    if (!kind) return;
    lmw::presets::Preset p{name.toStdString(), {}};
    for (auto key : lmw::presets::keys(*kind))
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(processor.state().getParameter(module + "." + juce::String(key.data(), key.size()))))
            p.values.emplace_back(std::string(key), processor.currentText(*r));
    if (!file.getParentDirectory().createDirectory() || !file.replaceWithText(juce::String::fromUTF8(lmw::presets::write(*kind, p).c_str())))
        showError("Preset not saved", "Could not write " + file.getFullPathName());
}

juce::File LMWSynthEditor::patchFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LMW-Synth Patches");
}

void LMWSynthEditor::showError(const juce::String& title, const juce::String& message) {
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::NoIcon)
                                     .withTitle(title).withMessage(message).withButton("OK", 1)
                                     .withAssociatedComponent(this), nullptr);
}

void LMWSynthEditor::savePatch() {
    patchFolder().createDirectory();
    const auto name = processor.patchName().isEmpty() ? juce::String("Untitled") : processor.patchName();
    // JUCE's own dialog (not the system one), so it is black on white like the rest (Spec S6.2). The name is
    // offered without ".lmwpatch" (user 2026-11-24: why show the extension); it is added when saving.
    chooser = std::make_unique<juce::FileChooser>("Save patch", patchFolder().getChildFile(name),
                                                  "*.lmwpatch", false, false, this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this)](const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            auto file = fc.getResult();
            if (file == juce::File()) return;
            bool added = false;                                // "Pad v1.2" -> "Pad v1.2.lmwpatch" (the dot kept)
            if (!file.hasFileExtension("lmwpatch")) { file = file.getSiblingFile(file.getFileName() + ".lmwpatch"); added = true; }
            const auto write = [safe, file] {
                if (safe == nullptr) return;
                const auto patchName = file.getFileNameWithoutExtension();
                safe->processor.setPatchName(patchName);
                if (!file.replaceWithText(safe->processor.patchText(patchName)))
                    safe->showError("Save patch", "Could not write " + file.getFullPathName());
                else
                    safe->processor.recordSound();             // Spec S7.7: saved, so not "changed since"
                safe->refreshPatchInfo();
            };
            if (!added || !file.existsAsFile()) { write(); return; }
            // The dialog checked the name as typed (without the extension): ask before replacing the file.
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::NoIcon, "Save patch",
                "\"" + file.getFileNameWithoutExtension() + "\" already exists. Replace it?", "Replace", "Cancel", safe.getComponent(),
                juce::ModalCallbackFunction::create([write](int ok) { if (ok == 1) write(); }));
        });
}

void LMWSynthEditor::loadPatch() {
    patchFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser>("Load patch", patchFolder(), "*.lmwpatch", false, false, this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this)](const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            juce::String error;
            {
                const LMWSynthProcessor::ScopedStep step(safe->processor, "load " + file.getFileName());   // S6.14
                if (!safe->processor.loadPatchText(file.loadFileAsString(), error)) {
                    safe->showError("Load patch", file.getFileName() + ": " + error);
                    return;
                }
                if (safe->processor.patchName().isEmpty()) safe->processor.setPatchName(file.getFileNameWithoutExtension());
            }
            safe->refreshPatchInfo();
            safe->repaint();
        });
}

void LMWLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                                          bool isHighlighted, bool isDown) {
    if (b.isEnabled()) {
        LookAndFeel_V4::drawButtonBackground(g, b, backgroundColour, isHighlighted, isDown);
        return;
    }
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(kBg);
    g.fillRoundedRectangle(r, 6.0f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle(r, 6.0f);
    const float dashes[] = {4.0f, 3.0f};
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
    g.setColour(kFg);
    g.fillPath(dashed);
}

void LMWLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool isHighlighted, bool isDown) {
    if (b.isEnabled()) {
        LookAndFeel_V4::drawButtonText(g, b, isHighlighted, isDown);
        return;
    }
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour(kFg);
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4, 2), juce::Justification::centred, 1);
}

void LMWLookAndFeel::drawFileBrowserRow(juce::Graphics& g, int width, int height, const juce::File&,
                                        const juce::String& filename, juce::Image*, const juce::String& fileSizeDescription,
                                        const juce::String& fileTimeDescription, bool isDirectory, bool isItemSelected,
                                        int, juce::DirectoryContentsDisplayComponent&) {
    const auto fg = isItemSelected ? kBg : kFg;
    g.fillAll(isItemSelected ? kFg : kBg);
    g.setColour(fg);
    // Glyph: a folder (box with a tab) or a page (box).
    const float h = static_cast<float>(height), s = h * 0.55f, gx = 4.0f, gy = (h - s) * 0.5f;
    if (isDirectory) {
        g.drawRect(juce::Rectangle<float>(gx, gy + s * 0.2f, s * 1.2f, s * 0.8f), 1.2f);
        g.fillRect(juce::Rectangle<float>(gx, gy, s * 0.5f, s * 0.25f));
    } else {
        g.drawRect(juce::Rectangle<float>(gx + s * 0.15f, gy, s * 0.8f, s), 1.2f);
    }
    const int x = static_cast<int>(gx + s * 1.5f);
    g.setFont(juce::FontOptions(h * 0.7f));
    if (width > 450 && !isDirectory) {
        const int sizeX = width * 7 / 10, dateX = width * 8 / 10;
        g.drawFittedText(filename, x, 0, sizeX - x, height, juce::Justification::centredLeft, 1);
        g.setFont(juce::FontOptions(h * 0.55f));
        g.drawFittedText(fileSizeDescription, sizeX, 0, dateX - sizeX - 8, height, juce::Justification::centredRight, 1);
        g.drawFittedText(fileTimeDescription, dateX, 0, width - 8 - dateX, height, juce::Justification::centredRight, 1);
    } else {
        g.drawFittedText(filename, x, 0, width - x, height, juce::Justification::centredLeft, 1);
    }
}

// ---- Spec S7.7: whole-instrument presets ----

void LMWSynthEditor::updatePatchLabel() {
    const auto name = processor.patchName();
    const auto text = name.isEmpty() ? juce::String() : "Patch: " + name + (processor.soundChanged() ? "*" : "");
    if (patchLabel.getText() != text) patchLabel.setText(text, juce::dontSendNotification);
    patchLabel.setTooltip(name.isEmpty() ? juce::String("Patch name (the file name when saved or loaded)")
                          : "Patch name: " + name + (processor.soundChanged()
                                ? "\n* = changed since it was chosen, loaded or saved" : juce::String()));
}

juce::File LMWSynthEditor::instrumentFolder() {
    const auto k = lmw::instruments::folderName;
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LMW-Synth Presets")
        .getChildFile(juce::String(k.data(), k.size()));
}

void LMWSynthEditor::showInstrumentMenu() {
    const auto& builtIn = lmw::instruments::builtIn();
    const auto folder = instrumentFolder();
    auto own = folder.findChildFiles(juce::File::findFiles, false, "*.lmwpatch");
    std::sort(own.begin(), own.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension()) < 0;
    });
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);              // black on white (Spec S6.2)
    menu.addSectionHeader("Instrument presets");
    menu.addItem(1, "Default");
    // Spec S7.16: one submenu per family; the world instruments also in theirs, below a separator (same ids: the
    // same preset). Then Presets > World (S7.13).
    for (const auto family : lmw::instruments::families) {
        juce::PopupMenu sub;
        sub.setLookAndFeel(&lookAndFeel);
        for (std::size_t i = 0; i < builtIn.size(); ++i)
            if (builtIn[i].family == family && builtIn[i].group != "World")
                sub.addItem(100 + static_cast<int>(i), juce::String::fromUTF8(builtIn[i].name.c_str()));
        bool separated = false;
        for (std::size_t i = 0; i < builtIn.size(); ++i)
            if (builtIn[i].family == family && builtIn[i].group == "World") {
                if (!separated) { sub.addSeparator(); separated = true; }
                sub.addItem(100 + static_cast<int>(i), juce::String::fromUTF8(builtIn[i].name.c_str()));
            }
        menu.addSubMenu(juce::String(family.data(), family.size()), sub);
    }
    juce::PopupMenu world;                           // Spec S7.13: Presets > World
    world.setLookAndFeel(&lookAndFeel);
    for (std::size_t i = 0; i < builtIn.size(); ++i)
        if (builtIn[i].group == "World") world.addItem(100 + static_cast<int>(i), juce::String::fromUTF8(builtIn[i].name.c_str()));
    menu.addSubMenu("World", world);
    {   // Spec S7.16 (user 2026-11-27): Presets > All, every built-in once, A-Z ignoring case (same ids)
        std::vector<std::size_t> order(builtIn.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return juce::String::fromUTF8(builtIn[a].name.c_str()).compareIgnoreCase(juce::String::fromUTF8(builtIn[b].name.c_str())) < 0;
        });
        juce::PopupMenu all;
        all.setLookAndFeel(&lookAndFeel);
        for (const auto i : order) all.addItem(100 + static_cast<int>(i), juce::String::fromUTF8(builtIn[i].name.c_str()));
        menu.addSubMenu("All", all);
    }
    if (!own.isEmpty()) {
        menu.addSectionHeader("Your presets");
        for (int i = 0; i < own.size(); ++i) menu.addItem(1000 + i, own[i].getFileNameWithoutExtension());
    }
    {   // Spec S7.9: tutorial lessons (their steps go into the notes)
        juce::PopupMenu lessons;
        lessons.setLookAndFeel(&lookAndFeel);
        for (const auto& l : lmw::tutorial::lessons())
            lessons.addItem(2000 + l.number, "Lesson " + juce::String(l.number) + " " + juce::String::fromUTF8("\xe2\x80\x94 ")
                                                 + juce::String::fromUTF8(lmw::tutorial::title(l.number).c_str()));
        menu.addSeparator();
        menu.addSubMenu("Tutorial (steps in the Notes tab)", lessons);
    }
    menu.addSeparator();
    menu.addItem(2, "Save as instrument preset...");
    if (folder.isDirectory()) menu.addItem(3, "Show presets folder");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&instrumentButton),
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), own, folder, focus = focusNow()](int result) {
            if (safe != nullptr) safe->restoreFocus(focus);
            if (safe == nullptr || result == 0) return;
            juce::String error;
            const auto& list = lmw::instruments::builtIn();
            bool ok = true;
            const LMWSynthProcessor::ScopedStep step(safe->processor, "instrument preset");   // Spec S6.14
            if (result == 1) {
                safe->processor.renameStep("preset Default");
                ok = safe->processor.applySound("Default", {}, {}, error);
            } else if (result == 2) {
                safe->saveInstrumentAs();
            } else if (result == 3) {
                folder.revealToUser();
            } else if (result >= 100 && result < 100 + static_cast<int>(list.size())) {
                const auto& ins = list[static_cast<std::size_t>(result - 100)];
                safe->processor.renameStep("preset " + ins.name);
                std::map<std::string, std::string> values;
                for (const auto& [id, value] : ins.values) values[id] = value;   // later entries win
                ok = safe->processor.applySound(juce::String::fromUTF8(ins.name.c_str()), values, ins.cables, error);
            } else if (result >= 1000 && result < 1000 + own.size()) {
                const auto file = own[result - 1000];
                ok = safe->processor.loadPatchText(file.loadFileAsString(), error, true);
                if (!ok) error = file.getFileName() + ": " + error;
            } else if (result > 2000 && result <= 2000 + static_cast<int>(lmw::tutorial::lessons().size())) {
                safe->processor.renameStep("Lesson " + std::to_string(result - 2000));
                ok = safe->applyLesson(result - 2000, error);
            }
            if (!ok) safe->showError("Cannot use this preset", error);
            safe->refreshPatchInfo();
            safe->repaint();
        });
}

void LMWSynthEditor::saveInstrumentAs() {
    auto* w = new juce::AlertWindow("Save instrument preset",
                                    "Name of the preset (the whole sound: all modules, voices, effects, cables):",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", processor.patchName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this), w](int result) {
            if (safe == nullptr || result != 1) return;
            const auto name = w->getTextEditorContents("name").trim();
            if (name.isEmpty()) return;
            const auto file = instrumentFolder().getChildFile(juce::String::fromUTF8(lmw::presets::fileName(name.toStdString()).c_str())
                                                              + ".lmwpatch");
            auto write = [safe, name, file] {
                if (safe == nullptr) return;
                safe->processor.setPatchName(name);
                if (!file.getParentDirectory().createDirectory() || !file.replaceWithText(safe->processor.patchText(name))) {
                    safe->showError("Preset not saved", "Could not write " + file.getFullPathName());
                    return;
                }
                safe->processor.recordSound();
                safe->refreshPatchInfo();
            };
            if (!file.existsAsFile()) { write(); return; }
            juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::NoIcon).withTitle("Replace preset?")
                    .withMessage("An instrument preset \"" + file.getFileNameWithoutExtension() + "\" exists. Replace it?")
                    .withButton("Replace", 1).withButton("Cancel", 0).withAssociatedComponent(safe.getComponent()),
                [write](int answer) { if (answer == 1) write(); });
        }), true);
    if (auto* field = w->getTextEditor("name")) { field->grabKeyboardFocus(); field->selectAll(); }
}

// Spec S7.9: a tutorial lesson: its starting sound (base + changes + cables; Lesson 11 keeps the sound) and,
// as the one exception to S7.7, its steps from docs/tutorial/Tutorial.md in the notes.
bool LMWSynthEditor::applyLesson(int number, juce::String& error) {
    const auto& lessons = lmw::tutorial::lessons();
    const auto it = std::find_if(lessons.begin(), lessons.end(), [number](const auto& l) { return l.number == number; });
    if (it == lessons.end()) return false;
    const auto name = "Lesson " + juce::String(number) + " " + juce::String::fromUTF8("\xe2\x80\x94 ")
                    + juce::String::fromUTF8(lmw::tutorial::title(number).c_str());
    if (!it->base.empty()) {
        std::map<std::string, std::string> values;
        std::string cables = it->cables;
        if (it->base != "Default") {
            for (const auto& ins : lmw::instruments::builtIn())
                if (ins.name == it->base) {
                    for (const auto& [id, value] : ins.values) values[id] = value;
                    if (cables.empty()) cables = ins.cables;
                }
        }
        for (const auto& [id, value] : it->values) values[id] = value;   // the lesson's changes win
        if (!processor.applySound(name, values, cables, error)) return false;
    }
    processor.setNotes(juce::String::fromUTF8(lmw::tutorial::notes(number).c_str()));
    return true;
}

// ---- Spec S7.12 (Phase 31): 9P /preset, /view, /snapshot ----

namespace {
std::string trimmedText(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) s.remove_suffix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) s.remove_prefix(1);
    return std::string(s);
}
bool sameText(std::string_view a, std::string_view b) {
    return juce::String::fromUTF8(a.data(), static_cast<int>(a.size())).equalsIgnoreCase(juce::String::fromUTF8(b.data(), static_cast<int>(b.size())));
}
constexpr std::array<const char*, 6> tabNames{"Synth", "Effects", "Matrix", "Notes", "Schematic", "Assistant"};   // showTab order
} // namespace

std::string LMWSynthEditor::runPreset(const std::string& text) {
    const auto t = trimmedText(text);
    if (t.empty()) return "no preset name";
    juce::String error;
    bool ok = true;
    const auto space = t.find(' ');
    const auto first = t.substr(0, space), rest = space == std::string::npos ? std::string() : trimmedText(t.substr(space + 1));
    if ((sameText(first, "tutorial") || sameText(first, "lesson")) && !rest.empty()) {          // Spec S7.9
        const int n = std::atoi(rest.c_str());
        const LMWSynthProcessor::ScopedStep step(processor, "Lesson " + juce::String(n));
        ok = applyLesson(n, error);
        if (!ok && error.isEmpty()) error = "no tutorial lesson " + juce::String(rest);
    } else if (const auto kind = lmw::presets::kindOf(juce::String(first).toLowerCase().toStdString()); kind && !rest.empty()) {
        const auto module = juce::String(first).toLowerCase();                                  // Spec S7.5
        if (sameText(rest, "Default")) { applyDefaults(module); refreshPatchInfo(); repaint(); return {}; }
        for (const auto& p : lmw::presets::builtIn(*kind))
            if (sameText(p.name, rest)) { applyPreset(module, p); refreshPatchInfo(); repaint(); return {}; }
        for (const auto& f : presetFolder(*kind).findChildFiles(juce::File::findFiles, false, "*.lmwpreset"))
            if (sameText(f.getFileNameWithoutExtension().toStdString(), rest)) {
                std::string why;
                auto preset = lmw::presets::parse(*kind, f.loadFileAsString().toStdString(), &why);
                if (!preset) return f.getFileName().toStdString() + ": " + why;
                applyPreset(module, *preset);
                refreshPatchInfo(); repaint();
                return {};
            }
        return "no " + module.toStdString() + " preset named " + rest;
    } else {                                                                                       // Spec S7.7
        const LMWSynthProcessor::ScopedStep step(processor, "instrument preset");
        bool found = false;
        if (sameText(t, "Default")) { processor.renameStep("preset Default"); ok = processor.applySound("Default", {}, {}, error); found = true; }
        for (const auto& ins : lmw::instruments::builtIn())
            if (!found && sameText(ins.name, t)) {
                processor.renameStep("preset " + ins.name);
                std::map<std::string, std::string> values;
                for (const auto& [id, value] : ins.values) values[id] = value;
                ok = processor.applySound(juce::String::fromUTF8(ins.name.c_str()), values, ins.cables, error);
                found = true;
            }
        for (const auto& f : instrumentFolder().findChildFiles(juce::File::findFiles, false, "*.lmwpatch"))
            if (!found && sameText(f.getFileNameWithoutExtension().toStdString(), t)) {
                ok = processor.loadPatchText(f.loadFileAsString(), error, true);
                found = true;
            }
        if (!found) return "no instrument preset named " + t;
    }
    refreshPatchInfo();
    repaint();
    return ok ? std::string() : error.toStdString();
}

std::string LMWSynthEditor::viewText() const {
    const auto rect = [](juce::Rectangle<int> r) {
        return std::to_string(r.getX()) + " " + std::to_string(r.getY()) + " " + std::to_string(r.getWidth()) + " " + std::to_string(r.getHeight());
    };
    std::string s = "tab " + std::string(tabNames[static_cast<std::size_t>(std::clamp(currentTab, 0, 5))]) + "\n";
    s += "size " + std::to_string(getWidth()) + " " + std::to_string(getHeight()) + "\n";
    s += std::string("compact ") + (compactButton.getToggleState() ? "on" : "off") + "\n";
    s += std::string("keyboard ") + (keyboardShown ? "on" : "off") + "\n";
    s += std::string("scales ") + (settings.showScales ? "on" : "off") + "\n";     // Spec S6.20
    s += std::string("rings ") + (settings.socketRings ? "on" : "off") + "\n";
    s += std::string("gradient ") + (settings.panelGradient ? "on" : "off") + "\n";   // Spec S6.30
    s += "scroll " + std::to_string(rackView.getViewPositionX()) + " " + std::to_string(rackView.getViewPositionY()) + "\n";   // S6.21
    if (currentTab == 0 || currentTab == 1)
        for (const auto& g : currentTab == 0 ? groups : effectGroups)
            s += "panel " + rect(getLocalArea(rackArea.get(), g.bounds)) + " " + g.title.toStdString() + "\n";   // editor pixels (S6.21)
    if (currentTab == 4 && schematic != nullptr) {
        s += "area " + rect(schematicView.getBounds()) + " schematic\n";
        for (const auto& [name, r] : schematic->blockAreas()) s += "panel " + rect(getLocalArea(schematic.get(), r)) + " " + name + "\n";
    }
    if (currentTab == 2 && matrix != nullptr) s += "area " + rect(matrix->getBounds()) + " matrix\n";
    if (currentTab == 1 && fxMatrix != nullptr)            // Spec S6.25: the effects matrix, editor pixels
        s += "area " + rect(getLocalArea(rackArea.get(), fxMatrix->getBounds())) + " fxmatrix\n";
    if (keyboardShown) s += "area " + rect(keyboard.getBounds()) + " keyboard\n";
    return s;
}

std::string LMWSynthEditor::applyView(const std::string& text) {
    std::string_view rest(text);
    while (!rest.empty()) {
        const auto nl = rest.find('\n');
        const auto line = trimmedText(rest.substr(0, nl));
        rest = nl == std::string_view::npos ? std::string_view{} : rest.substr(nl + 1);
        if (line.empty()) continue;
        const auto space = line.find(' ');
        const auto key = line.substr(0, space), value = space == std::string::npos ? std::string() : trimmedText(line.substr(space + 1));
        if (key == "tab") {
            const auto it = std::find_if(tabNames.begin(), tabNames.end(), [&](const char* n) { return sameText(n, value); });
            if (it == tabNames.end()) return "unknown tab: " + value + " (Synth, Effects, Matrix, Notes, Schematic, Assistant)";
            if (it - tabNames.begin() == 5 && assistant == nullptr) return "no Assistant tab here";
            const int tab = static_cast<int>(it - tabNames.begin());
            juce::Button* buttons[] = {&panelTab, &effectsTab, &matrixTab, &notesTab, &schematicTab, &assistantTab};
            buttons[tab]->setToggleState(true, juce::dontSendNotification);   // the radio group clears the others
            showTab(tab);
        } else if (key == "size") {
            int w = 0, h = 0;
            if (std::sscanf(value.c_str(), "%d %d", &w, &h) != 2 || w < 100 || h < 100) return "bad size: " + value + " (width height)";
            setSize(w, h);
        } else if (key == "compact" || key == "keyboard") {
            if (value != "on" && value != "off") return "bad " + key + ": " + value + " (on or off)";
            const bool on = value == "on";
            if (key == "keyboard") setKeyboardShown(on);
            else if (compactButton.getToggleState() != on) {
                compactButton.setToggleState(on, juce::dontSendNotification);
                if (compactButton.onClick) compactButton.onClick();
            }
        } else if (key == "scales" || key == "rings") {             // Spec S6.20 (also saved to config.ini)
            if (value != "on" && value != "off") return "bad " + key + ": " + value + " (on or off)";
            (key == "scales" ? settings.showScales : settings.socketRings) = value == "on";
            saveConfig();
            applyAids();
        } else if (key == "gradient") {                           // Spec S6.30 (also saved to config.ini)
            if (value != "on" && value != "off") return "bad gradient: " + value + " (on or off)";
            settings.panelGradient = value == "on";
            saveConfig();
            repaint();
            if (rackArea) rackArea->repaint();
        } else if (key == "scroll") {                             // Spec S6.21: the rack view position
            int x = 0, y = 0;
            if (std::sscanf(value.c_str(), "%d %d", &x, &y) != 2) return "bad scroll: " + value + " (x y)";
            rackView.setViewPosition(x, y);
        } else if (key == "pointer" && value == "away") {
            // Pictures: no hover band, hover box or tooltip — the pointer just above the editor (a title bar).
            juce::Desktop::getInstance().setMousePosition(getScreenPosition() - juce::Point<int>(0, 8));
        } else if (key == "focus" && value == "none") {
            unfocusAllComponents();                       // no keyboard cursor (Matrix, Schematic) in a picture
        } else {
            return "bad view line: " + line + " (tab, size, compact, keyboard, scales, rings, scroll, pointer away, focus none)";
        }
    }
    return {};
}

std::string LMWSynthEditor::snapshotPng() {
    const auto image = createComponentSnapshot(getLocalBounds(), true, 1.0f);
    juce::MemoryOutputStream out;
    juce::PNGImageFormat png;
    if (!png.writeImageToStream(image, out)) return {};
    return std::string(static_cast<const char*>(out.getData()), out.getDataSize());
}

// ---- Spec S6.26: units without effect ----

namespace {
std::string gainName(lmw::dsp::In i) {                     // the fader that scales an input (PluginProcessor::useParams)
    using lmw::dsp::In;
    switch (i) {
        case In::MixIn1: case In::MixIn2: case In::MixIn3: case In::MixIn4: return "MIXER level";
        case In::Mix2In1: case In::Mix2In2: case In::Mix2In3: return "MIX 2 level";
        case In::Vco1Fm: case In::Vco2Fm: case In::Vco3Fm: return "FM";
        case In::Vco1Pm: case In::Vco2Pm: case In::Vco3Pm: return "PM";
        case In::Vcf1Cutoff: case In::Vcf2Cutoff: return "Env";
        case In::FormantVowel: case In::FormantRes: return "FORMANT Mix";
        case In::VcaMod: return "Tremolo";
        case In::VcaVel: return "Velocity";
        default: return "level";
    }
}
}

std::string LMWSynthEditor::panelUnit(const std::vector<Jack*>& jacks) {
    for (const auto* j : jacks)
        if (j->output) return std::string(lmw::dsp::portUnit(static_cast<lmw::dsp::Out>(j->port)));
    for (const auto* j : jacks) return std::string(lmw::dsp::portUnit(static_cast<lmw::dsp::In>(j->port)));
    return {};
}

juce::String LMWSynthEditor::useNote(std::string_view unit) const {
    switch (shownUse.unit(unit)) {
        case lmw::dsp::Use::Unused: {
            const auto why = lmw::dsp::unusedReason(processor.currentPatch(), shownUseParams, shownUse, unit, gainName);
            if (why.starts_with("silent: "))                  // S6.27: "Silent: no sound reaches it (...)."
                return juce::String::fromUTF8(("Silent: " + why.substr(8) + ".").c_str());
            return juce::String::fromUTF8(("No effect on the sound: " + why + ".").c_str());
        }
        case lmw::dsp::Use::Passing: return "Passes the sound unchanged: Mix 0.";
        case lmw::dsp::Use::Used: break;
    }
    return {};
}

void LMWSynthEditor::updateUse(bool force) {
    auto up = processor.useParams();
    const auto r = lmw::dsp::unitUse(processor.currentPatch(), up);
    bool same = r == shownUse && processor.patchVersion() == shownPatchVersion;
    for (std::size_t i = 0; same && i < up.gain.size(); ++i) same = (up.gain[i] == 0.0f) == (shownUseParams.gain[i] == 0.0f);
    if (same && !force) return;
    const bool repaintNeeded = force || !(r == shownUse);
    shownUse = r;
    shownUseParams = up;
    for (std::size_t i = 0; i < headers.size(); ++i) headers[i]->setNote(useNote(panelUnit(groupJacks[i])));
    for (std::size_t e = 0; e < effectHeaders.size(); ++e) effectHeaders[e]->setNote(useNote(panelUnit(effectGroupJacks[e + 1])));
    if (schematic) schematic->setUse(shownUse, [this](std::string_view u) { return useNote(u); });
    if (repaintNeeded) rackArea->repaint();
}

// A panel's frame: solid when its unit has an effect, short dashes without effect, long dashes while it passes its
// input unchanged (S6.26). 2 px when dashed, so the gaps show.
void LMWSynthEditor::drawPanelFrame(juce::Graphics& g, juce::Rectangle<int> b, const std::vector<Jack*>& jacks, int thickness) const {
    const auto use = shownUse.unit(panelUnit(jacks));
    g.setColour(kFg);
    if (use == lmw::dsp::Use::Used) { g.drawRect(b, thickness); return; }
    const float shortDash[] = {6.0f, 5.0f}, longDash[] = {16.0f, 6.0f};
    const float* dash = use == lmw::dsp::Use::Unused ? shortDash : longDash;
    const float t = static_cast<float>(std::max(2, thickness)), h = t / 2;
    const auto r = b.toFloat();
    g.drawDashedLine({r.getX(), r.getY() + h, r.getRight(), r.getY() + h}, dash, 2, t);
    g.drawDashedLine({r.getRight() - h, r.getY(), r.getRight() - h, r.getBottom()}, dash, 2, t);
    g.drawDashedLine({r.getRight(), r.getBottom() - h, r.getX(), r.getBottom() - h}, dash, 2, t);
    g.drawDashedLine({r.getX() + h, r.getBottom(), r.getX() + h, r.getY()}, dash, 2, t);
}

// ---- Spec S6.28: the load meter in the header ----

void LMWSynthEditor::updateLoadLabel() {
    auto l = processor.loadInfo();
    l.memoryMB = lmw::app::residentMemoryMB();
    const bool warn = processor.loadWarning();
    loadLabel.setText("CPU " + juce::String(juce::roundToInt(l.cpu * 100.0)) + " %", juce::dontSendNotification);
    // The theme's text on its page; a warning white on dark red (#B00020, 7.4:1 with white, either theme); at the Max
    // preset the page on the text (21:1).
    const bool maxContrast = palette.minRatio >= 21.0;
    const juce::Colour red = maxContrast ? juce::Colour(kFg) : juce::Colour(0xffb00020);
    const juce::Colour onRed = maxContrast ? juce::Colour(kBg) : juce::Colour(0xffffffff);   // fixed on purpose (scan: allowed)
    loadLabel.setColour(juce::Label::backgroundColourId, warn ? red : juce::Colour(kBg));
    loadLabel.setColour(juce::Label::textColourId, warn ? onRed : juce::Colour(kFg));
    loadLabel.setColour(juce::Label::outlineColourId, warn ? red : juce::Colour(kFg));
    juce::String tip = "Audio load: " + juce::String(l.cpu * 100.0, 1) + " % of the time each audio block may take "
                     + "(peak " + juce::String(l.peak * 100.0, 1) + " % in the last 5 s).\n"
                     + "Voices playing: " + juce::String(l.voices) + ".\n"
                     + juce::String::fromUTF8("Overruns (a block took longer than it may \xe2\x80\x94 a dropout is likely): ")
                     + juce::String(static_cast<int>(l.overruns));
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        tip += "; audio device dropouts: " + juce::String(static_cast<int>(l.xruns));
    tip += ".\n";
    if (l.memoryMB > 0.0) tip += "Memory: " + juce::String(l.memoryMB, 1) + " MB.\n";
    tip += warn ? juce::String::fromUTF8("Too heavy? Lower KEYS \xe2\x80\xba Voices or switch off effects (red: above 70 % or a dropout in the last 5 s).")
                : juce::String("Red above 70 % or after a dropout.");
    if (!loadLabel.isMouseOver()) loadLabel.setTooltip(tip);   // frozen while hovered: a changing text restarts the tooltip
}
