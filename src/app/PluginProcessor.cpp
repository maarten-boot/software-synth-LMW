// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

LMWSynthProcessor::LMWSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

void LMWSynthProcessor::prepareToPlay(double, int) {}

bool LMWSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void LMWSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* LMWSynthProcessor::createEditor() { return new LMWSynthEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LMWSynthProcessor(); }
