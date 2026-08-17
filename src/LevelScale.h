#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

// How a signal level becomes a 0..1 display value, for every meter in the app.
//
// Deliberately generous: a plain linear amplitude scale leaves normal speech
// down around 1-2% of the widget, which reads as "nothing is happening". On
// this scale -36 dB fills 40% and -12 dB fills 80%, so the display actually
// moves with the voice.
//
// This lives in its own header because it was duplicated before: the VU meter
// used the dB mapping while the waveform used raw linear amplitude, so the two
// disagreed about the same signal — the meter showed 40% while the waveform
// was pinned to its 2px minimum.
namespace LevelScale
{
    inline constexpr float minDb = -60.0f;

    /** dB (-60..0) -> 0..1 */
    inline float fromDb(float db)
    {
        if (db <= minDb) return 0.0f;
        return juce::jlimit(0.0f, 1.0f, (db - minDb) / -minDb);
    }

    /** Linear RMS (0..1) -> 0..1 on the same scale. */
    inline float fromRms(float rms)
    {
        if (rms <= 0.0f) return 0.0f;
        return fromDb(20.0f * std::log10(rms));
    }

    // Where the colour zones sit on the scale above: green up to -15 dB,
    // yellow to -7.5 dB, red beyond.
    inline constexpr float greenEnd  = 0.750f;
    inline constexpr float yellowEnd = 0.875f;
}
