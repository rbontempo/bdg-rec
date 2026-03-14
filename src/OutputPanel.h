#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class OutputPanel : public juce::Component
{
public:
    OutputPanel();
    void paint(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
