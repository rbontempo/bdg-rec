#include "Dsp.h"
#include <cmath>

//==============================================================================
void Dsp::normalize(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    float* data = buffer.getWritePointer(0);

    // Find absolute peak
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max(peak, std::abs(data[i]));

    if (peak == 0.0f)
        return;

    const float target = 0.891f; // -1 dB
    const float gain = target / peak;

    for (int i = 0; i < numSamples; ++i)
        data[i] = juce::jlimit(-1.0f, 1.0f, data[i] * gain);
}

//==============================================================================
void Dsp::compress(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || sampleRate <= 0.0)
        return;

    float* data = buffer.getWritePointer(0);

    const double thresholdDb   = -20.0;
    const double ratio         = 4.0;
    const double attackMs      = 5.0;
    const double releaseMs     = 100.0;

    const double thresholdLinear = std::pow(10.0, thresholdDb / 20.0); // ~0.1
    const double attackCoeff     = std::exp(-1.0 / (attackMs * 0.001 * sampleRate));
    const double releaseCoeff    = std::exp(-1.0 / (releaseMs * 0.001 * sampleRate));

    double envelope = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double sampleAbs = static_cast<double>(std::abs(data[i]));

        const double coeff = (sampleAbs > envelope) ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0 - coeff) * sampleAbs;

        double gain = 1.0;
        if (envelope > thresholdLinear)
        {
            const double overDb         = 20.0 * std::log10(envelope / thresholdLinear);
            const double compressedOver = overDb / ratio;
            const double targetDb       = 20.0 * std::log10(thresholdLinear) + compressedOver;
            const double targetLinear   = std::pow(10.0, targetDb / 20.0);
            gain = targetLinear / envelope;
        }

        data[i] = juce::jlimit(-1.0f, 1.0f, static_cast<float>(data[i] * gain));
    }
}

//==============================================================================
void Dsp::noiseReduce(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || sampleRate <= 0.0)
        return;

    constexpr int fftOrder = 11;
    constexpr int fftSize  = 2048; // 1 << 11
    constexpr int hopSize  = fftSize / 2;

    if (numSamples < fftSize)
        return;

    const int noiseEnd = std::min(static_cast<int>(sampleRate * 5.0), numSamples);

    float* data = buffer.getWritePointer(0);

    // Build Hann window
    std::vector<float> window(fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    juce::dsp::FFT fft(fftOrder);
    // Working buffer for FFT: needs fftSize * 2 floats
    std::vector<float> fftData(fftSize * 2);

    //--------------------------------------------------------------------------
    // 1) Build noise profile from first 5 seconds
    //--------------------------------------------------------------------------
    std::vector<float> noiseSpectrum(fftSize / 2 + 1, 0.0f);
    int noiseFrames = 0;

    {
        int pos = 0;
        while (pos + fftSize <= noiseEnd)
        {
            // Fill fftData with windowed input
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            for (int i = 0; i < fftSize; ++i)
                fftData[i] = data[pos + i] * window[i];

            fft.performRealOnlyForwardTransform(fftData.data());

            // Accumulate magnitudes for bins 0..fftSize/2
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                float re = fftData[bin * 2];
                float im = fftData[bin * 2 + 1];
                noiseSpectrum[bin] += std::sqrt(re * re + im * im);
            }

            ++noiseFrames;
            pos += hopSize;
        }
    }

    if (noiseFrames == 0)
    {
        DBG("noiseReduce: no noise frames collected, skipping");
        return;
    }

    for (auto& v : noiseSpectrum)
        v /= static_cast<float>(noiseFrames);

    DBG("noiseReduce: noiseEnd=" + juce::String(noiseEnd) + " noiseFrames=" + juce::String(noiseFrames)
        + " totalSamples=" + juce::String(numSamples));

    //--------------------------------------------------------------------------
    // 2) Spectral subtraction with overlap-add
    //--------------------------------------------------------------------------
    const float oversubtraction = 2.0f;
    const float spectralFloor   = 0.02f;

    std::vector<float> output(numSamples, 0.0f);
    std::vector<float> windowSum(numSamples, 0.0f);

    {
        int pos = 0;
        while (pos + fftSize <= numSamples)
        {
            // Windowed FFT
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            for (int i = 0; i < fftSize; ++i)
                fftData[i] = data[pos + i] * window[i];

            fft.performRealOnlyForwardTransform(fftData.data());

            // Spectral subtraction
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                float re  = fftData[bin * 2];
                float im  = fftData[bin * 2 + 1];
                float mag = std::sqrt(re * re + im * im);
                float phase = std::atan2(im, re);

                float cleanedMag = std::max(mag - oversubtraction * noiseSpectrum[bin],
                                            spectralFloor * mag);

                fftData[bin * 2]     = cleanedMag * std::cos(phase);
                fftData[bin * 2 + 1] = cleanedMag * std::sin(phase);
            }

            fft.performRealOnlyInverseTransform(fftData.data());

            // Overlap-add with synthesis window and FFT scaling
            for (int i = 0; i < fftSize; ++i)
            {
                int idx = pos + i;
                if (idx < numSamples)
                {
                    output[idx]    += fftData[i] * window[i] / static_cast<float>(fftSize);
                    windowSum[idx] += window[i] * window[i];
                }
            }

            pos += hopSize;
        }
    }

    //--------------------------------------------------------------------------
    // 3) Normalize by window sum; crossfade at boundaries
    //--------------------------------------------------------------------------
    constexpr float fullCoverage = 0.9f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (windowSum[i] >= fullCoverage)
        {
            data[i] = juce::jlimit(-1.0f, 1.0f, output[i] / windowSum[i]);
        }
        else if (windowSum[i] > 1e-10f)
        {
            float blend     = windowSum[i] / fullCoverage;
            float processed = output[i] / windowSum[i];
            float mixed     = processed * blend + data[i] * (1.0f - blend);
            data[i] = juce::jlimit(-1.0f, 1.0f, mixed);
        }
        // else: keep original sample (no coverage)
    }
}
