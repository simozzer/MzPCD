// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Params.h"
#include "PCD/Pcd.h"

class MzPcdProcessor final : public juce::AudioProcessor
{
public:
    MzPcdProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }   // the feedback rings on

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // any thread: the step last struck, for the panel's lights; -1 stopped
    int step() const { return engine.step(); }

    // any thread: the envelope as it stands, for the panel
    float envelope() const { return engine.envelope(); }

private:
    params::Reader reader;
    pcd::Pcd engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MzPcdProcessor)
};
