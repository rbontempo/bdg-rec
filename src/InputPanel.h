#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class InputPanel : public juce::Component
{
public:
    InputPanel();
    void paint(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputPanel)
};
