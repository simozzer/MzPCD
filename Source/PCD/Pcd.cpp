// MzPCD - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Pcd.h"

#include <algorithm>
#include <cmath>

namespace pcd
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    constexpr double toneLow = 500.0, toneHigh = 12000.0;   // TONE's travel
    constexpr double maxDriveDb = 36.0;
    constexpr double maxLoopGain = 1.2;                       // FEEDBACK at full
    constexpr double loopSeconds = 0.0025;                    // the speaker to the pickup, about a metre
    constexpr double resonanceQ = 4.0;

    // A soft clipper that leans a little one way, as a valve stage does, for the even
    // harmonics; the DC that makes is taken out after.
    double shaper(double u)
    {
        constexpr double bias = 0.2;
        return std::tanh(u + bias) - std::tanh(bias);
    }

    // A trapezoidal state-variable filter's coefficients, at a frequency and damping.
    struct Svf
    {
        double a1, a2, a3;
        Svf(double hz, double fs, double k)
        {
            const double g = std::tan(pi * std::min(hz, fs * 0.45) / fs);
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        // one sample through: returns the band output, and the low-pass in `low`
        double run(double x, double& ic1, double& ic2, double& low) const
        {
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            low = v2;
            return v1;
        }
    };
}

double Pcd::howlFrequency(double toneHz)
{
    // a quarter of the tone's cutoff, where a speaker cabinet sings: from 150 Hz to 2.5 kHz
    return std::clamp(toneHz * 0.25, 150.0, 2500.0);
}

void Pcd::prepare(double sampleRate)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    clock.prepare(fs);
    env.prepare(fs);
    smoothCoef = 1.0 - std::exp(-1.0 / (0.010 * fs));
    dcCoef = 1.0 - 2.0 * pi * 5.0 / fs;

    loopLength = std::max(1, (int) std::lround(loopSeconds * fs));
    for (auto& c : channels) c.loop.assign((size_t) loopLength, 0.0f);
    reset();
}

void Pcd::reset()
{
    clock.reset();
    env.reset();
    lastStruck = -1;
    settled = false;
    for (auto& c : channels)
    {
        c.os.reset();
        c.dcX = c.dcY = c.lp1 = c.lp2 = c.bp1 = c.bp2 = c.comingBack = 0.0;
        std::fill(c.loop.begin(), c.loop.end(), 0.0f);
        c.dry.fill(0.0f);
        c.loopPos = c.dryPos = 0;
    }
}

void Pcd::setHostClock(bool isPlaying, double ppq, double bpm)
{
    clock.set(isPlaying, ppq, bpm, stepsPerQuarter(target.rate));
    if (! clock.isPlaying()) lastStruck = -1;
}

void Pcd::process(float* left, float* right, int n)
{
    const Settings& s = target;
    env.setDecay(s.decay);

    // where the knobs are heading
    const double driveT = std::clamp(s.drive, 0.0f, 1.0f) * maxDriveDb;
    const double toneT = std::log2(toneLow) + std::clamp(s.tone, 0.0f, 1.0f) * std::log2(toneHigh / toneLow);
    const double feedbackT = std::clamp(s.feedback, 0.0f, 1.0f);
    const double wetT = std::clamp(s.mix, 0.0f, 1.0f);
    const double outT = std::pow(10.0, std::clamp(s.level, -24.0f, 12.0f) / 20.0);
    if (! settled)
    {
        drive = driveT; toneOct = toneT; feedback = feedbackT; wet = wetT; out = outT;
        settled = true;
    }

    const double driveAmount = std::clamp(s.driveAmount, 0.0f, 1.0f) * maxDriveDb;
    const double toneAmount = std::clamp(s.toneAmount, -1.0f, 1.0f) * 3.0;   // octaves
    const double feedbackAmount = std::clamp(s.feedbackAmount, 0.0f, 1.0f);

    float* sides[2] = { left, right };
    float e = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        long long st = 0;
        if (clock.stepAt(i, st))
        {
            const int k = (int) (((st % steps) + steps) % steps);
            env.strike(level(s.pattern, k));
            lastStruck.store(k, std::memory_order_relaxed);
        }
        e = env.next();

        drive    += (driveT - drive) * smoothCoef;
        toneOct  += (toneT - toneOct) * smoothCoef;
        feedback += (feedbackT - feedback) * smoothCoef;
        wet      += (wetT - wet) * smoothCoef;
        out      += (outT - out) * smoothCoef;

        // the pattern's push on each, on top of the knob
        const double gain = std::pow(10.0, std::min(maxDriveDb + 12.0, drive + driveAmount * e) / 20.0);
        const double toneHz = std::exp2(std::clamp(toneOct + toneAmount * e, std::log2(200.0), std::log2(16000.0)));
        const double loopGain = std::clamp(feedback + feedbackAmount * e, 0.0, 1.0) * maxLoopGain;

        // Level made up as the drive goes up, so DRIVE changes the colour more than the volume:
        // a sound at an ordinary level, about -18 dB, comes out about as loud as it went in.
        constexpr double reference = 0.12;
        const double makeup = reference / std::max(1.0e-4, std::fabs(shaper(reference * gain)));

        const Svf tone(toneHz, fs, 1.4142);
        const Svf speaker(howlFrequency(toneHz), fs, 1.0 / resonanceQ);

        for (int c = 0; c < 2; ++c)
        {
            if (sides[c] == nullptr) continue;
            Channel& ch = channels[c];
            const float x = sides[c][i];

            // the clipper, at twice the rate, with what came back round the loop
            double a, b;
            ch.os.upsample((double) x + ch.comingBack, a, b);
            const double clipped = ch.os.downsample(shaper(a * gain), shaper(b * gain));

            // the DC the lean makes, out; then the tone
            const double dc = clipped - ch.dcX + dcCoef * ch.dcY;
            ch.dcX = clipped;
            ch.dcY = dc;
            double low = 0.0;
            tone.run(dc, ch.lp1, ch.lp2, low);
            const double wetSide = low * makeup;

            // round the loop: the output, a moment later, through the speaker's resonance
            const double heard = ch.loop[(size_t) ch.loopPos];
            ch.loop[(size_t) ch.loopPos] = (float) wetSide;
            if (++ch.loopPos >= loopLength) ch.loopPos = 0;
            double unused = 0.0;
            const double sung = speaker.run(heard, ch.bp1, ch.bp2, unused) / resonanceQ;   // unity at the peak
            ch.comingBack = sung * loopGain;

            // the dry side, held back by the oversampling's delay, to meet the wet
            const float dry = ch.dry[(size_t) ch.dryPos];
            ch.dry[(size_t) ch.dryPos] = x;
            if (++ch.dryPos >= (int) ch.dry.size()) ch.dryPos = 0;

            sides[c][i] = (float) ((dry * (1.0 - wet) + wetSide * wet) * out);
        }
    }

    envNow.store(e, std::memory_order_relaxed);
}

} // namespace pcd
