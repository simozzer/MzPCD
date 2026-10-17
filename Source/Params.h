// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PCD/Settings.h"

#include <vector>

namespace params
{

// how a knob's value reads
enum class Unit { Plain, Percent, Decibels, Drive, Push, Tone, Bipolar, Decay };

//
// One row per front-panel control: its parameter ID, the name a host shows, and the field
// of pcd::Settings it drives. The layout, the reader and the panel all work from this, so a
// control cannot exist in one and be missing from another.
//
// IDs are saved in every host project. Never rename one.
//
struct Spec
{
    const char* id;
    const char* name;
    const char* legend;                    // as printed on the panel
    const char* hint;                      // what it does, for the hover hint
    float pcd::Settings::* f = nullptr;    // a knob, min to max
    int   pcd::Settings::* i = nullptr;    // a switch (with choices), or a number, min to max
    juce::StringArray choices = {};
    float min = 0.0f, max = 1.0f;
    Unit unit = Unit::Plain;
};

const std::vector<Spec>& specs();
const Spec& spec(const juce::String& id);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Reads every parameter into Settings, from the audio thread, without a lookup by name.
class Reader
{
public:
    explicit Reader(juce::AudioProcessorValueTreeState& state);
    pcd::Settings read() const;

private:
    std::vector<std::atomic<float>*> values;
};

} // namespace params
