#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
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
    void devicesChanged() override; // Task 19

private:
    BdgLookAndFeel bdgLookAndFeel;

    AudioEngine    audioEngine;

    HeaderBar      headerBar;
    InputPanel     inputPanel{audioEngine};
    RecordingPanel recordingPanel{audioEngine};
    OutputPanel    outputPanel;
    DspOverlay     dspOverlay;
    ToastComponent toastComponent;

    // Task 18 – Settings persistence
    juce::ApplicationProperties appProperties;

    // Recording state
    juce::File     lastRecordedFile;
    bool           isRecording = false;

    void handleRecordButtonClicked();

    // Task 18 – persist / restore settings
    void saveSettings();
    void loadSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
