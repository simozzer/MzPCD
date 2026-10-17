// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <array>
#include <cmath>

namespace pcd
{

//
// Twice the sample rate, and back, for the clipper: its harmonics would otherwise fold back
// down as aliases. One 63-tap low-pass at a quarter of the doubled rate - a half-band,
// Blackman-windowed - does both jobs: going up, it takes away the image the doubling makes;
// coming down, everything above the original Nyquist.
//
class Oversampler
{
public:
    static constexpr int taps = 63;

    Oversampler()
    {
        constexpr double pi = 3.14159265358979323846;
        const int mid = taps / 2;
        double sum = 0.0;
        for (int i = 0; i < taps; ++i)
        {
            const int k = i - mid;
            const double sinc = k == 0 ? 0.5 : std::sin(pi * k * 0.5) / (pi * k);
            const double w = 0.42 - 0.5 * std::cos(2.0 * pi * i / (taps - 1)) + 0.08 * std::cos(4.0 * pi * i / (taps - 1));
            h[(size_t) i] = sinc * w;
            sum += h[(size_t) i];
        }
        for (auto& c : h) c /= sum;   // unity at DC
        reset();
    }

    void reset() { up.reset(); down.reset(); }

    // one sample in, the two at twice the rate out: the sample and a zero, filtered, doubled
    void upsample(double x, double& a, double& b)
    {
        a = 2.0 * up.push(x, h);
        b = 2.0 * up.push(0.0, h);
    }

    // the two samples at twice the rate in, one out
    double downsample(double a, double b)
    {
        down.push(a, h);
        return down.push(b, h);
    }

    // how many samples, at the original rate, it delays a signal by going up and back down
    static constexpr int latency() { return (taps - 1) / 2; }

private:
    // a delay line kept twice over, so the taps read it without wrapping
    struct Line
    {
        std::array<double, taps * 2> v {};
        int pos = 0;

        void reset() { v.fill(0.0); pos = 0; }

        double push(double x, const std::array<double, taps>& h)
        {
            pos = pos == 0 ? taps - 1 : pos - 1;
            v[(size_t) pos] = v[(size_t) pos + taps] = x;
            const double* p = v.data() + pos;
            double y = 0.0;
            for (int i = 0; i < taps; ++i) y += h[(size_t) i] * p[i];
            return y;
        }
    };

    std::array<double, taps> h {};
    Line up, down;
};

} // namespace pcd
