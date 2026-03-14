#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgLookAndFeel.h"
#include "HeaderBar.h"
#include "InputPanel.h"
#include "RecordingPanel.h"
#include "OutputPanel.h"
#include "AudioEngine.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    BdgLookAndFeel bdgLookAndFeel;

    AudioEngine    audioEngine;

    HeaderBar      headerBar;
    InputPanel     inputPanel{audioEngine};
    RecordingPanel recordingPanel{audioEngine};
    OutputPanel    outputPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
