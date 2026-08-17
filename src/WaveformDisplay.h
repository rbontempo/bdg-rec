#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include <deque>

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    void paint(juce::Graphics& g) override;

    void setRecording(bool shouldRecord);
    void pushRmsSample(float rms);

private:
    bool isRecording = false;
    std::deque<float> rmsSamples;
    juce::Image logoImage;

    // Takes the scaled 0..1 level, not raw RMS.
    juce::Colour vuColor(float level) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
