#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class Dsp
{
public:
    /** RMS normalize to -16 dB target + brick-wall limiter at -1 dB. */
    static void normalize(juce::AudioBuffer<float>& buffer, double sampleRate);

    /** 3-band compressor (low/mid/high) with auto makeup gain. Optimized for voice. */
    static void compress(juce::AudioBuffer<float>& buffer, double sampleRate);

    /** De-esser: reduces sibilance (4-8 kHz) for voice. */
    static void deEss(juce::AudioBuffer<float>& buffer, double sampleRate);

    /** RNNoise neural-network noise suppression (48 kHz, automatic). */
    static void noiseReduce(juce::AudioBuffer<float>& buffer, double sampleRate);

private:
    Dsp() = delete;
};
