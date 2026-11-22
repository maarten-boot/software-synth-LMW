// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AssistantView.h"
#include "PluginProcessor.h"
#include "assistant/ToolLine.h"

namespace {
const juce::Colour ink = juce::Colours::black;                  // S6.2: black on white, 21:1
const juce::Colour quiet(0xff4a4a4a);                           // tool lines: #4A4A4A on white, 8.9:1 (AAA)

juce::String dollars(double d) {
    return "$" + juce::String(d, d < 0.1 ? 3 : 2);
}
} // namespace

AssistantView::AssistantView(LMWSynthProcessor& p) : session(std::make_unique<AssistantSession>(p)) {
    setTitle("Assistant");
    log.setMultiLine(true, true);
    log.setReadOnly(true);
    log.setCaretVisible(false);
    log.setScrollbarsShown(true);
    log.setTitle("Conversation with the assistant");
    log.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
    log.setColour(juce::TextEditor::outlineColourId, ink);
    addAndMakeVisible(log);

    input.setMultiLine(true, true);
    input.setReturnKeyStartsNewLine(false);
    input.setTitle("Your question or wish");
    input.setTextToShowWhenEmpty("Ask a question or describe a sound, e.g. \"a warm pad with a slow attack\"", quiet);
    input.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
    input.setColour(juce::TextEditor::textColourId, ink);
    input.setColour(juce::TextEditor::outlineColourId, ink);
    input.send = [this] { send(); };
    addAndMakeVisible(input);

    sendButton.setTooltip("Send the question (Enter; Shift+Enter starts a new line)");
    sendButton.onClick = [this] { send(); };
    stopButton.setTooltip("Stop after the step that is running; changes made so far stay (Undo last answer)");
    stopButton.onClick = [this] { session->stop(); statusLabel.setText("stopping...", juce::dontSendNotification); };
    newButton.setTooltip("Start a new conversation (the sound stays as it is)");
    newButton.onClick = [this] {
        session->newConversation();
        log.clear();
        empty = true;
        showHelp();
        updateButtons();
    };
    undoButton.setTooltip("Put back the sound from before the last answer; press again for the one before");
    undoButton.onClick = [this] {
        AssistantSession::Callbacks cb;
        cb.undone = [this, a = std::weak_ptr<bool>(alive)](bool ok, const std::string& error) {
            if (a.expired()) return;
            add(ok ? "(the sound from before the last answer is back)\n\n" : "(undo failed: " + juce::String(error) + ")\n\n", quiet, false, true);
            updateButtons();
        };
        if (session->undo(cb)) updateButtons();
    };
    settingsButton.setTooltip("Key, model and costs");
    settingsButton.onClick = [this] { if (openSettings) openSettings(); };
    for (auto* b : {&sendButton, &stopButton, &newButton, &undoButton, &settingsButton}) addAndMakeVisible(*b);
    for (auto* l : {&costLabel, &statusLabel}) {
        l->setColour(juce::Label::textColourId, ink);
        addAndMakeVisible(*l);
    }
    costLabel.setJustificationType(juce::Justification::centredRight);
    updateButtons();
}

void AssistantView::ready() {
    if (empty) { log.clear(); showHelp(); }
    updateButtons();
    resized();
}

AssistantView::~AssistantView() {
    *alive = false;
    session.reset();                                       // stops and waits for the thread
}

void AssistantView::setFontSize(float size) {
    fontSize = size;
    input.applyFontToAllText(juce::FontOptions(size));
    input.setFont(juce::FontOptions(size));
    log.applyFontToAllText(juce::FontOptions(size), false);
    for (auto* l : {&costLabel, &statusLabel}) l->setFont(juce::FontOptions(size - 1.0f));
}

void AssistantView::focusInput() { input.grabKeyboardFocus(); }

void AssistantView::paint(juce::Graphics& g) { g.fillAll(juce::Colours::white); }

void AssistantView::resized() {
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom(26);
    statusLabel.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2));
    costLabel.setBounds(bottom);
    area.removeFromBottom(4);
    auto buttons = area.removeFromBottom(30);
    const int w = 130;
    sendButton.setBounds(buttons.removeFromRight(90).reduced(2));
    stopButton.setBounds(buttons.removeFromRight(80).reduced(2));
    settingsButton.setBounds(buttons.removeFromLeft(settingsButton.isVisible() ? 110 : 0).reduced(2));
    newButton.setBounds(buttons.removeFromLeft(w + 20).reduced(2));
    undoButton.setBounds(buttons.removeFromLeft(w + 20).reduced(2));
    area.removeFromBottom(4);
    input.setBounds(area.removeFromBottom(std::max(60, static_cast<int>(fontSize * 4.5f))));
    area.removeFromBottom(6);
    log.setBounds(area);
}

void AssistantView::add(const juce::String& text, juce::Colour colour, bool bold, bool italic) {
    log.moveCaretToEnd();
    auto f = juce::FontOptions(fontSize);
    if (bold) f = f.withStyle("Bold");
    if (italic) f = f.withStyle("Italic");
    log.setFont(f);
    log.setColour(juce::TextEditor::textColourId, colour);
    log.insertTextAtCaret(text);
    log.moveCaretToEnd();
}

