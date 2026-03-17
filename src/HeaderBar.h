#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "Strings.h"

class HeaderBar : public juce::Component
{
public:
    HeaderBar();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // Called when language changes — parent should repaint all panels
    std::function<void()> onLanguageChanged;

    static constexpr int preferredHeight = 48;

private:
    bool hitTest(int x, int y) override { return true; }
    juce::Rectangle<int> ptRect, enRect;
    juce::Image iconImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};
