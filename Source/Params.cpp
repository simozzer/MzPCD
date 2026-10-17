// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Params.h"

#include <cmath>

namespace params
{

using S = pcd::Settings;

const std::vector<Spec>& specs()
{
    static const std::vector<Spec> table = {
        { "pattern",  "Pattern",  "PATTERN", "Which of the 32 patterns plays the distortion - the PCF's: on each step it pushes the drive, the tone and the feedback, by that step's level. Drag up or down, or use the wheel.",
          nullptr, &S::pattern, {}, 1.0f, 32.0f },
        { "rate",     "Rate",     "RATE",    "How long a step is: an eighth, a sixteenth or a thirty-second note. The pattern's sixteen steps run that fast.",
          nullptr, &S::rate, { "1/8", "1/16", "1/32" } },

        { "drive",    "Drive",    "DRIVE",   "How hard the sound is driven into the clipper, 0 to 36 dB. The level is made up as it goes, so it changes the colour more than the volume.",
          &S::drive, nullptr, {}, 0.0f, 1.0f, Unit::Drive },
        { "tone",     "Tone",     "TONE",    "Dark to bright: the cutoff after the clipper, 500 Hz to 12 kHz. It also sets the pitch the feedback howls at.",
          &S::tone, nullptr, {}, 0.0f, 1.0f, Unit::Tone },
        { "feedback", "Feedback", "FDBK",    "How much of the output comes round again, as a guitar amp's does through the speaker and back into the pickup. Up high it sustains, then howls - but never runs away.",
          &S::feedback },

        { "driveAmount",    "Pattern Drive",    "DRIVE", "How far a full-level step pushes the drive up, on top of DRIVE: up to 36 dB more.",
          &S::driveAmount, nullptr, {}, 0.0f, 1.0f, Unit::Push },
        { "toneAmount",     "Pattern Tone",     "TONE",  "How far a full-level step moves the tone: up to three octaves brighter, or - turned left - darker.",
          &S::toneAmount, nullptr, {}, -1.0f, 1.0f, Unit::Bipolar },
        { "feedbackAmount", "Pattern Feedback", "FDBK",  "How far a full-level step pushes the feedback up, on top of FDBK - so the howl swells on the steps the pattern strikes.",
          &S::feedbackAmount },
        { "decay",          "Pattern Decay",    "DEC",   "How fast each step's push dies away, 15 ms to 1.5 s.",
          &S::decay, nullptr, {}, 0.0f, 1.0f, Unit::Decay },

        { "mix",      "Mix",      "MIX",     "How much of the sound goes through the distortion: 0% is dry, 100% all of it. The dry side is delayed to line up with the wet.",
          &S::mix, nullptr, {}, 0.0f, 1.0f, Unit::Percent },
        { "level",    "Level",    "LEVEL",   "Output level, -24 to +12 dB.",
          &S::level, nullptr, {}, -24.0f, 12.0f, Unit::Decibels },
    };

    return table;
}

const Spec& spec(const juce::String& id)
{
    for (const auto& p : specs())
        if (id == p.id) return p;
    jassertfalse;
    return specs().front();
}

namespace
{
    juce::String text(Unit unit, float v)
    {
        switch (unit)
        {
            case Unit::Decibels: return (v > 0.0f ? "+" : "") + juce::String(v, 1) + " dB";
            case Unit::Percent:  return juce::String(juce::roundToInt(v * 100.0f)) + "%";
            case Unit::Drive:    return juce::String(v * 36.0f, 1) + " dB";
            case Unit::Push:     return "+" + juce::String(v * 36.0f, 1) + " dB";
            case Unit::Tone:
            {
                const double hz = 500.0 * std::pow(24.0, (double) v);
                return hz < 1000.0 ? juce::String(juce::roundToInt(hz)) + " Hz" : juce::String(hz / 1000.0, 1) + " kHz";
            }
            case Unit::Bipolar:
            {
                const int p = juce::roundToInt(v * 100.0f);
                return (p > 0 ? "+" : "") + juce::String(p);
            }
            case Unit::Decay:
            {
                const double s = 0.015 * std::pow(100.0, (double) v);
                return s < 1.0 ? juce::String(juce::roundToInt(s * 1000.0)) + " ms" : juce::String(s, 2) + " s";
            }
            default: return juce::String(juce::roundToInt(v * 100.0f));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        const juce::ParameterID id { p.id, 1 };

        if (p.f != nullptr)
        {
            const Unit unit = p.unit;
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(p.min, p.max, 0.0f), defaults.*(p.f),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [unit] (float v, int) { return text(unit, v); })));
        }
        else if (! p.choices.isEmpty())
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, p.name, p.choices, defaults.*(p.i)));
        }
        else
        {
            // a number, shown from min - the pattern, 1 to 32, held as 0 to 31
            layout.add(std::make_unique<juce::AudioParameterInt>(
                id, p.name, (int) p.min, (int) p.max, defaults.*(p.i) + (int) p.min));
        }
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

pcd::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
    {
        const float v = values[n]->load(std::memory_order_relaxed);
        const auto& p = table[n];

        if (p.f != nullptr)                s.*(p.f) = v;
        else if (! p.choices.isEmpty())    s.*(p.i) = juce::roundToInt(v);
        else                               s.*(p.i) = juce::roundToInt(v) - (int) p.min;
    }

    return s;
}

} // namespace params
