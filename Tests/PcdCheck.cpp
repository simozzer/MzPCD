// MzPCD - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The engine, checked on its own: clean when not driven and distorted when it is, the
// pattern pushing the drive on the steps it strikes, TONE, the feedback sustaining and
// howling at its pitch and dying away when turned down, the oversampling keeping the
// aliases down, the dry signal lined up with the wet, the block size making no difference,
// and nothing at any setting running away. The clock is MzPCF's, held to the sample by its
// own checks. Links no JUCE.
//
//     PcdCheck                 run the checks
//     PcdCheck --wav <dir>     and also write a few demo phrases there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Pcd.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace pcd;

namespace
{

int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-50s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0)
{
    char buf[128];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

constexpr double pi = 3.14159265358979323846;
constexpr double rate = 48000.0;

struct Host
{
    double rate = 48000.0, bpm = 120.0, ppq = 0.0;
    bool playing = true;
    int block = 512;
    long long played = 0;

    template <class F>
    void run(Pcd& p, float* l, float* r, int n, F&& afterBlock)
    {
        for (int at = 0; at < n; )
        {
            const int len = std::min(block, n - at);
            p.setHostClock(playing, ppq + (double) played * bpm / 60.0 / rate, bpm);
            p.process(l + at, r ? r + at : nullptr, len);
            afterBlock(at, len);
            if (playing) played += len;
            at += len;
        }
    }

    void run(Pcd& p, float* l, float* r, int n) { run(p, l, r, n, [] (int, int) {}); }
};

std::vector<float> sine(int n, double hz, double amp)
{
    std::vector<float> v((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin(2.0 * pi * hz * i / rate));
    return v;
}

std::vector<float> noise(size_t n, unsigned seed, float amp = 0.5f)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-amp, amp);
    std::vector<float> v(n);
    for (auto& x : v) x = u(rng);
    return v;
}

// The power at one frequency over [from, from + n), by Goertzel, as an amplitude.
double amplitudeAt(const std::vector<float>& v, size_t from, size_t n, double hz)
{
    const double w = 2.0 * pi * hz / rate, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + n; ++i)
    {
        const double s0 = v[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 2.0 * std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) n;
}

double rms(const std::vector<float>& v, size_t from, size_t n)
{
    double s = 0;
    for (size_t i = from; i < from + n; ++i) s += (double) v[i] * v[i];
    return std::sqrt(s / (double) n);
}

// Total harmonic distortion of a 1 kHz sine over a window: what is not the fundamental.
double thd(const std::vector<float>& v, size_t from, size_t n)
{
    const double total = rms(v, from, n);
    const double fundamental = amplitudeAt(v, from, n, 1000.0) / std::sqrt(2.0);
    return std::sqrt(std::max(0.0, total * total - fundamental * fundamental)) / std::max(1.0e-12, fundamental);
}

std::vector<float> through(Settings s, std::vector<float> x, Host h = {})
{
    Pcd p;
    p.prepare(h.rate);
    p.setSettings(s);
    h.run(p, x.data(), nullptr, (int) x.size());
    return x;
}

Settings clean()
{
    Settings s;
    s.drive = 0.0f;
    s.driveAmount = 0.0f;
    s.tone = 1.0f;
    s.feedback = 0.0f;
    return s;
}

//============================================================================== the checks

void driveCheck()
{
    std::printf("the drive\n");

    const auto in = sine(48000, 1000.0, 0.1);
    auto y = through(clean(), in);
    const double gainDb = 20.0 * std::log10(amplitudeAt(y, 9600, 19200, 1000.0) / 0.1);
    const double low = thd(y, 9600, 19200);
    check(std::fabs(gainDb) < 1.5 && low < 0.05, "not driven: clean, and at the level it came in",
          fmt("%+.1f dB, %.1f%% THD", gainDb, low * 100.0));

    Settings s = clean();
    s.drive = 1.0f;
    y = through(s, in);
    const double high = thd(y, 9600, 19200);
    const double level = 20.0 * std::log10(rms(y, 9600, 19200) / rms(in, 9600, 19200));
    check(high > 0.25, "driven hard: heavily distorted", fmt("%.0f%% THD", high * 100.0));
    check(std::fabs(level) < 9.0, "and not much louder, made up for", fmt("%+.1f dB", level));

    // the pattern: pattern 1 strikes the beats; between them, clean again
    s = clean();
    s.pattern = 0;
    s.driveAmount = 1.0f;
    s.decay = 0.0f;
    y = through(s, sine(24000 * 2, 1000.0, 0.1));
    const double onBeat = thd(y, 24000 + 120, 960), between = thd(y, 24000 + 4800, 960);
    check(onBeat > 0.2 && between < 0.05, "the pattern pushes the drive on the steps it strikes",
          fmt("%.0f%% on the beat, %.1f%% between", onBeat * 100.0, between * 100.0));
}

