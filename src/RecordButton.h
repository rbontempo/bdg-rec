#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class RecordButton : public juce::Component,
                     public juce::Timer
{
public:
    RecordButton();

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e)  override;
    void mouseUp(const juce::MouseEvent& e)    override;
    void timerCallback() override;

    void setRecordingState(bool recording);

    std::function<void()> onClick;

private:
    bool isRecordingState = false;
    bool hovered = false;
    float pulsePhase = 0.0f; // 0..2π for smooth pulse animation

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordButton)
};
