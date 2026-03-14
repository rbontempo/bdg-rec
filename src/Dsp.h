#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class Dsp
{
public:
    /** Peak-normalize to -1 dB ceiling (0.891 linear). */
    static void normalize(juce::AudioBuffer<float>& buffer);

    /** Voice compressor: threshold -20 dB, ratio 4:1, attack 5 ms, release 100 ms. */
    static void compress(juce::AudioBuffer<float>& buffer, double sampleRate);

    /** Spectral subtraction noise reduction.
        Uses first 5 seconds as noise profile, FFT size 2048, 50% overlap. */
    static void noiseReduce(juce::AudioBuffer<float>& buffer, double sampleRate);

private:
    Dsp() = delete;
};