void toneCheck()
{
    std::printf("the tone\n");

    auto bright = clean(), dark = clean();
    bright.drive = dark.drive = 0.8f;
    dark.tone = 0.0f;
    const auto in = sine(48000, 1000.0, 0.3);
    const auto b = through(bright, in), d = through(dark, in);
    const double hb = amplitudeAt(b, 9600, 19200, 7000.0), hd = amplitudeAt(d, 9600, 19200, 7000.0);
    check(20.0 * std::log10(hb / std::max(1.0e-12, hd)) > 20.0, "TONE dark takes the top off the distortion",
          fmt("7 kHz: %.1f dB down", 20.0 * std::log10(hb / std::max(1.0e-12, hd))));

    // the pattern moving the tone: brighter on the beats
    Settings s = dark;
    s.pattern = 0;
    s.toneAmount = 1.0f;
    s.decay = 0.0f;
    const auto y = through(s, sine(24000 * 2, 1000.0, 0.3));
    const double onBeat = amplitudeAt(y, 24000 + 120, 960, 5000.0), between = amplitudeAt(y, 24000 + 4800, 960, 5000.0);
    check(onBeat > between * 4.0, "the pattern opens the tone on the steps it strikes",
          fmt("5 kHz: %.1f dB brighter", 20.0 * std::log10(onBeat / std::max(1.0e-12, between))));
}

void feedbackCheck()
{
    std::printf("the feedback\n");

    // a burst, then silence in
    auto burst = noise(48000 * 4, 5, 0.3f);
    std::fill(burst.begin() + 2400, burst.end(), 0.0f);

    Settings s = clean();
    s.drive = 0.6f;
    s.tone = 0.5f;
    auto y = through(s, burst);
    check(rms(y, 48000, 24000) < 1.0e-4, "no feedback: the sound stops when the input does", fmt("%.2g after", rms(y, 48000, 24000)));

    s.feedback = 1.0f;
    y = through(s, burst);
    const double howl = rms(y, 48000 * 2, 48000);
    float peak = 0;
    for (float v : y) peak = std::max(peak, std::fabs(v));
    check(howl > 0.02, "full feedback: it sustains, and howls", fmt("%.3f two seconds on", howl));
    check(peak < 2.0f, "but never runs away", fmt("peak %.2f", peak));

    // its pitch: where the speaker's resonance puts it
    const double toneHz = 500.0 * std::pow(24.0, 0.5);
    const double want = Pcd::howlFrequency(toneHz);
    int crossings = 0;
    for (size_t i = 48000 * 2 + 1; i < 48000 * 3; ++i) crossings += y[i - 1] < 0 && y[i] >= 0 ? 1 : 0;
    check(std::fabs(crossings - want) < want * 0.2, "at the pitch TONE gives it", fmt("%.0f Hz, near %.0f", crossings, want));

    // turned down while it howls: it dies away
    Pcd p;
    p.prepare(rate);
    p.setSettings(s);
    std::vector<float> x = burst;
    Host h;
    h.run(p, x.data(), nullptr, 48000 * 2);
    s.feedback = 0.0f;
    p.setSettings(s);
    h.run(p, x.data() + 48000 * 2, nullptr, 48000 * 2);
    check(rms(x, 48000 * 3, 48000) < 1.0e-3, "and turned down, dies away", fmt("%.2g a second on", rms(x, 48000 * 3, 48000)));

    // the pattern swelling the feedback: louder on the beats than between, long after the input stopped
    s = clean();
    s.drive = 0.6f;
    s.tone = 0.5f;
    s.feedback = 0.55f;
    s.feedbackAmount = 0.45f;
    s.pattern = 0;
    s.decay = 0.3f;
    y = through(s, burst);
    check(rms(y, 48000 * 3, 48000) > 1.0e-3, "the pattern can keep it howling on its steps", fmt("%.3f", rms(y, 48000 * 3, 48000)));
}

