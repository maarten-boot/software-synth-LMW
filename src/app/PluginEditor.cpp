// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "ui/Theme.h"

namespace {
juce::Colour toJuce(lmw::ui::Rgb c) { return juce::Colour(c.r, c.g, c.b); }
const juce::Colour kBg      = toJuce(lmw::ui::theme::background);
const juce::Colour kFg      = toJuce(lmw::ui::theme::foreground);
}

LMWLookAndFeel::LMWLookAndFeel() {
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
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, kBg);
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, kFg);
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, kFg);
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
    setColour(juce::MidiKeyboardComponent::textLabelColourId, kFg);
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
    g.fillRect(juce::Rectangle<float>(cx - 3.0f, sliderPos, 6.0f, bottom - sliderPos));

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
const juce::Identifier kLayoutProp{"uiLayout"};     // rack layout (Spec S6.5), RackLayout::serialize()
constexpr int kMaxRows = 4;

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
      palette(lmw::ui::theme::palette(settings.minRatio())),
      keyboard(p.keyboardState(), palette) {
    setLookAndFeel(&lookAndFeel);
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
    // VCO 1 | VCO 2 | VCO 3 | NOISE | LFO 1;  MIXER | VCF 1 | VCF 2;  KEYS | ENV 1 | ENV 2 | LFO 2 | VCA | OUT.
    // Titles of the envelopes name their normalled destination (S5.2).
    std::array<std::size_t, 3> vcoPanel{};
    for (int n = 0; n < 3; ++n) {
        const auto i = static_cast<std::size_t>(n);
        vcoPanel[i] = groups.size();
        groups.push_back({"VCO " + juce::String(n + 1), 0,
            {addFader(P::vcoFm[i], "FM", fmTip(n)),                // above the FM input socket
             addFader(P::vcoTune[i], "Tune", inputTip(lmw::dsp::vcoPitchIn(n),
                      "Detune, -7 to +7 semitones; Oct shifts by whole octaves")),
             stacked(addChoice(P::vcoWave[i], "Wave", {"Sine", "Saw", "Square", "Triangle"}),
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
    const std::size_t lfoPanel = groups.size();
    groups.push_back({"LFO 1", 0, {addChoice(P::lfoWave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),
                                   addFader(P::lfoRate, "Rate", [] {
                                       return juce::String("Shared LFO 1 (vibrato): lfo1.out feeds vco1.fm, vco2.fm and vco3.fm\n"
                                                           "(normalled); the FM faders set the depth");
                                   })}, {}});
    const std::size_t mixPanel = groups.size();
    groups.push_back({"MIXER", 1,
        {addFader(P::mixVco[0], "VCO 1", inputTip(In::MixIn1, "Level of mixer input 1")),
         addFader(P::mixVco[1], "VCO 2", inputTip(In::MixIn2, "Level of mixer input 2")),
         addFader(P::mixVco[2], "VCO 3", inputTip(In::MixIn3, "Level of mixer input 3")),
         addFader(P::mixNoise, "Noise", inputTip(In::MixIn4, "Level of mixer input 4"))}, {}});
    std::array<std::size_t, 2> vcfPanel{};               // Spec S5.9, S5.11: VCF 1 -> VCF 2 in series
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
             stacked(addChoice(P::vcfSlope[i], "Slope", {"24 dB", "12 dB", "6 dB"},
                               "Steepness. Bandpass and notch: 12 or 24 dB (6 dB acts as 12 dB)."),
                     addChoice(P::vcfType[i], "Type", {"Low", "High", "Band", "Notch"},
                               "Lowpass, highpass, bandpass or notch"))}, {}});
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
    groups.push_back({"LFO 2", 2, {addChoice(P::lfo2Wave, "Wave", {"Sine", "Triangle", "Saw", "Square"}),
                                   addFader(P::lfo2Rate, "Rate", [] {
                                       return juce::String("Shared LFO 2 (tremolo): lfo2.out feeds vca.mod (normalled);\n"
                                                           "the VCA Tremolo fader sets the depth");
                                   })}, {}});
    const std::size_t vcaPanel = groups.size();
    groups.push_back({"VCA", 2, {addFader(P::vcaInitial, "Initial", [this] {
                         return describeInput(In::VcaIn) + "\n" + describeInput(In::VcaCv)
                              + "\nGain = Initial + CV (0..1). Above 0, notes keep sounding after release.";
                     }),
                     addFader(P::vcaTremolo, "Tremolo", inputTip(In::VcaMod,
                         "Tremolo depth: vca.mod swings the gain down by up to this much (0 = off)"))}, {}});
    const std::size_t outPanel = groups.size();
    groups.push_back({"OUT", 2, {addFader(P::gain, "Volume", inputTip(In::OutIn, "All voices summed, then soft limiter"))}, {}});
    // Spec S7.4: Effects tab. The reverb follows everything on the Synth tab (after OUT's limiter).
    effectGroups.push_back({"REVERB", 0,
        {addFader(P::reverbMix, "Mix", [] { return juce::String("Dry/wet balance: 0 = off (no reverb at all), 100 % = only reverb"); }),
         addFader(P::reverbSize, "Size", [] { return juce::String("Room size: longer decay when larger"); }),
         addFader(P::reverbDamping, "Damping", [] { return juce::String("High-frequency damping of the reflections: darker when higher"); }),
         addFader(P::reverbWidth, "Width", [] { return juce::String("Stereo width of the reverb: 0 = mono, 100 % = widest"); })}, {}});
    for (const auto& grp : effectGroups)
        for (const auto& item : grp.items)
            if (item.fader != nullptr) { effectComponents.push_back(&item.fader->slider); effectComponents.push_back(&item.fader->label); }
    jassert(usedFaders == faders.size() && usedChoices == choices.size());

    // Spec S6.5: rack. A row holds at most as many columns as the widest default row, so the window
    // width never changes; panels can be moved into up to 4 rows. The layout is remembered.
    {
        std::vector<int> columns;
        int rowCount = 0;
        for (const auto& g : groups) rowCount = std::max(rowCount, g.row + 1);
        lmw::ui::RackLayout::Rows defaultRows(static_cast<std::size_t>(rowCount));
        for (std::size_t i = 0; i < groups.size(); ++i) {
            columns.push_back(groups[i].columns());
            defaultRows[static_cast<std::size_t>(groups[i].row)].push_back(static_cast<int>(i));
            headers.push_back(std::make_unique<PanelHeader>(*this, static_cast<int>(i)));
            addAndMakeVisible(*headers.back());
        }
        int maxColumns = 0;
        for (const auto& row : defaultRows) {
            int c = 0;
            for (int g : row) c += columns[static_cast<std::size_t>(g)];
            maxColumns = std::max(maxColumns, c);
        }
        rack.emplace(columns, defaultRows, maxColumns, kMaxRows);
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
            {keysPanel, 1, true, out(Out::KeysVel), "Vel"},
            {mixPanel, 0, false, in(In::MixIn1), "In 1"},       {mixPanel, 1, false, in(In::MixIn2), "In 2"},
            {mixPanel, 2, false, in(In::MixIn3), "In 3"},       {mixPanel, 3, false, in(In::MixIn4), "In 4"},
            {mixPanel, 3, true, out(Out::Mix), "Out"},
            {noisePanel, 0, true, out(Out::Noise), "Out"},     {noisePanel, 2, true, out(Out::Rand), "Rand"},
            {vcfPanel[0], 0, false, in(In::Vcf1In), "In"},      {vcfPanel[0], 1, false, in(In::Vcf1Res), "Res"},
            {vcfPanel[0], 2, false, in(In::Vcf1Cutoff), "Cut"}, {vcfPanel[0], 4, true, out(Out::Vcf1), "Out"},
            {vcfPanel[1], 0, false, in(In::Vcf2In), "In"},      {vcfPanel[1], 1, false, in(In::Vcf2Res), "Res"},
            {vcfPanel[1], 2, false, in(In::Vcf2Cutoff), "Cut"}, {vcfPanel[1], 4, true, out(Out::Vcf2), "Out"},
            {vcaPanel, 0, false, in(In::VcaIn), "In"},          {vcaPanel, 0, false, in(In::VcaCv), "CV"},
            {vcaPanel, 1, false, in(In::VcaMod), "Mod"},        {vcaPanel, 1, true, out(Out::Vca), "Out"},
            {lfoPanel, 1, true, out(Out::Lfo1), "Out"},         {lfo2Panel, 1, true, out(Out::Lfo2), "Out"},
            {outPanel, 0, false, in(In::OutIn), "In"},
            {env1Panel, 0, false, in(In::Env1Gate), "Gate"},    {env1Panel, 3, true, out(Out::Env1), "Out"},
            {env2Panel, 0, false, in(In::Env2Gate), "Gate"},    {env2Panel, 3, true, out(Out::Env2), "Out"},
        };
        for (int n = 0; n < 3; ++n) {                         // FM under FM, Pitch under Tune, Out last
            const auto g = vcoPanel[static_cast<std::size_t>(n)];
            ports.push_back({g, 0, false, in(lmw::dsp::vcoFmIn(n)), "FM"});
            ports.push_back({g, 1, false, in(lmw::dsp::vcoPitchIn(n)), "Pitch"});
            ports.push_back({g, 2, true, out(lmw::dsp::vcoOut(n)), "Out"});
        }
        groupJacks.resize(groups.size());
        for (const auto& d : ports) {
            jacks.push_back(std::make_unique<Jack>(*this, d.output, d.port, d.name));
            jacks.back()->column = d.column;
            groupJacks[d.group].push_back(jacks.back().get());
            addAndMakeVisible(*jacks.back());
        }
    }

    // Spec S7, S6.7, S6.8: Synth / Effects / Matrix / Schematic / Notes tabs, patch name, Load / Save.
    ++openEditors;
    juce::LookAndFeel::setDefaultLookAndFeel(&defaultLook.get());
    for (auto* tab : {&panelTab, &effectsTab, &matrixTab, &schematicTab, &notesTab}) {
        tab->setRadioGroupId(1);
        tab->setClickingTogglesState(true);
        addAndMakeVisible(*tab);
    }
    panelTab.setToggleState(true, juce::dontSendNotification);
    panelTab.setTooltip("The synthesizer: module panels and cables");
    effectsTab.setTooltip("Effects after the synthesizer: reverb");
    notesTab.setTooltip("Free-text notes, saved with the patch");
    panelTab.onClick = [this] { if (panelTab.getToggleState()) showTab(0); };
    matrixTab.setTooltip("The patch as a pin matrix: every cable with its level");
    effectsTab.onClick = [this] { if (effectsTab.getToggleState()) showTab(1); };
    matrixTab.onClick = [this] { if (matrixTab.getToggleState()) showTab(2); };
    notesTab.onClick = [this] { if (notesTab.getToggleState()) showTab(3); };
    schematicTab.setTooltip("The instrument as a schematic, with every connection");
    schematicTab.onClick = [this] { if (schematicTab.getToggleState()) showTab(4); };
    patchLabel.setTooltip("Patch name (the file name when saved or loaded)");
    // Spec S10.5: where the 9P server of this instance listens.
    titleLabel.setTooltip(processor.ninepEndpoints().empty()
        ? juce::String("9P server: not available")
        : "9P server (connect with a 9P client, e.g. 9p -a " + juce::String(processor.ninepEndpoints()).upToFirstOccurrenceOf(" ", false, false)
              + " ls):\n" + juce::String(processor.ninepEndpoints()).replace(" ", "\n"));
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(patchLabel);
    loadButton.setTooltip("Load a patch file (parameters, cables, notes)");
    saveButton.setTooltip("Save parameters, cables and notes as a patch file");
    loadButton.onClick = [this] { loadPatch(); };
    saveButton.onClick = [this] { savePatch(); };
    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    notesEditor.setMultiLine(true, true);
    notesEditor.setReturnKeyStartsNewLine(true);
    notesEditor.setTabKeyUsedAsCharacter(false);
    notesEditor.setScrollbarsShown(true);
    notesEditor.setTitle("Patch notes");
    notesEditor.setTextToShowWhenEmpty("Notes for this patch: they are saved with it.", kFg);
    notesEditor.onTextChange = [this] { processor.setNotes(notesEditor.getText()); };
    addChildComponent(notesEditor);
    // Spec S6.8: the Matrix tab edits the same patch as the cables (through connect(), which reports
    // an input that is full).
    matrix = std::make_unique<MatrixView>([this]() -> const lmw::dsp::Patch& { return processor.currentPatch(); },
                                          [this](int in, int out, float level) { return connect(in, out, level); });
    matrix->setPalette(palette);                            // unit colours in the headers (S6.9)
    addChildComponent(*matrix);
    schematic = std::make_unique<SchematicView>([this]() -> const lmw::dsp::Patch& { return processor.currentPatch(); });
    schematic->setPalette(palette);
    addChildComponent(*schematic);
    for (auto* c : effectComponents) c->setVisible(false);  // Effects tab not shown at start

    resetLayoutButton.setTooltip("Put all module panels back in their original places");
    resetLayoutButton.onClick = [this] {
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

    // Spec S6.3: contrast preset; Custom asks for a minimum ratio (4.5 .. 21).
    contrastBox.addItemList({"AA 4.5:1", "AAA 7:1", "Max 21:1", "Custom"}, 1);
    contrastBox.setTooltip("Minimum contrast of every colour pair (WCAG). Black and white are 21:1 at\n"
                           "every setting; stricter settings darken or invert the pressed keys.\n"
                           "Also in the config file: " + configFile().getFullPathName());
    contrastBox.setTitle("Contrast");
    updateContrastBox();
    contrastBox.onChange = [this] {
        const int id = contrastBox.getSelectedId();
        if (id == 4) { askCustomRatio(); return; }
        if (id >= 1 && id <= 3) {
            settings.contrast = static_cast<lmw::ui::ContrastPreset>(id - 1);
            applyContrast();
        }
    };
    addAndMakeVisible(contrastBox);

    keyboard.setWantsKeyboardFocus(true);       // QWERTY keys play notes (Spec S3.2)
    addAndMakeVisible(keyboard);

    // Child widgets (e.g. slider value boxes) take their colours when created, before they had this
    // look-and-feel; re-send it so every child uses the black-on-white theme (Spec S6.2).
    sendLookAndFeelChange();

    // A saved state's view wins; a new window starts with the config file's default.
    const bool compact = processor.state().state.getProperty(kCompactProp, settings.compact.value_or(false));
    compactButton.setToggleState(compact, juce::dontSendNotification);
    setCompact(compact);                         // sets the window size

    cables = std::make_unique<CableLayer>(*this);   // last child: drawn above the panels
    addAndMakeVisible(*cables);
    cables->setBounds(getLocalBounds());
    shownPatchVersion = p.patchVersion();

    refreshPatchInfo();                          // patch name and notes from the saved state
    p.resetKeyboardDisplay();
    startTimerHz(30);                            // show external MIDI notes on the keyboard
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::Component>(&keyboard)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

LMWSynthEditor::~LMWSynthEditor() {
    stopTimer();
    chooser.reset();
    setLookAndFeel(nullptr);
    if (--openEditors == 0 && &juce::LookAndFeel::getDefaultLookAndFeel() == &defaultLook.get())
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);   // the last LMW editor is closing
}

void LMWSynthEditor::timerCallback() {
    processor.updateKeyboardFromMidiInput();
    if (processor.patchFileVersion() != shownPatchFile || (!notesEditor.hasKeyboardFocus(true)
            && notesEditor.getText() != processor.notes()))   // patch loaded, or state restored by the host
        refreshPatchInfo();
    if (processor.patchVersion() != shownPatchVersion) {    // cables changed (e.g. state restored)
        shownPatchVersion = processor.patchVersion();
        repaint();
    }
}

bool LMWSynthEditor::isTypingText() {
    return dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr;
}

bool LMWSynthEditor::keyPressed(const juce::KeyPress& key) {
    return !isTypingText() && keyboard.keyPressed(key);
}

bool LMWSynthEditor::keyStateChanged(bool isKeyDown) {
    return !isTypingText() && keyboard.keyStateChanged(isKeyDown);
}

juce::String LMWSynthEditor::describeInput(lmw::dsp::In in) const {
    const auto& patch = processor.currentPatch();
    const auto idx = [](auto e) { return static_cast<std::size_t>(e); };
    const juce::String inName(lmw::dsp::inputNames[idx(in)].data(), lmw::dsp::inputNames[idx(in)].size());
    const juce::String arrow = juce::String::fromUTF8("\xe2\x86\x92");
    // Signal flow reads left to right: source → input (Spec S6.1). Several cables: "a + b (50 %) → in".
    if (!patch.isPatched(in)) {
        const auto src = patch.source(in);
        if (src == lmw::dsp::noSource) return inName + ": not connected (reads 0); patch a cable into it";
        const juce::String srcName(lmw::dsp::outputNames[idx(src)].data(), lmw::dsp::outputNames[idx(src)].size());
        return srcName + " " + arrow + " " + inName + " (normalled)";
    }
    juce::StringArray sources;
    for (int k = 0; k < patch.pinCount(in); ++k) {
        const auto& pin = patch.pin(in, k);
        const auto n = lmw::dsp::outputNames[static_cast<std::size_t>(pin.source)];
        sources.add(juce::String(n.data(), n.size())
                    + (pin.level < 1.0f ? " (" + juce::String(juce::roundToInt(pin.level * 100.0f)) + " %)" : juce::String()));
    }
    return sources.joinIntoString(" + ") + " " + arrow + " " + inName + (patch.pinCount(in) > 1 ? " (cables, summed)" : " (cable)");
}

LMWSynthEditor::Item LMWSynthEditor::addFader(const char* paramId, const juce::String& text,
                                              std::function<juce::String()> tip) {
    Fader& k = faders[usedFaders++];
    k.slider.setSliderSnapsToMousePosition(false);   // drag moves relative; a click does not jump the value
    k.slider.tip = std::move(tip);
    k.attachment = std::make_unique<SliderAttachment>(processor.state(), paramId, k.slider);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(k.slider);
    addAndMakeVisible(k.label);
    return {&k, nullptr};
}

LMWSynthEditor::Item LMWSynthEditor::addChoice(const char* paramId, const juce::String& text,
                                               const juce::StringArray& options, const juce::String& tip) {
    Choice& c = choices[usedChoices++];
    c.box.addItemList(options, 1);
    c.box.setTooltip(tip);
    c.attachment = std::make_unique<ComboAttachment>(processor.state(), paramId, c.box);
    c.label.setText(text, juce::dontSendNotification);
    c.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c.box);
    addAndMakeVisible(c.label);
    return {nullptr, &c, nullptr};
}

void LMWSynthEditor::setCompact(bool compact) {
    m = compact ? compactMetrics : normalMetrics;
    for (std::size_t i = 0; i < usedFaders; ++i) {
        faders[i].slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, m.boxW, m.boxH);
        faders[i].label.setFont(juce::FontOptions(m.labelFont));
    }
    for (std::size_t i = 0; i < usedChoices; ++i) choices[i].label.setFont(juce::FontOptions(m.labelFont));
    notesEditor.applyFontToAllText(juce::FontOptions(m.labelFont + 3.0f));   // Spec S7: notes, readable size
    notesEditor.setFont(juce::FontOptions(m.labelFont + 3.0f));
    updateSize();
}

