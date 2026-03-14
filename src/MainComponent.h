#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgLookAndFeel.h"
#include "HeaderBar.h"
#include "InputPanel.h"
#include "RecordingPanel.h"
#include "OutputPanel.h"
#include "AudioEngine.h"
#include "DspOverlay.h"
#include "ToastComponent.h"

class MainComponent : public juce::Component,
                      public AudioEngine::Listener
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioEngine::Listener
    void audioLevelsChanged(float rmsL, float rmsR) override {}
    void dspStarted() override;
    void dspStepChanged(const juce::String& step) override;
    void dspFinished(const juce::File& file) override;
    void dspError(const juce::String& error) override;

private:
    BdgLookAndFeel bdgLookAndFeel;

    AudioEngine    audioEngine;

    HeaderBar      headerBar;
    InputPanel     inputPanel{audioEngine};
    RecordingPanel recordingPanel{audioEngine};
    OutputPanel    outputPanel;
    DspOverlay     dspOverlay;
    ToastComponent toastComponent;

    // Recording state
    juce::File     lastRecordedFile;
    bool           isRecording = false;

    void handleRecordButtonClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
