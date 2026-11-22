// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.6: the Assistant settings dialog — provider, API key (hidden, Paste, Clear, Test connection),
// model with its prices, effort, where the key is kept (system keychain or the settings file), the spending
// limit per session (S12.7), and "How to get a key". OK applies everything at once, Cancel nothing.
// Black on white like every dialog (S6.2).
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "AssistantSession.h"
#include "assistant/Claude.h"
#include <functional>
#include <string>

class AssistantSettings final : public juce::Component {
public:
    struct Values {
        std::string store = "file";              // "keychain" or "file"
        std::string key, model = "claude-opus-5-5", effort = "medium";
        double limit = 2.0;
    };
    std::function<void(const Values&)> onOk;
    std::function<void()> onCancel, onHowTo;

    AssistantSettings(Values v, bool keychainAvailable) : values(std::move(v)) {
        const auto label = [this](juce::Label& l, const juce::String& t) {
            l.setText(t, juce::dontSendNotification);
            l.setColour(juce::Label::textColourId, juce::Colours::black);
            addAndMakeVisible(l);
        };
        label(providerName, "Provider");
        provider.addItem("Claude (Anthropic)", 1);
        provider.setSelectedId(1, juce::dontSendNotification);
        provider.setTooltip("More providers (OpenAI-compatible, local models) may follow");
        provider.setTitle("Provider");
        addAndMakeVisible(provider);

        label(keyName, "API key");
        key.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
        key.setText(juce::String::fromUTF8(values.key.c_str()), false);
        key.setTitle("API key");
        key.setTextToShowWhenEmpty("sk-ant-...", juce::Colour(0xff4a4a4a));
        key.onTextChange = [this] { testResult.setText({}, juce::dontSendNotification); };
        addAndMakeVisible(key);
        paste.setButtonText("Paste");
        paste.setTooltip("Paste the key you copied from the Claude Console");
        paste.onClick = [this] { key.setText(juce::SystemClipboard::getTextFromClipboard().trim(), true); };
        clear.setButtonText("Clear");
        clear.onClick = [this] { key.clear(); };
        test.setButtonText("Test connection");
        test.setTooltip("Asks the service whether the key works (free)");
        test.onClick = [this] { runTest(); };
        howTo.setButtonText("How to get a key...");
        howTo.onClick = [this] { if (onHowTo) onHowTo(); };
        for (auto* b : {&paste, &clear, &test, &howTo}) addAndMakeVisible(*b);
        testResult.setColour(juce::Label::textColourId, juce::Colours::black);
        addAndMakeVisible(testResult);

        label(modelName, "Model");
        int id = 1, chosen = 1;
        for (const auto& p : lmw::assistant::defaultPrices()) {
            model.addItem(juce::String(p.label) + "   $" + juce::String(p.in, 2) + " in / $" + juce::String(p.out, 2)
                              + " out per million tokens", id);
            if (p.model == values.model) chosen = id;
            ++id;
        }
        model.setSelectedId(chosen, juce::dontSendNotification);
        model.setTitle("Model");
        addAndMakeVisible(model);

        label(effortName, "Effort");
        effort.addItem("Low (fastest, cheapest)", 1);
        effort.addItem("Medium", 2);
        effort.addItem("High (most careful, costs more)", 3);
        effort.setSelectedId(values.effort == "low" ? 1 : values.effort == "high" ? 3 : 2, juce::dontSendNotification);
        effort.setTitle("Effort");
        addAndMakeVisible(effort);

        label(storeName, "Keep the key in");
        keychain.setButtonText("the system keychain");
        file.setButtonText("the settings file (plain text, readable by programs you run)");
        for (auto* t : {&keychain, &file}) {
            t->setRadioGroupId(7);
            t->setColour(juce::ToggleButton::textColourId, juce::Colours::black);
            t->setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
            t->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff4a4a4a));
            addAndMakeVisible(*t);
        }
        keychain.setEnabled(keychainAvailable);
        if (!keychainAvailable) keychain.setTooltip("No system keychain found on this computer");
        (values.store == "keychain" && keychainAvailable ? keychain : file).setToggleState(true, juce::dontSendNotification);

        label(limitName, "Spending limit");
        limit.setText(juce::String(values.limit, 2), false);
        limit.setInputRestrictions(8, "0123456789.");
        limit.setTitle("Spending limit per session in dollars");
        addAndMakeVisible(limit);
        label(limitNote, "dollars per session, then it asks before going on (0 = no limit)");

        ok.setButtonText("OK");
        cancel.setButtonText("Cancel");
        ok.onClick = [this] { if (onOk) onOk(current()); };
        cancel.onClick = [this] { if (onCancel) onCancel(); };
        for (auto* b : {&ok, &cancel}) addAndMakeVisible(*b);
        setSize(720, 400);
    }

    Values current() const {
        Values v;
        v.store = keychain.getToggleState() ? "keychain" : "file";
        v.key = key.getText().trim().toStdString();
        const auto& prices = lmw::assistant::defaultPrices();
        const auto m = static_cast<std::size_t>(std::max(1, model.getSelectedId()) - 1);
        v.model = m < prices.size() ? prices[m].model : values.model;
        v.effort = effort.getSelectedId() == 1 ? "low" : effort.getSelectedId() == 3 ? "high" : "medium";
        v.limit = std::max(0.0, limit.getText().getDoubleValue());
        return v;
    }

    void resized() override {
        auto area = getLocalBounds().reduced(14);
        const auto row = [&area](int h = 30) { auto r = area.removeFromTop(h); area.removeFromTop(6); return r; };
        const int nameW = 130;
        auto r = row();
        providerName.setBounds(r.removeFromLeft(nameW));
        provider.setBounds(r.removeFromLeft(260));
        r = row();
        keyName.setBounds(r.removeFromLeft(nameW));
        clear.setBounds(r.removeFromRight(70));
        r.removeFromRight(6);
        paste.setBounds(r.removeFromRight(70));
        r.removeFromRight(6);
        key.setBounds(r);
        r = row();
        r.removeFromLeft(nameW);
        test.setBounds(r.removeFromLeft(140));
        r.removeFromLeft(8);
        howTo.setBounds(r.removeFromRight(150));
        testResult.setBounds(r);
        r = row();
        modelName.setBounds(r.removeFromLeft(nameW));
        model.setBounds(r);
        r = row();
        effortName.setBounds(r.removeFromLeft(nameW));
        effort.setBounds(r.removeFromLeft(260));
        r = row(26);
        storeName.setBounds(r.removeFromLeft(nameW));
        keychain.setBounds(r);
        r = row(26);
        r.removeFromLeft(nameW);
        file.setBounds(r);
        r = row();
        limitName.setBounds(r.removeFromLeft(nameW));
        limit.setBounds(r.removeFromLeft(80));
        r.removeFromLeft(8);
        limitNote.setBounds(r);
        auto buttons = area.removeFromBottom(32);
        cancel.setBounds(buttons.removeFromRight(90));
        buttons.removeFromRight(8);
        ok.setBounds(buttons.removeFromRight(90));
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::white); }

private:
    void runTest() {
        lmw::assistant::Settings s;
        s.key = key.getText().trim().toStdString();
        testResult.setText("testing...", juce::dontSendNotification);
        test.setEnabled(false);
        juce::Thread::launch([s, safe = juce::Component::SafePointer<AssistantSettings>(this)] {
            const auto why = AssistantSession::testConnection(s);
            juce::MessageManager::callAsync([safe, why] {
                if (safe == nullptr) return;
                safe->test.setEnabled(true);
                safe->testResult.setText(why.empty() ? juce::String("the key works") : juce::String::fromUTF8(why.c_str()),
                                         juce::dontSendNotification);
            });
        });
    }

    Values values;
    juce::Label providerName, keyName, testResult, modelName, effortName, storeName, limitName, limitNote;
    juce::ComboBox provider, model, effort;
    juce::TextEditor key, limit;
    juce::TextButton paste, clear, test, howTo, ok, cancel;
    juce::ToggleButton keychain, file;
};