void LMWSynthEditor::updateSize() {
    // Width of the widest row, at least that of the default layout, so moving panels never makes
    // the window narrower (Spec S6.5).
    int width = 0;
    for (const auto* rows : {&rack->rows(), &rack->defaultRows()})
        for (const auto& row : *rows)
            width = std::max(width, rack->rowColumns(row) * m.colW + (static_cast<int>(row.size()) - 1) * m.gap);
    const int rows = static_cast<int>(rack->rows().size());
    const int pad = m.gap + 2;
    // title, gap, rows with gaps between, gap, keyboard
    setSize(pad + width + pad, pad + m.titleH + m.gap + rows * m.rowH + (rows - 1) * m.gap + m.gap + m.keysH + pad);
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
    setTooltip("Drag to move this panel.\nArrow keys (when selected) or right-click: move menu.");
}

void LMWSynthEditor::PanelHeader::paint(juce::Graphics& g) {
    // Spec S6.9: the title bar in the unit's colour (raised to the contrast preset; white at Max).
    const auto& title = editor.groups[static_cast<std::size_t>(panel)].title;
    g.fillAll(toJuce(editor.palette.unitColour(lmw::ui::theme::unitOf(title.toStdString()))));
    g.setColour(kFg);
    if (editor.draggedPanel == panel) g.drawRect(getLocalBounds(), 2);   // keep the thick drag frame visible
    g.setFont(juce::FontOptions(editor.m.titleFont, juce::Font::bold));
    g.drawText(title, getLocalBounds(), juce::Justification::centred);
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
    editor.dragPanel(panel, e.getEventRelativeTo(&editor).getPosition());
}

