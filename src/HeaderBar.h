#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class HeaderBar : public juce::Component
{
public:
    HeaderBar();
    void paint(juce::Graphics& g) override;

    static constexpr int preferredHeight = 48;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
