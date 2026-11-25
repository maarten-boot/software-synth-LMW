// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S7.10 (Phase 27b): the Terms window. See TermsView.h.
#include "TermsView.h"
#include "TermsText.h"   // generated from docs/Terms.md
#include <functional>

namespace {
constexpr int pad = 12, gap = 6, buttonH = 26;
const juce::Colour ink = juce::Colours::black, paper = juce::Colours::white;
const juce::Colour quiet{0xff4d4d4d};                 // topic after a Find hit: 8.4:1 on white

juce::String utf8(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }

// A button with a visible keyboard focus (a thick black ring); a See also button also tells the list when it
// gets the focus (Tab), so the list scrolls to it.
class FocusButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    std::function<void(juce::Component&)> onFocus;
    void focusGained(FocusChangeType) override { repaint(); if (onFocus) onFocus(*this); }
    void focusLost(FocusChangeType) override { repaint(); }
    void paintOverChildren(juce::Graphics& g) override {
        if (!hasKeyboardFocus(false)) return;
        g.setColour(ink);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 6.0f, 3.0f);
    }
};
using SeeAlsoButton = FocusButton;
} // namespace

// One term: name (bold; "> " when marked; its topic after it in Find), text, Where line, See also buttons.
class TermsView::Card final : public juce::Component {
public:
    Card(const lmw::terms::Term& t, bool withTopic, bool isMarked, TermsView& owner) : term(t) {
        juce::AttributedString a;
        a.setWordWrap(juce::AttributedString::byWord);
        a.append((isMarked ? "> " : "") + utf8(t.name), juce::FontOptions(18.0f, juce::Font::bold), ink);
        if (withTopic) a.append("   " + utf8(t.topic), juce::FontOptions(15.0f, juce::Font::italic), quiet);
        a.append("\n" + utf8(t.text), juce::FontOptions(16.0f), ink);
        if (!t.where.empty()) {
            a.append("\nWhere: ", juce::FontOptions(16.0f, juce::Font::bold), ink);
            a.append(utf8(t.where), juce::FontOptions(16.0f), ink);
        }
        text = a;
        setTitle(utf8(t.name));
        setDescription(utf8(t.text + (t.where.empty() ? "" : " Where: " + t.where)));
        if (!t.seeAlso.empty()) {
            seeLabel.setText("See also:", juce::dontSendNotification);
            seeLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
            seeLabel.setColour(juce::Label::textColourId, ink);
            addAndMakeVisible(seeLabel);
        }
        for (const auto& name : t.seeAlso) {
            auto b = std::make_unique<SeeAlsoButton>(utf8(name));
            b->setTooltip("Show the term " + utf8(name));
            b->onClick = [&owner, name] { owner.jumpTo(name); };
            b->onFocus = [&owner](juce::Component& c) { owner.ensureVisible(c); };
            addAndMakeVisible(*b);
            buttons.push_back(std::move(b));
        }
    }
    // Lays out for `width`; returns the height.
    int layout(int width) {
        const int inner = std::max(100, width - 2 * pad);
        juce::TextLayout l;
        l.createLayout(text, static_cast<float>(inner));
        textH = static_cast<int>(std::ceil(l.getHeight()));
        int x = pad, y = pad + textH + gap;
        if (!buttons.empty()) {
            const int lw = 76;
            seeLabel.setBounds(x, y, lw, buttonH);
            x += lw;
            for (auto& b : buttons) {
                const int bw = b->getBestWidthForHeight(buttonH) + 8;
                if (x + bw > pad + inner && x > pad + lw) { x = pad + lw; y += buttonH + 4; }
                b->setBounds(x, y, bw, buttonH);
                x += bw + 6;
            }
            y += buttonH;
        }
        return y + pad;
    }
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xffd0d0d0));
        g.drawHorizontalLine(getHeight() - 1, static_cast<float>(pad), static_cast<float>(getWidth() - pad));
        text.draw(g, juce::Rectangle<float>(static_cast<float>(pad), static_cast<float>(pad),
                                            static_cast<float>(getWidth() - 2 * pad), static_cast<float>(textH + 4)));
    }
    const lmw::terms::Term& term;
    juce::Component* firstButton() { return buttons.empty() ? nullptr : buttons.front().get(); }

private:
    juce::AttributedString text;
    int textH = 0;
    juce::Label seeLabel;
    std::vector<std::unique_ptr<SeeAlsoButton>> buttons;
};

// The list: topic headings (list mode only) and the cards.
class TermsView::Content final : public juce::Component {
public:
    std::vector<std::unique_ptr<juce::Label>> topics;
    std::vector<std::unique_ptr<Card>> cards;
    std::vector<juce::Component*> order;              // topics and cards, top to bottom
    void layout(int width) {
        int y = 0;
        for (auto* c : order) {
            if (auto* card = dynamic_cast<Card*>(c)) {
                const int h = card->layout(width);
                card->setBounds(0, y, width, h);
                y += h;
            } else {
                c->setBounds(pad, y + 8, width - 2 * pad, 30);
                y += 40;
            }
        }
        setSize(width, std::max(y, 1));
    }
    Card* cardOf(const std::string& name) {
        for (auto& c : cards) if (c->term.name == name) return c.get();
        return nullptr;
    }
    void paint(juce::Graphics& g) override { g.fillAll(paper); }
};