void LMWSynthEditor::PanelHeader::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.dropPanel(panel, e.getEventRelativeTo(&editor).getPosition());
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
        t.marker = {first.getX(), y - 2, getWidth() - 2 * first.getX(), 4};
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
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), panel](int result) {
                           if (safe == nullptr) return;
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

    if (currentTab == 1) {                                   // Effects tab: fixed panels, titles drawn here
        g.setFont(juce::FontOptions(m.titleFont, juce::Font::bold));
        for (const auto& grp : effectGroups) {
            const auto& b = grp.bounds;
            g.drawRect(b, 1);
            g.drawText(grp.title, b.getX(), b.getY(), b.getWidth(), m.header - 3, juce::Justification::centred);
            g.drawHorizontalLine(b.getY() + m.header - 3, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));
        }
    }
    if (currentTab != 0) return;                             // other tabs: no rack panel frames
    for (std::size_t i = 0; i < groups.size(); ++i) {        // titles are drawn by the PanelHeaders
        const auto& b = groups[i].bounds;
        g.drawRect(b, static_cast<int>(i) == draggedPanel ? 3 : 1);
        if (!groupJacks[i].empty())                          // line above the patch points
            g.drawHorizontalLine(b.getBottom() - 3 - m.jackH, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));
    }
    if (drop) g.fillRect(drop->marker);                     // drop marker (Spec S6.5)
}

