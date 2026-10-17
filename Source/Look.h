// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//
// The look of the rack's effects units (MzDebirth), so the PCD looks as the PCF does.
//
namespace colours
{
    const juce::Colour rail      { 0xff26272b };
    const juce::Colour screw     { 0xff8d8f93 };
    const juce::Colour mixer     { 0xff3c4f63 };
    const juce::Colour mixerEdge { 0xff22303e };
    const juce::Colour display   { 0xff160606 };
    const juce::Colour ledRed    { 0xffff3b22 };
    const juce::Colour ledOff    { 0xff4a1712 };
    const juce::Colour pad       { 0xff1b1b1d };
    const juce::Colour light     { 0xffe9e9e6 };
    const juce::Colour knobCap   { 0xff15181d };
    const juce::Colour pointer   { 0xffef7d22 };
    const juce::Colour stepRed   { 0xffd8321f };
}

namespace fonts
{
    inline juce::Font bold(float size)  { return juce::FontOptions(size, juce::Font::bold); }
    juce::Font digits(float size);
}

//
// Knobs take their cap and pointer from the slider's own colours:
//
//   rotarySliderFillColourId     the cap
//   thumbColourId                the pointer
//   rotarySliderOutlineColourId  the scale round it
//
class Look final : public juce::LookAndFeel_V4
{
public:
    Look();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

// A panel's bevelled face.
void drawFace(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