TermsView::TermsView() : content(std::make_unique<Content>()) {
    std::string md;
    for (const char* part : lmw::terms::textParts) md += part;
    terms = lmw::terms::parse(md);

    findLabel.setColour(juce::Label::textColourId, ink);
    findLabel.setFont(juce::FontOptions(16.0f));
    addAndMakeVisible(findLabel);
    find.setTitle("Find a term");
    find.setTextToShowWhenEmpty("a word, for example cutoff", juce::Colour(0xff595959));   // 7:1 on white
    find.setFont(juce::FontOptions(16.0f));
    find.setColour(juce::TextEditor::backgroundColourId, paper);
    find.setColour(juce::TextEditor::textColourId, ink);
    find.setColour(juce::TextEditor::outlineColourId, ink);
    find.setColour(juce::TextEditor::focusedOutlineColourId, ink);
    find.setEscapeAndReturnKeysConsumed(false);      // Escape closes the window
    find.onTextChange = [this] { mark.clear(); rebuild(); viewport.setViewPosition(0, 0); };
    addAndMakeVisible(find);
    for (auto* l : {&count, &none}) {
        l->setColour(juce::Label::textColourId, ink);
        l->setFont(juce::FontOptions(16.0f));
    }
    addAndMakeVisible(count);
    allButton = std::make_unique<FocusButton>("Show all");
    allButton->setTooltip("Empty Find and show every term by topic");
    allButton->onClick = [this] { showAll(); };
    addAndMakeVisible(*allButton);
    viewport.setViewedComponent(content.get(), false);
    viewport.setScrollBarsShown(true, false);
    viewport.setWantsKeyboardFocus(false);
    addAndMakeVisible(viewport);
    addChildComponent(none);                          // over the (empty) list
    setTitle("Terms");
    setSize(760, 640);
    rebuild();
}

TermsView::~TermsView() = default;

void TermsView::paint(juce::Graphics& g) {
    g.fillAll(paper);
    g.setColour(ink);
    g.drawHorizontalLine(viewport.getY() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TermsView::resized() {
    auto r = getLocalBounds().reduced(pad);
    auto top = r.removeFromTop(30);
    findLabel.setBounds(top.removeFromLeft(48));
    allButton->setBounds(top.removeFromRight(96));
    top.removeFromRight(gap);
    count.setBounds(top.removeFromRight(130));
    top.removeFromRight(gap);
    find.setBounds(top);
    r.removeFromTop(gap);
    none.setBounds(r.removeFromTop(0).withHeight(30).translated(0, 8));
    viewport.setBounds(getLocalBounds().withTrimmedTop(pad + 30 + gap));
    layoutContent();
}

void TermsView::layoutContent() {
    content->layout(viewport.getMaximumVisibleWidth());
}

void TermsView::updateCount() {
    const auto total = static_cast<int>(terms.size());
    count.setText(juce::String(findMode ? static_cast<int>(shown.size()) : total) + " of " + juce::String(total) + " terms",
                  juce::dontSendNotification);
    none.setVisible(findMode && shown.empty());
    if (none.isVisible()) none.setText("No term has \"" + find.getText().trim() + "\"", juce::dontSendNotification);
}

void TermsView::rebuild() {
    const auto want = find.getText().toStdString();
    findMode = !juce::String(want).trim().isEmpty();
    shown.clear();
    if (findMode) shown = lmw::terms::find(terms, want);
    else for (const auto& t : terms) shown.push_back(&t);

    content->order.clear();
    content->cards.clear();
    content->topics.clear();
    std::string topic;
    for (const auto* t : shown) {
        if (!findMode && t->topic != topic) {
            topic = t->topic;
            auto l = std::make_unique<juce::Label>(juce::String(), utf8(topic));
            l->setFont(juce::FontOptions(22.0f, juce::Font::bold));
            l->setColour(juce::Label::textColourId, ink);
            content->addAndMakeVisible(*l);
            content->order.push_back(l.get());
            content->topics.push_back(std::move(l));
        }
        auto c = std::make_unique<Card>(*t, findMode, !mark.empty() && t->name == mark, *this);
        content->addAndMakeVisible(*c);
        content->order.push_back(c.get());
        content->cards.push_back(std::move(c));
    }
    updateCount();
    layoutContent();
}

bool TermsView::jumpTo(const std::string& name) {
    if (lmw::terms::byName(terms, name) == nullptr) return false;
    // Called from a See also button that rebuild() deletes: do the work after the click has returned.
    juce::Component::SafePointer<TermsView> safe(this);
    juce::MessageManager::callAsync([safe, name] {
        if (safe == nullptr) return;
        safe->mark = name;
        safe->find.setText({}, false);
        safe->rebuild();
        auto* c = safe->content->cardOf(name);
        if (c != nullptr) safe->viewport.setViewPosition(0, c->getY());
        // Keyboard: on to the jumped-to term's first See also button (Tab goes on through its others).
        if (auto* b = c != nullptr ? c->firstButton() : nullptr) b->grabKeyboardFocus();
        else safe->find.grabKeyboardFocus();
    });
    return true;
}

void TermsView::showAll() {
    mark.clear();
    find.setText({}, false);
    rebuild();
    viewport.setViewPosition(0, 0);
}

void TermsView::setFind(const juce::String& text) {
    find.setText(text, true);                         // onTextChange rebuilds
}

void TermsView::ensureVisible(juce::Component& c) {
    const auto area = content->getLocalArea(&c, c.getLocalBounds());
    const auto view = viewport.getViewArea();
    if (area.getY() < view.getY()) viewport.setViewPosition(0, area.getY() - pad);
    else if (area.getBottom() > view.getBottom()) viewport.setViewPosition(0, area.getBottom() - view.getHeight() + pad);
}