void LMWSynthEditor::layoutItem(const Item& item, juce::Rectangle<int> column) const {
    column.reduce(m.colW >= 80 ? 4 : 2, 0);
    if (item.fader != nullptr) {
        item.fader->label.setBounds(column.removeFromTop(m.labelH));
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
    }
}

void LMWSynthEditor::resized() {
    if (!rack) return;
    const int pad = m.gap + 2;
    auto area = getLocalBounds().reduced(pad);
    auto title = area.removeFromTop(m.titleH);
    compactButton.setBounds(title.removeFromRight(90).reduced(0, 2));
    title.removeFromRight(m.gap);
    resetLayoutButton.setBounds(title.removeFromRight(110).reduced(0, 2));
    title.removeFromRight(m.gap);
    contrastBox.setBounds(title.removeFromRight(m.colW >= 80 ? 130 : 116).reduced(0, 3));
    title.removeFromRight(m.gap);
    saveButton.setBounds(title.removeFromRight(m.colW >= 80 ? 76 : 64).reduced(0, 2));
    title.removeFromRight(m.gap / 2);
    loadButton.setBounds(title.removeFromRight(m.colW >= 80 ? 76 : 64).reduced(0, 2));
    titleLabel.setBounds(title.removeFromLeft(titleTextWidth()));
    titleLabel.setFont(juce::FontOptions(m.titleH * 0.6f, juce::Font::bold));
    title.removeFromLeft(m.gap);
    const int tabW = m.colW >= 80 ? 70 : 60;
    panelTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));
    effectsTab.setBounds(title.removeFromLeft(tabW + (m.colW >= 80 ? 10 : 8)).reduced(0, 2));
    matrixTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));
    schematicTab.setBounds(title.removeFromLeft(tabW + (m.colW >= 80 ? 22 : 18)).reduced(0, 2));
    notesTab.setBounds(title.removeFromLeft(tabW).reduced(0, 2));
    title.removeFromLeft(m.gap);
    patchLabel.setBounds(title);
    area.removeFromTop(m.gap);
    keyboard.setBounds(area.removeFromBottom(m.keysH));
    // All 128 keys (75 white) fill a wide window; narrower windows keep 16 px keys and scroll.
    keyboard.setKeyWidth(std::max(16.0f, static_cast<float>(keyboard.getWidth()) / 75.0f));
    area.removeFromBottom(m.gap);
    notesEditor.setBounds(area);                             // Notes tab: the area of the panels
    if (matrix) { matrix->setBounds(area); matrix->setFontSize(m.labelFont - 2.0f); }   // Matrix tab
    if (schematic) schematic->setBounds(area);              // Schematic tab
    {                                                        // Effects tab: panels side by side, first row
        auto row = area.withHeight(m.rowH);
        for (auto& grp : effectGroups) {
            grp.bounds = row.removeFromLeft(grp.columns() * m.colW);
            row.removeFromLeft(m.gap);
            auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
            inner.removeFromBottom(m.jackH);                 // same fader height as on the Synth tab
            for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
        }
    }

    // Spec S6.5: panels in the order of the rack layout.
    rowBounds.clear();
    for (const auto& rowPanels : rack->rows()) {
        if (!rowBounds.empty()) area.removeFromTop(m.gap);
        auto row = area.removeFromTop(m.rowH);
        rowBounds.push_back(row);
        for (int index : rowPanels) {
            auto& grp = groups[static_cast<std::size_t>(index)];
            grp.bounds = row.removeFromLeft(grp.columns() * m.colW);
            row.removeFromLeft(m.gap);
            headers[static_cast<std::size_t>(index)]->setBounds(grp.bounds.getX() + 1, grp.bounds.getY() + 1,
                                                                 grp.bounds.getWidth() - 2, m.header - 4);
            auto inner = grp.bounds.reduced(0, 3).withTrimmedTop(m.header);
            auto jackRow = inner.removeFromBottom(m.jackH).withTrimmedTop(2);   // Spec S6.6
            const auto& js = groupJacks[static_cast<std::size_t>(index)];
            for (int c = 0; c < grp.columns(); ++c) {                            // sockets per fader column
                std::vector<Jack*> col;
                for (auto* j : js) if (j->column == c) col.push_back(j);
                const int left = jackRow.getX() + c * m.colW, n = static_cast<int>(col.size());
                for (int k = 0; k < n; ++k)
                    col[static_cast<std::size_t>(k)]->setBounds(left + m.colW * k / n, jackRow.getY(),
                                                                m.colW * (k + 1) / n - m.colW * k / n, jackRow.getHeight());
            }
            for (const auto& item : grp.items) layoutItem(item, inner.removeFromLeft(m.colW));
        }
    }
    if (cables) cables->setBounds(getLocalBounds());
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

