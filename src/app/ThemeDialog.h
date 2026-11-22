// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S6.13 (Phase 21d): the "Edit colours..." dialog — one row per theme item (ui/Theme.h themeItems):
// its name, a swatch (click: a colour picker), the contrast it gets against black text (or, for the cable,
// against white) at the current contrast setting, and Reset (back to the Default theme's colour). Black on
// white like every dialog (S6.2). Every change is reported through onChange(item, colour or nothing).
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/Config.h"
#include "ui/Theme.h"
#include <functional>
#include <optional>
#include <vector>

class ThemeDialog final : public juce::Component {
public:
    std::function<void(std::size_t item, std::optional<lmw::ui::Rgb> colour)> onChange;   // nothing = reset
    std::function<void(float opacity)> onOpacity;          // cables at rest, 0.2 .. 1

    ThemeDialog(lmw::ui::theme::Colours current, double minRatio) : colours(current), ratio(minRatio) {
        for (std::size_t i = 0; i < lmw::ui::theme::themeItems.size(); ++i) {
            auto& r = rows.emplace_back(std::make_unique<Row>());
            r->name.setText(juce::String(lmw::ui::theme::themeItems[i].label.data(), lmw::ui::theme::themeItems[i].label.size()),
                            juce::dontSendNotification);
            r->swatch.setTitle(r->name.getText() + " colour");
            r->swatch.setTooltip("Click to choose the colour");
            r->swatch.onClick = [this, i] { pick(i); };
            r->reset.setButtonText("Reset");
            r->reset.setTooltip("Back to the Default theme's colour");
            r->reset.onClick = [this, i] {
                lmw::ui::theme::itemColour(colours, i) = lmw::ui::theme::itemColour(lmw::ui::theme::Colours{}, i);
                if (onChange) onChange(i, std::nullopt);
                refresh();
            };
            for (juce::Component* c : {static_cast<juce::Component*>(&r->name), static_cast<juce::Component*>(&r->swatch),
                                       static_cast<juce::Component*>(&r->contrast), static_cast<juce::Component*>(&r->reset)})
                addAndMakeVisible(c);
        }
        opacityName.setText("Cables at rest (opacity)", juce::dontSendNotification);
        opacity.setSliderStyle(juce::Slider::LinearHorizontal);
        opacity.setRange(20.0, 100.0, 1.0);
        opacity.setTextValueSuffix(" %");
        opacity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
        opacity.setTitle("Cables at rest, opacity");
        opacity.setTooltip("How see-through cables at rest are: 100 % = solid. Raised when too faint for the contrast "
                           "setting; solid from AAA up.");
        opacity.setValue(colours.cableOpacity * 100.0, juce::dontSendNotification);
        opacity.onValueChange = [this] {
            colours.cableOpacity = static_cast<float>(opacity.getValue() / 100.0);
            if (onOpacity) onOpacity(colours.cableOpacity);
            refresh();
        };
        addAndMakeVisible(opacityName);
        addAndMakeVisible(opacity);
        addAndMakeVisible(opacityInfo);
        note.setText("Every colour is still raised to the contrast setting (Menu > Contrast); the contrast shown is "
                     "the one you see.", juce::dontSendNotification);
        addAndMakeVisible(note);
        refresh();
        setSize(680, 30 * static_cast<int>(rows.size() + 1) + 70);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(12);
        note.setBounds(area.removeFromBottom(40));
        for (auto& r : rows) {
            auto row = area.removeFromTop(30).reduced(0, 3);
            r->name.setBounds(row.removeFromLeft(220));
            r->swatch.setBounds(row.removeFromLeft(90));
            row.removeFromLeft(10);
            r->reset.setBounds(row.removeFromRight(70));
            r->contrast.setBounds(row);
        }
        auto row = area.removeFromTop(30).reduced(0, 3);
        opacityName.setBounds(row.removeFromLeft(220));
        opacity.setBounds(row.removeFromLeft(200));
        row.removeFromLeft(10);
        opacityInfo.setBounds(row);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::white); }

