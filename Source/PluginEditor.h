// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Look.h"
#include "PluginProcessor.h"

//
// A knob with its legend above it, on a parameter.
//
class Knob final : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState&, const params::Spec&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String legend;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//
// A red seven-segment style readout of a whole-number parameter, set by dragging up and
// down or the mouse wheel; double-click for its default.
//
class LedNumber final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedNumber(juce::RangedAudioParameter&, int digits);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int value() const;

private:
    void put(int v);

    juce::RangedAudioParameter& param;
    int digits, dragFrom = 0;
    float wheel = 0.0f;
};

//
// A switch, as a pad that steps through its positions, named on it.
//
class Switch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Switch(juce::RangedAudioParameter&, const juce::String& legend);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String legend;
    bool pressed = false;
};

//
// The pattern, drawn: a bar a step, as tall as how hard the step pushes, the step playing
// lit - and a meter at the side of the push as it is now.
//
class PatternDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    PatternDisplay();

    void show(int pattern, int step, float push);
    void paint(juce::Graphics&) override;

private:
    int pattern = 0, step = -1;
    float push = 0.0f;
};

class MzPcdEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MzPcdEditor(MzPcdProcessor&);
    ~MzPcdEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MzPcdProcessor& processor;
    Look look;
    juce::TooltipWindow tooltips { this, 600 };

    LedNumber pattern;
    PatternDisplay display;
    Switch rate;
    std::vector<std::unique_ptr<Knob>> levelKnobs, pushKnobs, outputKnobs;

    juce::Rectangle<int> face, pcdArea, outArea, levelsLabel, pushesLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MzPcdEditor)
};