juce::Rectangle<float> LMWSynthEditor::Jack::socket() const {
    const float d = editor.m.colW >= 80 ? 16.0f : 14.0f;
    return juce::Rectangle<float>(d, d).withCentre({getWidth() * 0.5f, 2.0f + d * 0.5f});
}

juce::Point<float> LMWSynthEditor::Jack::centreInEditor() const {
    return editor.getLocalPoint(this, socket().getCentre());
}

void LMWSynthEditor::Jack::paint(juce::Graphics& g) {
    const auto c = socket();
    const auto& patch = editor.processor.currentPatch();
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
        if (patch.isPatched(static_cast<lmw::dsp::In>(port))) g.fillEllipse(c.reduced(4.5f));
    }
    g.setColour(kFg);
    if (editor.drag && editor.drag->target == this) g.drawEllipse(c.expanded(1.5f), 2.0f);   // drop target
    if (hasKeyboardFocus(false)) g.drawRect(getLocalBounds(), 1);                           // focus ring
    // Label shrinks to fit a narrow slot (several sockets under one fader column).
    float size = editor.m.colW >= 80 ? 12.0f : 11.0f;
    juce::Font font{juce::FontOptions(size)};
    while (size > 8.0f && juce::GlyphArrangement::getStringWidth(font, name) > static_cast<float>(getWidth() - 2)) {
        size -= 0.5f;
        font = juce::Font(juce::FontOptions(size));
    }
    g.setFont(font);
    g.drawText(name, getLocalBounds().withTrimmedTop(static_cast<int>(c.getBottom()) + 1), juce::Justification::centredTop, false);
}

