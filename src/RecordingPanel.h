#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class RecordingPanel : public juce::Component
{
public:
    RecordingPanel();
    void paint(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingPanel)
};
