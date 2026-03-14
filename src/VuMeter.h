#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VuMeter : public juce::Component
{
public:
    VuMeter();

    void setLevels(float l, float r);
    void paint(juce::Graphics& g) override;

private:
    float levelL{0.0f};
    float levelR{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter)
};
