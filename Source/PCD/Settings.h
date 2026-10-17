// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

namespace pcd
{

namespace Rate { enum { Eighth, Sixteenth, ThirtySecond }; }

// the steps to a beat at each RATE
inline int stepsPerQuarter(int rate) { return rate == Rate::Eighth ? 2 : rate == Rate::ThirtySecond ? 8 : 4; }

//
// The front panel, in panel units: 0 to 1 unless it says otherwise.
//
// DRIVE, TONE and FEEDBACK set where each sits; the pattern pushes each of them from there
// by its own amount, on every step it strikes.
//
struct Settings
{
    int   pattern  = 0;              // 0 to 31, MzPCF's patterns
    int   rate     = Rate::Sixteenth;

    float drive    = 0.35f;          // 0 to 36 dB into the clipper
    float tone     = 0.5f;           // dark to bright: the cutoff after it, 500 Hz to 12 kHz
    float feedback = 0.0f;           // how much of the output comes round again, as an amp's does

    float driveAmount    = 0.5f;     // how far a full step pushes the drive up, 0 to 36 dB
    float toneAmount     = 0.0f;     // -1 to +1: how far it moves the tone, down or up
    float feedbackAmount = 0.0f;     // how far it pushes the feedback up
    float decay          = 0.35f;    // how fast a step's push dies away, 15 ms to 1.5 s

    float mix   = 1.0f;              // 0 dry to 1 all through the distortion
    float level = 0.0f;              // output, dB, -24 to +12
};

} // namespace pcd
