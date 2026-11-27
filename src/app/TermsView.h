// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.10 (Phase 27b): the Terms window's content — a Find field, "N of M terms", Show all, and below the
// terms (by topic while Find is empty, else the Find hits with their topic), each with its text, "Where: ..."
// and a See also button per related term. Black on white. The terms come from docs/Terms.md, built in.
#pragma once
#include "ui/Terms.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <string>
#include <vector>

class TermsView final : public juce::Component {
public:
    TermsView();
    ~TermsView() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void lookAndFeelChanged() override;                   // Spec S6.29: the Find placeholder in the new theme's grey

    // See also: empty Find, show the list by topic and scroll to `name`, marked "> ". False if unknown.
    bool jumpTo(const std::string& name);
    void showAll();                                  // empty Find, no mark, top of the list
    void setFind(const juce::String& text);          // as if typed
    int shownCount() const { return static_cast<int>(shown.size()); }
    const std::string& marked() const { return mark; }
    juce::TextEditor& findField() { return find; }

private:
    class Card;
    class Content;
    void rebuild();                                  // from the Find text and the mark
    void layoutContent();
    void updateCount();
    void ensureVisible(juce::Component& c);

    std::vector<lmw::terms::Term> terms;
    std::vector<const lmw::terms::Term*> shown;
    std::string mark;
    bool findMode = false;

    juce::TextEditor find;
    juce::Label findLabel{{}, "Find:"}, count, none;
    std::unique_ptr<juce::TextButton> allButton;      // a button with a focus ring (TermsView.cpp)
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
};
