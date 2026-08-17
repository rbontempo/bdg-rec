// DSP tests. No audio device and no display needed, so CI runs these.
//
// Relative includes on purpose — see tests/README.md.
#include "../src/Dsp.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

static void check(bool cond, const char* what)
{
    std::printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
    if (! cond) ++failures;
}

static juce::AudioBuffer<float> makeTone(int n, double sampleRate, double freq, float amp)
{
    juce::AudioBuffer<float> b(1, n);
    auto* d = b.getWritePointer(0);
    for (int i = 0; i < n; ++i)
        d[i] = amp * (float) std::sin(2.0 * 3.14159265358979 * freq * i / sampleRate);
    return b;
}

static bool allFinite(const juce::AudioBuffer<float>& b)
{
    const auto* d = b.getReadPointer(0);
    for (int i = 0; i < b.getNumSamples(); ++i)
        if (! std::isfinite(d[i]))
            return false;
    return true;
}

static float peak(const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    const auto* d = b.getReadPointer(0);
    for (int i = 0; i < b.getNumSamples(); ++i)
        p = juce::jmax(p, std::abs(d[i]));
    return p;
}

static float rms(const juce::AudioBuffer<float>& b)
{
    double sum = 0.0;
    const auto* d = b.getReadPointer(0);
    for (int i = 0; i < b.getNumSamples(); ++i)
        sum += (double) d[i] * d[i];
    return (float) std::sqrt(sum / juce::jmax(1, b.getNumSamples()));
}

static void testNormalize()
{
    std::printf("\n-- Dsp::normalize --\n");
    const double sr = 48000.0;

    {
        // A quiet tone should come up towards the -16 dB target.
        auto b = makeTone(sr * 2, sr, 220.0, 0.02f);
        Dsp::normalize(b, sr);
        check(allFinite(b), "quiet tone stays finite");
        check(rms(b) > 0.05f, "quiet tone is brought up");
        check(peak(b) <= 0.8911f, "never exceeds the -1 dB ceiling");
    }

    {
        // A loud tone must be brought DOWN and clamped by the limiter.
        auto b = makeTone(sr, sr, 440.0, 0.99f);
        Dsp::normalize(b, sr);
        check(allFinite(b), "loud tone stays finite");
        check(peak(b) <= 0.8911f, "loud tone is limited to the ceiling");
    }

    {
        // Near-silence: the gain cap must stop the noise floor being lifted to
        // full scale. Without it this asked for roughly +64 dB.
        auto b = makeTone(sr, sr, 300.0, 0.0001f);
        Dsp::normalize(b, sr);
        const float p = peak(b);
        std::printf("     near-silent input (amp 1e-4) -> peak %.4f\n", p);
        check(allFinite(b), "near-silent input stays finite");
        check(p < 0.5f, "gain cap keeps near-silence from being blown up");
    }

    {
        // Digital silence must be left exactly alone, not divided by zero.
        juce::AudioBuffer<float> b(1, 4096);
        b.clear();
        Dsp::normalize(b, sr);
        check(allFinite(b) && peak(b) == 0.0f, "digital silence is untouched");
    }
}

static void testNoiseReduce()
{
    std::printf("\n-- Dsp::noiseReduce --\n");

    // 48 kHz: RNNoise's native rate, no resampling involved.
    {
        const double sr = 48000.0;
        const int n = (int) sr;
        auto b = makeTone(n, sr, 440.0, 0.3f);
        Dsp::noiseReduce(b, sr);
        check(b.getNumSamples() == n, "48 kHz keeps the sample count");
        check(allFinite(b), "48 kHz output is finite");
        check(peak(b) <= 1.01f, "48 kHz output stays in range");
    }

    // 44.1 kHz: exercises the resample out-and-back, where the interpolator
    // used to read one sample past the end of the buffer.
    {
        const double sr = 44100.0;
        const int n = (int) sr;
        auto b = makeTone(n, sr, 440.0, 0.3f);
        Dsp::noiseReduce(b, sr);
        check(b.getNumSamples() == n, "44.1 kHz keeps the sample count");
        check(allFinite(b), "44.1 kHz output is finite (resample path)");
        check(peak(b) <= 1.01f, "44.1 kHz output stays in range");
        check(rms(b) > 0.0f, "44.1 kHz output is not silence");
    }

    // Shorter than one RNNoise frame: the zero-padded tail path.
    {
        const double sr = 48000.0;
        auto b = makeTone(100, sr, 440.0, 0.3f);
        Dsp::noiseReduce(b, sr);
        check(b.getNumSamples() == 100 && allFinite(b),
              "a buffer shorter than one frame is handled");
    }
}

static void testCompressAndDeEss()
{
    std::printf("\n-- Dsp::compress / Dsp::deEss --\n");
    const double sr = 48000.0;

    {
        auto b = makeTone((int) sr, sr, 200.0, 0.5f);
        Dsp::compress(b, sr);
        check(allFinite(b), "compressor output is finite");
        check(peak(b) <= 1.01f, "compressor output stays in range");
    }

    {
        // 6 kHz sits inside the de-esser's sibilance band.
        auto b = makeTone((int) sr, sr, 6000.0, 0.5f);
        const float before = rms(b);
        Dsp::deEss(b, sr);
        check(allFinite(b), "de-esser output is finite");
        check(rms(b) <= before * 1.05f, "de-esser does not amplify the sibilance band");
    }

    {
        // 200 Hz is well outside the band and should pass essentially intact.
        auto b = makeTone((int) sr, sr, 200.0, 0.5f);
        const float before = rms(b);
        Dsp::deEss(b, sr);
        const float after = rms(b);
        std::printf("     200 Hz through de-esser: rms %.4f -> %.4f\n", before, after);
        check(std::abs(after - before) < before * 0.2f,
              "de-esser leaves low frequencies alone");
    }
}

int main()
{
    testNormalize();
    testNoiseReduce();
    testCompressAndDeEss();

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
