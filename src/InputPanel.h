#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "AudioEngine.h"
#include "VuMeter.h"

class InputPanel : public juce::Component,
                   public AudioEngine::Listener
{
public:
    explicit InputPanel(AudioEngine& engine);
    ~InputPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioEngine::Listener
    void audioLevelsChanged(float l, float r) override;

private:
    AudioEngine& audioEngine;

    // Task 9 – Device selector
    juce::ComboBox deviceCombo;

    // Task 10 – VU meter
    VuMeter vuMeter;

    // Task 11 – Volume
    juce::Slider volumeSlider;

    // Task 11 – Monitor toggle
    juce::ToggleButton monitorButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputPanel)
};