void AssistantView::showHelp() {
    lmw::assistant::Settings probe;                        // S12.6: say first what is needed before the first question
    if (configure) configure(probe);
    if (probe.key.empty() && juce::SystemStats::getEnvironmentVariable("ANTHROPIC_API_KEY", {}).isEmpty())
        add("Before the first question: the assistant needs a Claude API key (a paid account at Anthropic, a few cents "
            "per question). Press Settings... below to enter it; \"How to get a key...\" there explains where to get one.\n\n",
            ink, true);
    add("Ask about the synth or describe the sound you want, in your own words. For example:\n"
        "  - Make a soft flute with a slow vibrato.\n"
        "  - Why does my sound click at the start of a note?\n"
        "  - What does the COMB filter do?\n"
        "The assistant changes the sound, plays a note so you hear it, and says what it did. "
        "Undo last answer puts the sound back.\n\n", quiet, false, true);
}

void AssistantView::updateButtons() {
    const bool busy = session->busy();
    sendButton.setEnabled(!busy);
    stopButton.setEnabled(busy);
    newButton.setEnabled(!busy);
    undoButton.setEnabled(!busy && session->canUndo());
    settingsButton.setVisible(static_cast<bool>(openSettings));
    settingsButton.setEnabled(!busy);
}

void AssistantView::updateCost(double answer) {
    costLabel.setText("this answer " + dollars(answer) + "   this session " + dollars(session->sessionCost()) + " (about)",
                      juce::dontSendNotification);
}

void AssistantView::send() {
    const auto question = input.getText().trim();
    if (question.isEmpty() || session->busy()) return;
    auto& s = session->settings();
    s.key.clear();
    if (configure) configure(s);
    if (s.key.empty()) s.key = juce::SystemStats::getEnvironmentVariable("ANTHROPIC_API_KEY", {}).toStdString();   // S12.6
    if (s.key.empty()) {
        add("The assistant needs a Claude API key first (a paid account at Anthropic). The window \"Assistant settings\" "
            "opens now: paste your key there and press OK. Its button \"How to get a key...\" explains where to get one. "
            "Later you find the same window with the Settings... button below or under Menu > Assistant settings....\n\n", ink, true);
        if (openSettings) openSettings();
        return;
    }
    if (empty) { log.clear(); empty = false; }
    add("You: ", ink, true);
    add(question + "\n", ink);
    input.clear();

    AssistantSession::Callbacks cb;
    const auto a = std::weak_ptr<bool>(alive);
    cb.text = [this, a](const std::string& t) {
        if (a.expired()) return;
        add("Assistant: ", ink, true);
        add(juce::String::fromUTF8(t.c_str()).trim() + "\n", ink);
    };
    cb.tool = [this, a](const std::string& tool, const lmw::json::Value& in, const std::string& result, bool isError) {
        if (a.expired()) return;
        add("  " + juce::String::fromUTF8(lmw::assistant::toolLine(tool, in, result, isError).c_str()) + "\n", quiet, false, true);
    };
    cb.status = [this, a](const std::string& t) {
        if (!a.expired()) statusLabel.setText(juce::String::fromUTF8(t.c_str()), juce::dontSendNotification);
    };
    cb.confirmSave = [this, a](const std::string& name, bool overwrite, std::function<void(bool)> reply) {
        if (a.expired()) { reply(false); return; }
        const auto n = juce::String::fromUTF8(name.c_str());
        auto opts = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::NoIcon)
                        .withTitle("Save instrument preset")
                        .withMessage(overwrite ? "The assistant wants to replace your preset \"" + n + "\" with this sound."
                                               : "The assistant wants to save this sound as your preset \"" + n + "\".")
                        .withButton(overwrite ? "Replace" : "Save", 1)
                        .withButton("Cancel", 0)
                        .withAssociatedComponent(this);
        juce::AlertWindow::showAsync(opts, [reply](int result) { reply(result == 1); });
    };
    cb.done = [this, a](const lmw::assistant::Outcome& o) {
        if (!a.expired()) finish(o);
    };
    statusLabel.setText("working...", juce::dontSendNotification);
    if (!session->ask(question.toStdString(), cb)) {
        add("(the assistant is not ready)\n\n", quiet, false, true);
        statusLabel.setText({}, juce::dontSendNotification);
    }
    updateButtons();
}

void AssistantView::finish(const lmw::assistant::Outcome& o) {
    if (o.end != lmw::assistant::End::Done)
        add("(" + juce::String::fromUTF8(o.message.c_str()) + ")\n", o.end == lmw::assistant::End::Failed ? ink : quiet,
            o.end == lmw::assistant::End::Failed, o.end != lmw::assistant::End::Failed);
    add("\n", ink);
    statusLabel.setText({}, juce::dontSendNotification);
    updateCost(o.cost);
    updateButtons();
    input.grabKeyboardFocus();
}
