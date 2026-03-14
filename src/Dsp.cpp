#include "Dsp.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include "rnnoise.h"

//==============================================================================
void Dsp::normalize(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || sampleRate <= 0.0)
        return;

    float* data = buffer.getWritePointer(0);

    // --- Step 1: Compute current RMS ---
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSq += (double)data[i] * (double)data[i];

    float rms = (float)std::sqrt(sumSq / numSamples);
    if (rms < 1e-10f)
        return; // silence

    // --- Step 2: RMS normalize to -16 dB target ---
    const float targetRms = (float)std::pow(10.0, -16.0 / 20.0); // ~0.1585
    const float gain = targetRms / rms;

    for (int i = 0; i < numSamples; ++i)
        data[i] *= gain;

    // --- Step 3: Brick-wall limiter at -1 dB (0.891) ---
    // Lookahead limiter with 5ms attack to avoid clicks
    const float ceiling = 0.891f;
    const int lookahead = (int)(0.005 * sampleRate); // 5ms
    const float releaseMs = 50.0f;
    const float releaseCoeff = (float)std::exp(-1.0 / (releaseMs * 0.001 * sampleRate));

    // Find peak envelope with lookahead
    std::vector<float> envelope(numSamples, 0.0f);

    // Forward pass: find the max peak within the next `lookahead` samples
    for (int i = 0; i < numSamples; ++i)
    {
        float peak = std::abs(data[i]);
        int end = std::min(i + lookahead, numSamples);
        for (int j = i + 1; j < end; ++j)
            peak = std::max(peak, std::abs(data[j]));
        envelope[i] = peak;
    }

    // Apply gain reduction with release
    float currentGain = 1.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float targetGain = 1.0f;
        if (envelope[i] > ceiling)
            targetGain = ceiling / envelope[i];

        // Smooth: instant attack, slow release
        if (targetGain < currentGain)
            currentGain = targetGain; // instant attack
        else
            currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * targetGain;

        data[i] *= currentGain;
    }
}

//==============================================================================
// Helper: single-band compressor with makeup gain on a float array
static void compressBand(float* data, int numSamples, double sampleRate,
                         double thresholdDb, double ratio,
                         double attackMs, double releaseMs)
{
    const double thresholdLin = std::pow(10.0, thresholdDb / 20.0);
    const double attackCoeff  = std::exp(-1.0 / (attackMs  * 0.001 * sampleRate));
    const double releaseCoeff = std::exp(-1.0 / (releaseMs * 0.001 * sampleRate));

    // Measure RMS before
    double rmsBefore = 0.0;
    for (int i = 0; i < numSamples; ++i)
        rmsBefore += (double)data[i] * (double)data[i];
    rmsBefore = std::sqrt(rmsBefore / numSamples);

    // Compress
    double envelope = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double s = std::abs((double)data[i]);
        const double c = (s > envelope) ? attackCoeff : releaseCoeff;
        envelope = c * envelope + (1.0 - c) * s;

        double gain = 1.0;
        if (envelope > thresholdLin)
        {
            double overDb       = 20.0 * std::log10(envelope / thresholdLin);
            double compOver     = overDb / ratio;
            double targetDb     = 20.0 * std::log10(thresholdLin) + compOver;
            double targetLin    = std::pow(10.0, targetDb / 20.0);
            gain = targetLin / envelope;
        }
        data[i] = (float)(data[i] * gain);
    }

    // Auto makeup gain
    double rmsAfter = 0.0;
    for (int i = 0; i < numSamples; ++i)
        rmsAfter += (double)data[i] * (double)data[i];
    rmsAfter = std::sqrt(rmsAfter / numSamples);

    if (rmsAfter > 1e-10)
    {
        float makeup = juce::jmin((float)(rmsBefore / rmsAfter), 6.0f);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= makeup;
    }
}

