// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Look.h"

juce::Font fonts::digits(float size)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
}

Look::Look()
{
    setColour(juce::BubbleComponent::backgroundColourId, colours::rail);
    setColour(juce::BubbleComponent::outlineColourId, colours::screw);
    setColour(juce::TooltipWindow::textColourId, colours::light);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff111214));
    setColour(juce::TooltipWindow::outlineColourId, colours::stepRed);
    setColour(juce::Slider::textBoxTextColourId, colours::light);
}

void Look::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;
    const auto c = bounds.getCentre();

    // the scale: eleven marks
    g.setColour(s.findColour(juce::Slider::rotarySliderOutlineColourId));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
        const bool major = i % 5 == 0;
        g.drawLine({ c + dir * (radius + 1.5f), c + dir * (radius + (major ? 5.0f : 3.5f)) }, major ? 1.3f : 0.8f);
    }

    // a skirt, then the cap, lit from above
    const auto cap = s.findColour(juce::Slider::rotarySliderFillColourId);
    const auto skirt = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c);
    g.setColour(cap.darker(0.7f));
    g.fillEllipse(skirt);

    const auto top = skirt.reduced(radius * 0.18f);
    g.setGradientFill(juce::ColourGradient(cap.brighter(0.25f), top.getCentreX(), top.getY(),
                                           cap.darker(0.35f), top.getCentreX(), top.getBottom(), false));
    g.fillEllipse(top);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(top, 0.8f);

    // the pointer
    const float a = startAngle + (endAngle - startAngle) * sliderPos;
    const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
    g.setColour(s.findColour(juce::Slider::thumbColourId));
    g.drawLine({ c + dir * (radius * 0.2f), c + dir * (radius * 0.9f) }, 2.0f);
}

void drawFace(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setGradientFill(juce::ColourGradient(c.brighter(0.08f), r.getX(), r.getY(),
                                           c.darker(0.12f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(c.brighter(0.35f).withAlpha(0.6f));
    g.drawLine(r.getX() + 3.0f, r.getY() + 0.5f, r.getRight() - 3.0f, r.getY() + 0.5f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
}
