// MzPCD - Created by Simon Moscrop
//
// THE REBIRTH DISTORTION, as OpenRB-338 decoded it: a C++ port of orb-engine's Distortion
// (crates/orb-engine/src/dist.rs) and the two tables it reads (tables/mod.rs: the level curve
// and the waveshaper). Copyright (C) 2026 the OpenRB-338 authors, and this port Simon Moscrop.
// This file is under the GNU GPL v3 (OpenRB-338's licence); the program it is compiled into is
// under the GNU AGPL v3, which section 13 of each allows.
//
// https://github.com/pingw33n/openrb-338
//
// Compiled into MzPCD's distortion, and from this repository into MzDebirth's DIST, so the
// two are the same.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace rbdist
{

//
// The tables, built once, as the original built them - in doubles, each stored as a float.
//
//   level[i]       a level or amount knob 0..127 as a gain: level[127] = 1, each step down
//                  1/24 less, level[0] = 0
//   shaper[t][e]   the waveshaper, a row for every fourth SHAPE: a sine's first part bending
//                  into a straight line, the row's number how far round the sine goes
//
struct Tables
{
    float level[128];
    float shaper[32][256];

    static const Tables& get()
    {
        static const Tables t;
        return t;
    }

private:
    static float f32(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }

    Tables()
    {
        const double step = (double) f32(0x3D2AAAABu);   // 0.041666668, a 24th
        level[127] = 1.0f;
        for (int i = 126; i >= 1; --i)
            level[i] = (float) ((double) level[i + 1] - step * (double) level[i + 1]);
        level[0] = 0.0f;

        // pi/8 and pi/1024 as the original had them, pi cut short
        const double c961 = (double) f32(0x3A88642Au), pi8 = (double) f32(0x3EC90FD0u), pi1024 = (double) f32(0x3B490FD0u);
        for (int t = 0; t < 32; ++t)
        {
            float* row = shaper[t];
            const double inc = ((double) (t * t) * c961) * pi8 + pi1024;
            double ph = 0.0, y = 0.0, z = 0.0;
            for (int e = 128; e < 256; ++e)
            {
                const double s = std::sin(ph) * 256.0;
                const double v = ((s * (1.0 - y)) + ((y * z) * 256.0)) * 0.7;
                row[e] = (float) v;
                row[256 - e] = (float) -v;
                y += 1.0 / 128.0;
                z += 1.0 / 128.0;
                ph += inc;
            }
            row[0] = -row[255];
        }
    }
};

// The output gain's grid, GRID[shape][amount] at 0, 1, .. 4 of x/127 * 3.999.
inline double grid(double shape, double amount)
{
    static constexpr float g[5][5] = {
        { 1.2f,  0.4f,  0.28f, 0.2f,  0.15f },
        { 0.35f, 0.2f,  0.15f, 0.14f, 0.12f },
        { 0.2f,  0.15f, 0.13f, 0.12f, 0.12f },
        { 0.15f, 0.12f, 0.11f, 0.1f,  0.1f  },
        { 0.12f, 0.1f,  0.08f, 0.08f, 0.08f },
    };
    const double c127 = 1.0 / 127.0, scale = 3.999;   // f32(1/127) as the original, near enough
    const double s = (std::clamp(shape, 0.0, 127.0) * (double) (float) c127) * scale;
    const double a = (std::clamp(amount, 0.0, 127.0) * (double) (float) c127) * scale;
    const int i = (int) s, j = (int) a;
    const double fs = s - i, fa = a - j;
    auto G = [&] (int r, int c) { return (double) g[r][c]; };
    const double top = (G(i + 1, j + 1) - G(i + 1, j)) * fa + G(i + 1, j);
    const double bot = (G(i, j + 1) - G(i, j)) * fa + G(i, j);
    return fs * (top - bot) + bot;
}

// The level curve between its steps, for an amount that moves smoothly.
inline double level(double amount)
{
    const auto& t = Tables::get();
    const double a = std::clamp(amount, 0.0, 127.0);
    const int i = std::min(126, (int) a);
    return (double) t.level[i] + (a - i) * ((double) t.level[i + 1] - (double) t.level[i]);
}

// The gain into the shaper, and out of it.
inline double inGain(double amount)               { return (level(amount) + 0.0625) * 2048.0; }
inline double outGain(double shape, double amount) { return grid(shape, amount) * 5.0 * (1.0 / 256.0); }

// SHAPE 0: a hard clip instead of the shaper - the gain in, and where it clips, which stops
// coming down from amount 90.
inline double clipGain(double amount)      { return 16.0 * level(amount) + 1.0; }
inline double clipThreshold(double amount) { return 1.0 / clipGain(std::min(amount, 90.0)); }

// The shaper's row for a SHAPE 1..127.
inline const float* row(int shape) { return Tables::get().shaper[std::clamp(shape, 0, 127) >> 2]; }

// A row looked up at v = gain * x + 128: its first entry below 0, entry 254 above 254, and
// between, the two entries either side.
inline double shape(const float* w, double v)
{
    if (v < 0.0)   return (double) w[0];
    if (v > 254.0) return (double) w[254];
    const int i = (int) v;
    return (v - i) * ((double) w[i + 1] - (double) w[i]) + (double) w[i];
}

//
// The unit as ReBirth ran it, on one side of one channel. ReBirth worked in blocks of 64
// samples at 44.1 kHz: on every block the gains move a tenth of the way to where AMOUNT and
// SHAPE put them, and every 16 blocks it looks at the knobs again. Here a block is as long in
// time at any rate, so the knobs answer as slowly as they did. Switching it in brings the
// gains up from where they were left, as ReBirth's did - from nothing, the first time.
//
// AMOUNT and SHAPE are ReBirth's, 0 to 127; SHAPE 0 is ReBirth 1.5's hard clip.
//
class Distortion
{
public:
    void prepare(double rate)
    {
        blockLength = std::max(1, (int) std::lround(64.0 * rate / 44100.0));
        reset();
    }

    void reset()
    {
        at = 0; counter = 0; clip = false; row = 0;
        gainIn = gainOut = targetIn = targetOut = 0.0f;
    }

    void process(float* x, int n, int amount, int shapeKnob)
    {
        amount = std::clamp(amount, 0, 127);
        shapeKnob = std::clamp(shapeKnob, 0, 127);

        for (int i = 0; i < n; ++i)
        {
            if (at == 0) startBlock(amount, shapeKnob);
            if (++at >= blockLength) at = 0;

            if (clip)
                x[i] = (float) std::clamp(cg * (double) x[i], -th, th);
            else
                x[i] = (float) (shape(Tables::get().shaper[row], (double) gainIn * (double) x[i] + 128.0) * (double) gainOut);
        }
    }

private:
    void startBlock(int amount, int shapeKnob)
    {
        if ((counter & 15) == 0)
        {
            clip = shapeKnob == 0;
            targetIn = (float) inGain(amount);
            row = shapeKnob >> 2;
            targetOut = (float) outGain(shapeKnob, amount);
        }
        counter = (uint16_t) (counter + 1);
        gainIn  = (float) ((double) gainIn  * 0.9 + (double) targetIn  * 0.1);
        gainOut = (float) ((double) gainOut * 0.9 + (double) targetOut * 0.1);

        // the clip reads AMOUNT on every block, not just every 16th
        const auto& t = Tables::get().level;
        cg = 16.0 * (double) t[amount] + 1.0;
        th = 1.0 / (amount < 90 ? cg : 16.0 * (double) t[90] + 1.0);
    }

    int blockLength = 64, at = 0;
    uint16_t counter = 0;
    bool clip = false;
    int row = 0;
    float gainIn = 0, gainOut = 0, targetIn = 0, targetOut = 0;
    double cg = 1.0, th = 1.0;
};

} // namespace rbdist