juce::String LMWSynthEditor::Jack::getTooltip() {
    if (output)
        return editor.describeOutput(static_cast<lmw::dsp::Out>(port))
             + "\nDrag to an input to patch a cable. Enter: cable menu.";
    const auto in = static_cast<lmw::dsp::In>(port);
    return editor.describeInput(in) + "\nDrag to an output to add a cable (up to 4 per input)"
         + (editor.processor.currentPatch().isPatched(in) ? "; drag the newest cable away to remove it. Delete: remove all." : ".")
         + "\nEnter: cable menu.";
}

void LMWSynthEditor::Jack::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) editor.showJackMenu(*this);
}

void LMWSynthEditor::Jack::mouseDrag(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || (!dragging && e.getDistanceFromDragStart() < 3)) return;
    dragging = true;
    editor.cableDrag(*this, e.getEventRelativeTo(&editor).position);
}

void LMWSynthEditor::Jack::mouseUp(const juce::MouseEvent& e) {
    if (!dragging) return;
    dragging = false;
    editor.cableDrop(*this, e.getEventRelativeTo(&editor).position);
}

bool LMWSynthEditor::Jack::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && dragging) { dragging = false; editor.cancelCableDrag(); return true; }
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::returnKey || (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())) {
        editor.showJackMenu(*this);
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
    // A cable hangs down a little between its ends; drawn black with a white edge so it stays
    // visible where it crosses black faders (only black and white, Spec S6.2).
    const auto draw = [&g](juce::Point<float> a, juce::Point<float> b) {
        const float sag = 15.0f + 0.2f * a.getDistanceFrom(b);   // the middle hangs lowest
        juce::Path path;
        path.startNewSubPath(a);
        path.quadraticTo((a + b) * 0.5f + juce::Point<float>(0.0f, sag), b);
        const juce::PathStrokeType edge(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const juce::PathStrokeType core(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(kBg);
        g.strokePath(path, edge);
        g.setColour(kFg);
        g.strokePath(path, core);
        for (auto p : {a, b}) g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(p));   // plugs
    };
    const auto& patch = editor.processor.currentPatch();
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {                // one cable per pin (Spec S5.14)
        const auto in = static_cast<In>(i);
        for (int k = 0; k < patch.pinCount(in); ++k) {
            const int src = patch.pin(in, k).source;
            if (editor.drag && editor.drag->detachedInput == i && editor.drag->detachedSource == src) continue;
            auto* from = editor.jackFor(true, src);
            auto* to = editor.jackFor(false, i);
            if (from != nullptr && to != nullptr) draw(from->centreInEditor(), to->centreInEditor());
        }
    }
    if (const auto& d = editor.drag)
        draw(d->fixed->centreInEditor(), d->target != nullptr ? d->target->centreInEditor() : d->loose);
}

LMWSynthEditor::Jack* LMWSynthEditor::jackFor(bool output, int port) const {
    for (const auto& j : jacks)
        if (j->output == output && j->port == port) return j.get();
    return nullptr;
}

LMWSynthEditor::Jack* LMWSynthEditor::findJack(juce::Point<float> where, const Jack& from) const {
    for (const auto& j : jacks)
        if (j->output != from.output && getLocalArea(j.get(), j->getLocalBounds()).toFloat().expanded(4.0f).contains(where))
            return j.get();
    return nullptr;
}

void LMWSynthEditor::cableDrag(Jack& from, juce::Point<float> where) {
    using lmw::dsp::In;
    if (!drag) {
        CableDrag d;
        const auto& patch = processor.currentPatch();
        const auto in = static_cast<In>(from.port);
        if (!from.output && patch.isPatched(in)) {          // pick up the newest cable plugged in here
            const auto& pin = patch.pin(in, patch.pinCount(in) - 1);
            d.detachedInput = from.port;
            d.detachedSource = pin.source;
            d.detachedLevel = pin.level;
            d.fixed = jackFor(true, pin.source);
        } else {
            d.fixed = &from;
        }
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
    if (d.detachedInput >= 0) {
        if (d.target == nullptr) {
            connect(d.detachedInput, d.detachedSource, 0.0f);           // dropped in the open: removed
        } else if (d.target->port != d.detachedInput) {                  // moved to another input, same level
            if (connect(d.target->port, d.detachedSource, d.detachedLevel)) connect(d.detachedInput, d.detachedSource, 0.0f);
        }
    } else if (d.target != nullptr) {
        if (d.fixed->output) connect(d.target->port, d.fixed->port);
        else connect(d.fixed->port, d.target->port);
    }
    repaint();
}

void LMWSynthEditor::cancelCableDrag() {
    drag.reset();
    repaint();
}

bool LMWSynthEditor::connect(int input, int output, float level) {
    const bool ok = processor.requestPatch(static_cast<lmw::dsp::In>(input), static_cast<std::int8_t>(output), level);
    if (!ok && output != lmw::dsp::Patch::none && level > 0.0f)
        showError("Input full", portName(false, input) + " already has " + juce::String(lmw::dsp::Patch::maxPins)
                                + " cables. Remove one first.");
    shownPatchVersion = processor.patchVersion();
    repaint();
    return ok;
}

void LMWSynthEditor::removeAllCables() {
    for (int i = 0; i < lmw::dsp::numInputs; ++i)
        if (processor.currentPatch().isPatched(static_cast<lmw::dsp::In>(i))) connect(i, lmw::dsp::Patch::none);
}

juce::String LMWSynthEditor::describeOutput(lmw::dsp::Out out) const {
    const auto& patch = processor.currentPatch();
    juce::StringArray fed;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) {
        const auto in = static_cast<lmw::dsp::In>(i);
        if (!patch.isPatched(in)) {
            if (lmw::dsp::normalled[static_cast<std::size_t>(i)] == out) fed.add(portName(false, i) + " (normalled)");
        } else if (const float l = patch.level(in, out); l > 0.0f) {
            fed.add(portName(false, i) + (l < 1.0f ? " (cable " + juce::String(juce::roundToInt(l * 100.0f)) + " %)" : " (cable)"));
        }
    }
    return portName(true, static_cast<int>(out)) + " " + juce::String::fromUTF8("\xe2\x86\x92") + " "
         + (fed.isEmpty() ? juce::String("(not connected)") : fed.joinIntoString(", "));
}

void LMWSynthEditor::showJackMenu(Jack& jack) {
    using lmw::dsp::In;
    using lmw::dsp::Out;
    const auto& patch = processor.currentPatch();
    bool anyCable = false;
    for (int i = 0; i < lmw::dsp::numInputs; ++i) anyCable = anyCable || patch.isPatched(static_cast<In>(i));

    juce::PopupMenu menu, sub;
    menu.setLookAndFeel(&lookAndFeel);               // black on white (Spec S6.2)
    sub.setLookAndFeel(&lookAndFeel);
    if (!jack.output) {
        const auto in = static_cast<In>(jack.port);
        menu.addSectionHeader(describeInput(in));
        const bool full = patch.pinCount(in) >= lmw::dsp::Patch::maxPins;
        for (int o = 0; o < lmw::dsp::numOutputs; ++o) {           // only possible items (no greyed ones, contrast)
            const bool cabled = patch.level(in, static_cast<Out>(o)) > 0.0f;
            if (cabled || !full) sub.addItem(100 + o, portName(true, o), true, cabled);
        }
        menu.addSubMenu("Connect from (ticked = cable; choose again to remove)", sub);
        if (patch.isPatched(in)) menu.addItem(1, patch.pinCount(in) > 1 ? "Remove all cables here" : "Remove cable");
    } else {
        const auto out = static_cast<Out>(jack.port);
        bool feeds = false;
        menu.addSectionHeader("Output " + portName(true, jack.port));
        for (int i = 0; i < lmw::dsp::numInputs; ++i) {
            const auto in = static_cast<In>(i);
            const bool cabled = patch.level(in, out) > 0.0f;
            feeds = feeds || cabled;
            if (cabled || patch.pinCount(in) < lmw::dsp::Patch::maxPins) sub.addItem(200 + i, portName(false, i), true, cabled);
        }
        menu.addSubMenu("Connect to (ticked = cable; choose again to remove)", sub);
        if (feeds) menu.addItem(2, "Remove cables from here");
    }
    if (anyCable) {
        menu.addSeparator();
        menu.addItem(3, "Remove all cables");
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&jack),
                       [safe = juce::Component::SafePointer<LMWSynthEditor>(this), output = jack.output,
                        port = jack.port](int r) {
        if (safe == nullptr || r == 0) return;
        const auto& p = safe->processor.currentPatch();
        if (r == 1) {
            safe->connect(port, lmw::dsp::Patch::none);
        } else if (r == 2) {
            for (int i = 0; i < lmw::dsp::numInputs; ++i)
                if (p.level(static_cast<In>(i), static_cast<lmw::dsp::Out>(port)) > 0.0f) safe->connect(i, port, 0.0f);
        } else if (r == 3) {
            safe->removeAllCables();
        } else if (!output && r >= 100 && r < 100 + lmw::dsp::numOutputs) {    // toggle that cable
            const bool cabled = p.level(static_cast<In>(port), static_cast<lmw::dsp::Out>(r - 100)) > 0.0f;
            safe->connect(port, r - 100, cabled ? 0.0f : 1.0f);
        } else if (output && r >= 200 && r < 200 + lmw::dsp::numInputs) {
            const bool cabled = p.level(static_cast<In>(r - 200), static_cast<lmw::dsp::Out>(port)) > 0.0f;
            safe->connect(r - 200, port, cabled ? 0.0f : 1.0f);
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
}

void LMWSynthEditor::updateContrastBox() {
    char text[32];
    std::snprintf(text, sizeof text, "Custom %.1f:1", settings.customRatio);
    contrastBox.changeItemText(4, text);
    contrastBox.setSelectedId(static_cast<int>(settings.contrast) + 1, juce::dontSendNotification);
}

void LMWSynthEditor::applyContrast() {
    palette = lmw::ui::theme::palette(settings.minRatio());
    updateContrastBox();
    saveConfig();
    keyboard.repaint();
    if (matrix) matrix->setPalette(palette);
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
            safe->applyContrast();                      // also restores the box after Cancel
        }), true);
}

