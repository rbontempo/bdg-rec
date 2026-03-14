#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgLookAndFeel.h"
#include "HeaderBar.h"
#include "InputPanel.h"
#include "RecordingPanel.h"
#include "OutputPanel.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    BdgLookAndFeel bdgLookAndFeel;

    HeaderBar      headerBar;
    InputPanel     inputPanel;
    RecordingPanel recordingPanel;
    OutputPanel    outputPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
