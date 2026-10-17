// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PluginProcessor.h"

#include "PluginEditor.h"

MzPcdProcessor::MzPcdProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "MzPCD", params::createLayout()),
      reader(state)
{
    // the oversampling's delay, so a host can line the track up with the others
    setLatencySamples(pcd::Pcd::latency());
}

void MzPcdProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    engine.setSettings(reader.read());
}

bool MzPcdProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void MzPcdProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, n);

    engine.setSettings(reader.read());

    // Where the host's timeline is at the top of this block. The engine runs it forward
    // sample by sample from there, and is put right again every block - so a loop jump or a
    // tempo change is followed within one block. No timeline, or stopped: nothing is struck,
    // and the knobs alone set the distortion.
    {
        bool playing = false;
        double ppq = 0.0, bpm = 120.0;

        if (auto* head = getPlayHead())
            if (const auto pos = head->getPosition())
                if (const auto p = pos->getPpqPosition())
                {
                    playing = pos->getIsPlaying();
                    ppq = *p;
                    if (const auto b = pos->getBpm()) bpm = *b;
                }

        engine.setHostClock(playing, ppq, bpm);
    }

    if (buffer.getNumChannels() == 0) return;

    engine.process(buffer.getWritePointer(0),
                   buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr, n);
}

juce::AudioProcessorEditor* MzPcdProcessor::createEditor()
{
    return new MzPcdEditor(*this);
}

void MzPcdProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void MzPcdProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MzPcdProcessor();
}