void aliasCheck()
{
    std::printf("aliases\n");

    // 9.5 kHz, driven hard: its fifth harmonic, 47.5 kHz, would fold to 500 Hz
    Settings s = clean();
    s.drive = 1.0f;
    const auto y = through(s, sine(48000, 9500.0, 0.5));
    const double f = amplitudeAt(y, 9600, 38400, 9500.0), a = amplitudeAt(y, 9600, 38400, 500.0);
    const double db = 20.0 * std::log10(a / std::max(1.0e-12, f));
    check(db < -40.0, "the oversampling keeps the aliases down", fmt("500 Hz alias at %.0f dB", db));
}

void latencyCheck()
{
    std::printf("dry and wet\n");

    // an impulse, half dry and half wet, not driven: one peak, where the latency says
    Settings s = clean();
    s.mix = 0.5f;
    std::vector<float> x(256, 0.0f);
    x[10] = 0.01f;
    const auto y = through(s, x);
    int peak = 0;
    for (int i = 0; i < (int) y.size(); ++i) if (std::fabs(y[(size_t) i]) > std::fabs(y[(size_t) peak])) peak = i;
    check(peak == 10 + Pcd::latency() && std::fabs(y[10]) < 1.0e-4f, "the dry side lined up with the wet",
          fmt("one peak, %.0f samples late", peak - 10));

    s.mix = 0.0f;
    s.drive = 1.0f;
    s.level = 6.0f;
    const auto in = noise(4800, 9, 0.2f);
    const auto z = through(s, in);
    double worst = 0;
    for (size_t i = (size_t) Pcd::latency(); i < z.size(); ++i)
        worst = std::max(worst, (double) std::fabs(z[i] - 1.99526f * in[i - (size_t) Pcd::latency()]));
    check(worst < 1.0e-4, "MIX dry, LEVEL +6 dB: the input, delayed and louder", fmt("worst %.2g", worst));
}

void gridCheck()
{
    std::printf("the grid\n");

    for (int r : { Rate::Eighth, Rate::Sixteenth, Rate::ThirtySecond })
    {
        Settings s;
        s.pattern = 2;
        s.rate = r;
        Pcd p;
        p.prepare(rate);
        p.setSettings(s);
        Host h;
        h.block = 1;
        std::vector<float> x(24000 * 2 + 1, 0.0f);
        int strikes = 0, last = -1;
        h.run(p, x.data(), nullptr, (int) x.size(), [&] (int, int) { if (p.step() != last) { ++strikes; last = p.step(); } });
        const int want = 2 * stepsPerQuarter(r) + 1;
        check(strikes == want, r == Rate::Eighth ? "1/8: a step every eighth" : r == Rate::Sixteenth ? "1/16: every sixteenth" : "1/32: every thirty-second",
              fmt("%.0f steps in two beats", strikes));
    }
}

void blockCheck()
{
    std::printf("blocks\n");

    Settings s;
    s.pattern = 8;
    s.drive = 0.6f;
    s.driveAmount = 0.6f;
    s.toneAmount = -0.5f;
    s.feedback = 0.4f;
    s.feedbackAmount = 0.4f;
    const auto in = noise(44100 * 2, 7, 0.3f);
    std::vector<std::vector<float>> outs;
    for (int block : { 1, 37, 512, 4096 })
    {
        Host h;
        h.rate = 44100.0;
        h.bpm = 133.0;
        h.block = block;
        Pcd p;
        p.prepare(h.rate);
        p.setSettings(s);
        auto l = in, r = in;
        h.run(p, l.data(), r.data(), (int) l.size());
        outs.push_back(l);
    }
    double worst = 0;
    for (size_t o = 1; o < outs.size(); ++o)
        for (size_t i = 0; i < in.size(); ++i)
            worst = std::max(worst, (double) std::fabs(outs[o][i] - outs[0][i]));
    check(worst < 1.0e-5, "the same at block sizes 1, 37, 512 and 4096", fmt("worst difference %.2g", worst));
}