// Simple 2nd-order Linkwitz-Riley crossover filter (12 dB/oct)
struct LRFilter
{
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void setLowpass(double freq, double sr)
    {
        double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        double cosw = std::cos(w0);
        double sinw = std::sin(w0);
        double alpha = sinw / (2.0 * 0.7071); // Q = 0.7071 for Butterworth
        double a0 = 1.0 + alpha;
        b0 = ((1.0 - cosw) / 2.0) / a0;
        b1 = (1.0 - cosw) / a0;
        b2 = b0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighpass(double freq, double sr)
    {
        double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        double cosw = std::cos(w0);
        double sinw = std::sin(w0);
        double alpha = sinw / (2.0 * 0.7071);
        double a0 = 1.0 + alpha;
        b0 = ((1.0 + cosw) / 2.0) / a0;
        b1 = -(1.0 + cosw) / a0;
        b2 = b0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void process(const float* in, float* out, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double x0 = (double)in[i];
            double y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x0;
            y2 = y1; y1 = y0;
            out[i] = (float)y0;
        }
    }
};

void Dsp::compress(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int n = buffer.getNumSamples();
    if (n == 0 || sampleRate <= 0.0)
        return;

    float* data = buffer.getWritePointer(0);

    // Crossover frequencies for voice: 250 Hz and 3000 Hz
    const double crossLow  = 250.0;
    const double crossHigh = 3000.0;

    // --- Split into 3 bands ---
    std::vector<float> low(n), mid(n), high(n), temp(n);

    // Low band: lowpass at 250 Hz (apply twice for LR4 = 24 dB/oct)
    LRFilter lpA, lpB;
    lpA.setLowpass(crossLow, sampleRate);
    lpB.setLowpass(crossLow, sampleRate);
    lpA.process(data, temp.data(), n);
    lpB.process(temp.data(), low.data(), n);

    // High band: highpass at 3000 Hz (apply twice)
    LRFilter hpA, hpB;
    hpA.setHighpass(crossHigh, sampleRate);
    hpB.setHighpass(crossHigh, sampleRate);
    hpA.process(data, temp.data(), n);
    hpB.process(temp.data(), high.data(), n);

    // Mid band: original minus low minus high
    for (int i = 0; i < n; ++i)
        mid[i] = data[i] - low[i] - high[i];

    // --- Compress each band with voice-optimized settings ---
    //                      threshold  ratio  attack  release
    compressBand(low.data(),  n, sampleRate, -24.0, 3.0, 10.0, 150.0); // gentle on lows
    compressBand(mid.data(),  n, sampleRate, -20.0, 4.0,  5.0, 100.0); // tighter on mids (voice)
    compressBand(high.data(), n, sampleRate, -22.0, 3.0,  3.0,  80.0); // fast on highs (sibilance)

    // --- Sum bands back ---
    for (int i = 0; i < n; ++i)
        data[i] = juce::jlimit(-1.0f, 1.0f, low[i] + mid[i] + high[i]);
}

//==============================================================================
void Dsp::deEss(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int n = buffer.getNumSamples();
    if (n == 0 || sampleRate <= 0.0)
        return;

    float* data = buffer.getWritePointer(0);

    // Extract sibilance band (4-8 kHz) with bandpass (HP at 4k + LP at 8k)
    std::vector<float> sibilance(n);
    std::vector<float> temp(n);

    LRFilter hpA, hpB, lpA, lpB;
    hpA.setHighpass(4000.0, sampleRate);
    hpB.setHighpass(4000.0, sampleRate);
    lpA.setLowpass(8000.0, sampleRate);
    lpB.setLowpass(8000.0, sampleRate);

    // Highpass at 4kHz (LR4)
    hpA.process(data, temp.data(), n);
    hpB.process(temp.data(), sibilance.data(), n);

    // Lowpass at 8kHz (LR4) on the highpassed signal
    lpA.process(sibilance.data(), temp.data(), n);
    lpB.process(temp.data(), sibilance.data(), n);

    // Compress the sibilance band aggressively
    const double thresholdDb = -25.0;
    const double ratio = 8.0;
    const double attackMs = 1.0;
    const double releaseMs = 30.0;
    const double thresholdLin = std::pow(10.0, thresholdDb / 20.0);
    const double attackCoeff  = std::exp(-1.0 / (attackMs  * 0.001 * sampleRate));
    const double releaseCoeff = std::exp(-1.0 / (releaseMs * 0.001 * sampleRate));

    std::vector<float> sibilanceCompressed(n);
    double envelope = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double s = std::abs((double)sibilance[i]);
        const double c = (s > envelope) ? attackCoeff : releaseCoeff;
        envelope = c * envelope + (1.0 - c) * s;

        double gain = 1.0;
        if (envelope > thresholdLin)
        {
            double overDb    = 20.0 * std::log10(envelope / thresholdLin);
            double compOver  = overDb / ratio;
            double targetDb  = 20.0 * std::log10(thresholdLin) + compOver;
            double targetLin = std::pow(10.0, targetDb / 20.0);
            gain = targetLin / envelope;
        }
        sibilanceCompressed[i] = (float)(sibilance[i] * gain);
    }