void LMWSynthEditor::Keyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                             bool isOver, juce::Colour lineColour, juce::Colour textColour) {
    if (isDown) {
        g.setColour(toJuce(pal.whiteKeyDown));
        g.fillRect(area);
        textColour = toJuce(lmw::ui::theme::textOn(pal.whiteKeyDown));
    } else if (isOver) {
        g.setColour(kFg);
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
        g.setColour(kFg);                               // keeps its edge where it meets white keys
        g.drawRect(area, 1.0f);
    } else if (isOver) {
        g.setColour(kBg);
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

int LMWSynthEditor::titleTextWidth() const { return m.colW >= 80 ? 120 : 96; }

void LMWSynthEditor::showTab(int tab) {
    currentTab = tab;
    const juce::Component* alwaysShown[] = {&titleLabel, &panelTab, &effectsTab, &matrixTab, &schematicTab, &notesTab, &patchLabel, &loadButton,
                                            &saveButton, &contrastBox, &resetLayoutButton, &compactButton, &keyboard};
    for (auto* child : getChildren()) {
        if (child == &notesEditor || child == matrix.get() || child == schematic.get()
            || std::find(std::begin(alwaysShown), std::end(alwaysShown), child) != std::end(alwaysShown))
            continue;
        const bool effect = std::find(effectComponents.begin(), effectComponents.end(), child) != effectComponents.end();
        child->setVisible(effect ? tab == 1 : tab == 0);   // hidden controls take no Tab focus either
    }
    matrix->setVisible(tab == 2);
    schematic->setVisible(tab == 4);
    notesEditor.setVisible(tab == 3);
    resetLayoutButton.setEnabled(true);
    if (tab == 2) matrix->grabKeyboardFocus();
    if (tab == 3) notesEditor.grabKeyboardFocus();
    repaint();
}

void LMWSynthEditor::refreshPatchInfo() {
    shownPatchFile = processor.patchFileVersion();
    const auto name = processor.patchName();
    patchLabel.setText(name.isEmpty() ? juce::String() : "Patch: " + name, juce::dontSendNotification);
    if (notesEditor.getText() != processor.notes()) notesEditor.setText(processor.notes(), false);
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
    // JUCE's own dialog (not the system one), so it is black on white like the rest (Spec S6.2).
    chooser = std::make_unique<juce::FileChooser>("Save patch", patchFolder().getChildFile(name + ".lmwpatch"),
                                                  "*.lmwpatch", false, false, this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<LMWSynthEditor>(this)](const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (!file.hasFileExtension("lmwpatch")) file = file.withFileExtension("lmwpatch");
            const auto patchName = file.getFileNameWithoutExtension();
            safe->processor.setPatchName(patchName);
            if (!file.replaceWithText(safe->processor.patchText(patchName)))
                safe->showError("Save patch", "Could not write " + file.getFullPathName());
            safe->refreshPatchInfo();
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
            if (!safe->processor.loadPatchText(file.loadFileAsString(), error)) {
                safe->showError("Load patch", file.getFileName() + ": " + error);
                return;
            }
            if (safe->processor.patchName().isEmpty()) safe->processor.setPatchName(file.getFileNameWithoutExtension());
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
