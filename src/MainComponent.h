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
#include "InlineWarning.h"
#include "UpdateChecker.h"
#include "AnalyticsReporter.h"

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
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
    void diskSpaceWarning(int remainingMinutes) override; // Task 2
    void recordingAutoStopped() override; // Task 2

private:
    BdgLookAndFeel bdgLookAndFeel;

    AudioEngine    audioEngine;

    HeaderBar      headerBar;
    InputPanel     inputPanel{audioEngine};
    RecordingPanel recordingPanel{audioEngine};
    OutputPanel    outputPanel;
    DspOverlay     dspOverlay;
    InlineWarning  inlineWarning;
    // Settings persistence (must be declared before analyticsReporter)
    juce::ApplicationProperties appProperties;

    UpdateChecker      updateChecker;
    AnalyticsReporter  analyticsReporter;

    // Recording state
    juce::File     lastRecordedFile;
    bool           isRecording = false;
    bool           diskWarningShown = false;

    void handleRecordButtonClicked();
    void updateAnalyticsContext();

    // Task 18 – persist / restore settings
    void saveSettings();
    void loadSettings();

    void showUpdateDialog(const juce::String& newVersion);

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    void showAboutDialog();

#if JUCE_WINDOWS
    std::unique_ptr<juce::MenuBarComponent> menuBarComponent;
#endif

    enum MenuIDs
    {
        idAbout = 1,
        idCheckUpdates,
        idLangPT,
        idLangEN,
        idQuit,
        idWebsite,
        idPortal
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