    // Replace: original - sibilance + compressed sibilance
    for (int i = 0; i < n; ++i)
        data[i] = juce::jlimit(-1.0f, 1.0f,
                               data[i] - sibilance[i] + sibilanceCompressed[i]);
}

//==============================================================================
void Dsp::noiseReduce(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || sampleRate <= 0.0)
        return;

    float* data = buffer.getWritePointer(0);

    // RNNoise requires 48000 Hz and processes frames of exactly 480 samples.
    const double targetRate = 48000.0;
    const int frameSize = 480; // 10 ms at 48 kHz

    bool needsResample = std::abs(sampleRate - targetRate) > 1.0;

    std::vector<float> resampled;
    float* processData = data;
    int processLen = numSamples;

    if (needsResample)
    {
        double ratio = targetRate / sampleRate;
        processLen = (int)(numSamples * ratio);
        resampled.resize(processLen);

        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process(1.0 / ratio, data, resampled.data(), processLen);
        processData = resampled.data();
    }

    // RNNoise expects samples in [-32768, 32768] (16-bit range)
    for (int i = 0; i < processLen; ++i)
        processData[i] *= 32768.0f;

    // Process with RNNoise
    DenoiseState* st = rnnoise_create(nullptr);

    std::vector<float> output(static_cast<size_t>(processLen));
    int pos = 0;
    float frame_in[480];
    float frame_out[480];

    while (pos + frameSize <= processLen)
    {
        std::copy(processData + pos, processData + pos + frameSize, frame_in);
        rnnoise_process_frame(st, frame_out, frame_in);
        std::copy(frame_out, frame_out + frameSize, output.data() + pos);
        pos += frameSize;
    }

    // Handle remaining samples: pad with zeros, process, keep only what we need
    if (pos < processLen)
    {
        int remaining = processLen - pos;
        std::fill(frame_in, frame_in + frameSize, 0.0f);
        std::copy(processData + pos, processData + pos + remaining, frame_in);
        rnnoise_process_frame(st, frame_out, frame_in);
        std::copy(frame_out, frame_out + remaining, output.data() + pos);
    }

    rnnoise_destroy(st);

    // Scale back to [-1, 1]
    for (int i = 0; i < processLen; ++i)
        output[static_cast<size_t>(i)] /= 32768.0f;

    // If we resampled, convert back to original sample rate
    if (needsResample)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        std::vector<float> backSampled(numSamples);
        double ratio = sampleRate / targetRate;
        interpolator.process(1.0 / ratio, output.data(), backSampled.data(), numSamples);
        std::copy(backSampled.begin(), backSampled.end(), data);
    }
    else
    {
        std::copy(output.begin(), output.end(), data);
    }
}
