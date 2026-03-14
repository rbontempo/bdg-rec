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

    // Settings persistence setters (Task 18)
    void setDevice(const juce::String& deviceName);
    void setVolume(int value); // 0-200

    // Settings getters (Task 18)
    juce::String getDeviceComboText() const { return deviceCombo.getText(); }
    int          getVolumeValue()     const { return static_cast<int>(volumeSlider.getValue()); }

    // Device hot-plug refresh (Task 19)
    void refreshDeviceList();

    // Callbacks for settings changes (Task 18)
    std::function<void()> onSettingsChanged;

private:
    AudioEngine& audioEngine;

    // Task 9 – Device selector
    juce::ComboBox deviceCombo;

    // Task 10 – VU meter
    VuMeter vuMeter;

    // Task 11 – Volume
    juce::Slider volumeSlider;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputPanel)
};
