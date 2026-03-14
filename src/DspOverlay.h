#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class DspOverlay : public juce::Component,
                   public juce::Timer
{
public:
    DspOverlay();
    ~DspOverlay() override;

    void paint(juce::Graphics& g) override;

    // Control
    void show(bool normalize, bool noiseReduction, bool compressor, bool deEsser);
    void setCurrentStep(const juce::String& step);
    void hide();

    // Timer
    void timerCallback() override;

private:
    enum class StepState { Pending, Active, Done };

    struct Step
    {
        juce::String key;   // matches AudioEngine emitted step IDs
        juce::String name;
        bool         enabled = true;
        StepState    state   = StepState::Pending;
    };

    juce::Array<Step> steps;

    float spinAngle   = 0.0f;
    float pulseAlpha  = 1.0f;
    float pulseDir    = -1.0f; // -1 = fading, +1 = brightening

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DspOverlay)
};
