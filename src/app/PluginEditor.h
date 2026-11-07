// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class LMWSynthEditor final : public juce::AudioProcessorEditor {
public:
    explicit LMWSynthEditor(LMWSynthProcessor&);
    void paint(juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LMWSynthEditor)
};
