// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.5: the Assistant tab (standalone only, S12.1) — the conversation (questions, answers, one line
// per change), an input box (Enter sends, Shift+Enter starts a new line), Send / Stop, New conversation,
// Undo last answer, and the cost (S12.7). Black on white like every tab (S6.2). The work is done by
// AssistantSession (S12.2) on its own thread.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "AssistantSession.h"
#include <functional>
#include <memory>

class LMWSynthProcessor;

class AssistantView final : public juce::Component {
public:
    explicit AssistantView(LMWSynthProcessor&);
    ~AssistantView() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void setFontSize(float size);
    void focusInput();
    void ready();                                       // after configure/openSettings are set: help text, buttons

    // Hooks for 22c/22d: where the key and settings come from, and the questions before sending.
    std::function<void(lmw::assistant::Settings&)> configure;   // key, model, effort, limit (S12.6, S12.7)
    std::function<void()> openSettings;                 // Settings... (none: button hidden)
    std::function<bool()> needsNotice;                  // S12.8: the privacy notice not yet accepted
    std::function<void()> noticeAccepted;               // remember it (config.ini)

    // S12.8: the notice, also shown by the settings dialog.
    static juce::String privacyText();

private:
    struct Input final : juce::TextEditor {
        std::function<void()> send;
        bool keyPressed(const juce::KeyPress& k) override {
            if (k.getKeyCode() == juce::KeyPress::returnKey && !k.getModifiers().isShiftDown()) {
                if (send) send();
                return true;
            }
            return juce::TextEditor::keyPressed(k);
        }
    };

    void send();
    void sendNow(const juce::String& question);         // after the notice and the limit question
    void add(const juce::String& text, juce::Colour colour, bool bold = false, bool italic = false);
    void finish(const lmw::assistant::Outcome&);
    void updateButtons();
    void updateCost(double answer);
    void showHelp();

    std::unique_ptr<AssistantSession> session;
    juce::TextEditor log;
    Input input;
    juce::TextButton sendButton{"Send"}, stopButton{"Stop"}, newButton{"New conversation"}, undoButton{"Undo last answer"},
        settingsButton{"Settings..."};
    juce::Label costLabel, statusLabel;
    float fontSize = 15.0f;
    bool empty = true;
    double raisedLimit = 0;                             // S12.7: "go on" raised the session limit to this
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