void stabilityCheck()
{
    std::printf("stability\n");

    std::mt19937 rng(101);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    bool finite = true;
    double loudest = 0;

    for (double r : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Pcd p;
        p.prepare(r);
        Host h;
        h.rate = r;
        h.bpm = 40.0 + 260.0 * u(rng);
        h.block = 256;
        const int chunk = (int) (r * 0.25);
        for (int round = 0; round < 32; ++round)
        {
            Settings s;
            s.pattern = (int) (u(rng) * patterns) % patterns;
            s.rate = (int) (u(rng) * 3.0f) % 3;
            s.drive = u(rng); s.tone = u(rng);
            s.feedback = round % 3 == 0 ? 1.0f : u(rng);
            s.driveAmount = u(rng); s.toneAmount = u(rng) * 2.0f - 1.0f; s.feedbackAmount = u(rng);
            s.decay = u(rng); s.mix = u(rng); s.level = -24.0f + 36.0f * u(rng);
            p.setSettings(s);
            h.playing = round % 7 != 3;
            auto l = noise((size_t) chunk, (unsigned) round, round % 5 == 0 ? 1.0f : 0.3f);
            auto rr = noise((size_t) chunk, (unsigned) round + 99, 0.3f);
            h.run(p, l.data(), rr.data(), chunk);
            for (int i = 0; i < chunk; ++i)
            {
                finite = finite && std::isfinite(l[(size_t) i]) && std::isfinite(rr[(size_t) i]);
                loudest = std::max(loudest, (double) std::max(std::fabs(l[(size_t) i]), std::fabs(rr[(size_t) i])));
            }
        }
    }
    check(finite, "no NaN or infinity, any setting, any rate", "");
    check(loudest < 8.0, "bounded, even at full feedback and LEVEL", fmt("peak %.2f", loudest));
}

void speedCheck()
{
    std::printf("speed\n");
    Pcd p;
    p.prepare(48000.0);
    Settings s;
    s.pattern = 2;
    s.feedback = 0.5f;
    p.setSettings(s);
    Host h;
    auto l = noise(48000 * 10, 1), r = noise(48000 * 10, 2);
    const auto t0 = std::chrono::steady_clock::now();
    h.run(p, l.data(), r.data(), (int) l.size());
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(10.0 / secs > 20.0, "stereo at 48 kHz, faster than 20x real time", fmt("%.0fx", 10.0 / secs));
}

//============================================================================== the demo renders

void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) { std::printf("  could not write %s\n", path.c_str()); return; }
    const uint32_t frames = (uint32_t) l.size(), bytes = frames * 4;
    auto u32 = [f] (uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32((uint32_t) sr); u32((uint32_t) sr * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (float v : { l[i], r[i] })
        {
            const int16_t s = (int16_t) std::lrint(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
            std::fwrite(&s, 2, 1, f);
        }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

// a plucked bass line, a note a beat, to put through it
std::vector<float> bassLine(int n, double bpm)
{
    const double notes[] = { 55.0, 55.0, 65.41, 49.0 };
    std::vector<float> v((size_t) n, 0.0f);
    const int perBeat = (int) (rate * 60.0 / bpm);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const int beat = i / perBeat;
        const double f = notes[beat % 4];
        const double t = (double) (i % perBeat) / rate;
        ph += f / rate;
        if (ph >= 1.0) ph -= 1.0;
        v[(size_t) i] = (float) (0.3 * (2.0 * ph - 1.0) * std::exp(-t * 3.0));
    }
    return v;
}

void demos(const std::string& dir)
{
    std::printf("demo renders\n");
    const double bpm = 126.0;
    const int twoBars = (int) (rate * 60.0 / bpm * 8.0);

    struct Demo { const char* name; Settings s; };
    Settings bite;   bite.pattern = 8;  bite.drive = 0.25f; bite.driveAmount = 0.8f; bite.toneAmount = 0.6f; bite.tone = 0.35f;
    Settings howl;   howl.pattern = 0;  howl.drive = 0.6f;  howl.tone = 0.55f; howl.feedback = 0.45f; howl.feedbackAmount = 0.5f; howl.decay = 0.5f;
    Settings wah;    wah.pattern = 10;  wah.drive = 0.5f;   wah.tone = 0.2f;   wah.toneAmount = 1.0f;  wah.driveAmount = 0.2f;
    const Demo list[] = { { "pcd-bite", bite }, { "pcd-feedback", howl }, { "pcd-tone-sweep", wah } };

    for (const auto& d : list)
    {
        const int n = twoBars * 2;
        auto l = bassLine(n, bpm), r = l;
        Pcd p;
        p.prepare(rate);
        p.setSettings(d.s);
        Host h;
        h.bpm = bpm;
        h.run(p, l.data(), r.data(), n);
        writeWav(dir + "/" + d.name + ".wav", l, r, (int) rate);
    }
}

} // namespace

int main(int argc, char** argv)
{
    gridCheck();
    driveCheck();
    toneCheck();
    feedbackCheck();
    aliasCheck();
    latencyCheck();
    blockCheck();
    stabilityCheck();
    speedCheck();

    if (argc >= 3 && std::strcmp(argv[1], "--wav") == 0)
        demos(argv[2]);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
