#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    void paint(juce::Graphics& g) override;

    void setRecording(bool shouldRecord);
    void pushRmsSample(float rms);

private:
    bool isRecording = false;
    std::vector<float> rmsSamples;

    juce::Colour vuColor(float rms) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