private:
    struct Swatch final : juce::Button {
        Swatch() : juce::Button("swatch") {}
        juce::Colour fill;
        void paintButton(juce::Graphics& g, bool highlighted, bool) override {
            const auto b = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(fill);
            g.fillRect(b);
            g.setColour(juce::Colours::black);
            g.drawRect(b, highlighted || hasKeyboardFocus(false) ? 3.0f : 1.0f);
        }
    };
    struct Row {
        juce::Label name, contrast;
        Swatch swatch;
        juce::TextButton reset;
    };
    // A colour picker that reports every change at once.
    struct Picker final : juce::Component, juce::ChangeListener {
        juce::ColourSelector selector{juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders
                                      | juce::ColourSelector::showColourspace};
        std::function<void(juce::Colour)> changed;
        explicit Picker(juce::Colour start) {
            selector.setCurrentColour(start, juce::dontSendNotification);
            selector.setColour(juce::ColourSelector::backgroundColourId, juce::Colours::white);
            selector.addChangeListener(this);
            addAndMakeVisible(selector);
            setSize(300, 300);
        }
        ~Picker() override { selector.removeChangeListener(this); }
        void resized() override { selector.setBounds(getLocalBounds()); }
        void changeListenerCallback(juce::ChangeBroadcaster*) override { if (changed) changed(selector.getCurrentColour()); }
    };

    void pick(std::size_t i) {
        const auto c = lmw::ui::theme::itemColour(colours, i);
        auto picker = std::make_unique<Picker>(juce::Colour(c.r, c.g, c.b));
        picker->changed = [safe = juce::Component::SafePointer<ThemeDialog>(this), i](juce::Colour col) {
            if (safe == nullptr) return;
            const lmw::ui::Rgb rgb{col.getRed(), col.getGreen(), col.getBlue()};
            lmw::ui::theme::itemColour(safe->colours, i) = rgb;
            if (safe->onChange) safe->onChange(i, rgb);
            safe->refresh();
        };
        juce::CallOutBox::launchAsynchronously(std::move(picker), rows[i]->swatch.getScreenBounds(), nullptr);
    }

    void refresh() {
        const auto pal = lmw::ui::theme::palette(ratio, colours);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& item = lmw::ui::theme::themeItems[i];
            const auto chosen = lmw::ui::theme::itemColour(colours, i);
            lmw::ui::theme::Colours shownAll;                 // the colours as drawn at this contrast
            shownAll.unit = pal.unit; shownAll.rack = pal.rack; shownAll.band = pal.band; shownAll.cable = pal.cable;
            const auto shown = lmw::ui::theme::itemColour(shownAll, i);
            rows[i]->swatch.fill = juce::Colour(chosen.r, chosen.g, chosen.b);
            rows[i]->swatch.repaint();
            const double r = item.textOnIt ? lmw::ui::contrastRatio(lmw::ui::theme::foreground, shown)
                                           : lmw::ui::contrastRatio(shown, lmw::ui::theme::background);
            const bool raised = item.textOnIt ? shown != chosen : pal.cableInk != chosen;
            rows[i]->contrast.setText(juce::String(lmw::ui::hexColour(chosen)) + "   " + juce::String(r, 1) + ":1"
                                          + (item.textOnIt ? " with black" : " on white, looks " + juce::String(lmw::ui::hexColour(shown)))
                                          + (raised ? " (adjusted)" : ""),
                                      juce::dontSendNotification);
        }
        const auto drawn = static_cast<double>(pal.cableAlpha()) * 100.0;
        opacityInfo.setText(drawn > colours.cableOpacity * 100.0 + 0.5
                                ? "drawn at " + juce::String(drawn, 0) + " %" + (ratio >= 7.0 ? " (solid from AAA)" : " (raised for contrast)")
                                : juce::String(), juce::dontSendNotification);
    }

    lmw::ui::theme::Colours colours;
    double ratio;
    juce::Label opacityName, opacityInfo;
    juce::Slider opacity;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Label note;
};
