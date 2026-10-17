// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Oversampler.h"
#include "Settings.h"

#include "PCF/Pcf.h"   // MzPCF's patterns, envelope and clock, compiled from its repository
#include "PCF/StepClock.h"

#include <array>
#include <atomic>
#include <vector>

namespace pcd
{

constexpr int patterns = pcf::patterns;
constexpr int steps = pcf::steps;

// A pattern's level on a step, 0 to 1: the PCF's, so a pattern number means the same in both.
inline float level(int pattern, int step) { return pcf::level(pattern, step); }

//
// The pattern-controlled distortion. A clipper, run at twice the sample rate so its
// harmonics do not alias, with a tone control after it and a feedback loop round it - and
// the PCF's idea playing all three: on every step of the host's timeline an envelope is
// struck to that step's level in the pattern, and pushes the drive, the tone and the
// feedback each by its own amount, dying away by DECAY.
//
// The feedback is an amplifier's: the output comes back round a short acoustic path and a
// speaker's resonance into the input. Turned up it sustains, and past a point it howls, at
// a pitch that follows TONE. The clipper bounds the loop, as the amp's does, so it can never
// run away.
//
class Pcd
{
public:
    void prepare(double sampleRate);
    void reset();

    void setSettings(const Settings& s) { target = s; }

    // Where the host's timeline is at the top of the next block.
    void setHostClock(bool playing, double ppq, double bpm);

    // In place. right may be null, for a mono channel.
    void process(float* left, float* right, int n);

    // the delay the oversampling makes, in samples; the dry signal is delayed to match it
    static constexpr int latency() { return Oversampler::latency(); }

    // any thread: the step last struck, 0 to 15, for the panel's lights; -1 stopped
    int step() const { return lastStruck.load(std::memory_order_relaxed); }

    // the envelope as it stands, 0 to 1 - for the panel, and the checks
    float envelope() const { return envNow.load(std::memory_order_relaxed); }

    // the frequency the feedback howls at, for a TONE, in Hz
    static double howlFrequency(double toneHz);

private:
    struct Channel
    {
        Oversampler os;
        double dcX = 0.0, dcY = 0.0;
        double lp1 = 0.0, lp2 = 0.0;       // the tone: a two-pole low-pass
        double bp1 = 0.0, bp2 = 0.0;       // the speaker's resonance, in the loop
        std::vector<float> loop;           // the acoustic path back to the input
        std::array<float, Oversampler::latency()> dry {};   // a ring as long as the delay it makes
        int loopPos = 0, dryPos = 0;
        double comingBack = 0.0;           // what the loop brings to the next sample
    };

    double fs = 48000.0;
    Settings target;
    pcf::StepClock clock;
    pcf::Envelope env;
    std::atomic<int> lastStruck { -1 };
    std::atomic<float> envNow { 0.0f };

    Channel channels[2];
    int loopLength = 1;
    double dcCoef = 0.9993;

    // the knobs, smoothed, so turning one does not zip; the envelope's push is added after
    double drive = 0.0, toneOct = 0.0, feedback = 0.0, wet = 1.0, out = 1.0;
    double smoothCoef = 0.0;
    bool settled = false;
};

} // namespace pcd
