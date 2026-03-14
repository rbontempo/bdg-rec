#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "AudioEngine.h"
#include "WaveformDisplay.h"
#include "RecordButton.h"

class RecordingPanel : public juce::Component,
                       public AudioEngine::Listener,
                       public juce::Timer
{
public:
    explicit RecordingPanel(AudioEngine& engine);
    ~RecordingPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioEngine::Listener
    void audioLevelsChanged(float rmsL, float rmsR) override;
    void diskSpaceWarning(int remainingMinutes) override;

    // juce::Timer
    void timerCallback() override;

    // Recording state control (called by MainComponent)
    void startRecording(const juce::File& destFolder);
    void stopRecording();

    // Callback wired up by MainComponent
    std::function<void()> onRecordClicked;

private:
    AudioEngine& audioEngine;

    // Child components
    WaveformDisplay waveformDisplay;
    RecordButton    recordButton;

    // Header: timer label
    juce::Label timerLabel;

    // State
    bool  isRecording  = false;
    int   elapsedSecs  = 0;
    int   timerTick    = 0;   // used for blink (incremented each 500 ms tick)

    // Disk space
    juce::File  currentDestFolder;
    int         diskUpdateTick = 0;
    int         diskWarningLevel{0}; // 0=normal, 1=warning, 2=critical
    juce::Label diskSpaceLabel;

    void updateTimerLabel();
    void updateDiskSpace();
    void paintDiskSpaceBar(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingPanel)
};
