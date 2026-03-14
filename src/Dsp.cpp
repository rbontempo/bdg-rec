#include "Dsp.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include "rnnoise.h"

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
        // Linear-interpolation resample to 48 kHz
        double ratio = targetRate / sampleRate;
        processLen = static_cast<int>(numSamples * ratio);
        resampled.resize(static_cast<size_t>(processLen));

        for (int i = 0; i < processLen; ++i)
        {
            double srcPos = i / ratio;
            int idx = static_cast<int>(srcPos);
            double frac = srcPos - idx;
            int idx1 = std::min(idx + 1, numSamples - 1);
            resampled[static_cast<size_t>(i)] = static_cast<float>((1.0 - frac) * data[idx] + frac * data[idx1]);
        }
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
        for (int i = 0; i < numSamples; ++i)
        {
            double srcIdx = static_cast<double>(i) * processLen / numSamples;
            int idx = static_cast<int>(srcIdx);
            double frac = srcIdx - idx;
            int idx1 = std::min(idx + 1, processLen - 1);
            data[i] = static_cast<float>((1.0 - frac) * output[static_cast<size_t>(idx)]
                                         + frac * output[static_cast<size_t>(idx1)]);
        }
    }
    else
    {
        std::copy(output.begin(), output.end(), data);
    }
}
