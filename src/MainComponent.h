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
    void recordingFinished(const juce::File& file, AudioEngine::StopReason reason) override;
    void recordingSaveFailed(const juce::File& preservedChunkFolder) override;
    void recordingStopping(AudioEngine::StopReason reason) override;
    void inputHealthChanged(AudioEngine::InputHealth health) override;

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
    // Carried from the stop to whichever notification lands last, so the
    // "samples were lost" warning is not overwritten by "saved in folder".
    juce::int64    lastTakeDropped = 0;
    double         lastTakeRate = 0.0;
    bool           isRecording = false;
    bool           diskWarningShown = false;

    // The treatments actually handed to the DSP for the take being processed.
    // The overlay and the "dsp_applied" event used to re-read the toggles,
    // which the user may have changed in the meantime.
    struct DspFlags { bool normalize = false, noise = false, compressor = false, deEsser = false; };
    DspFlags       runningDsp;

    // The device the user actually chose (and that opened). Persisted instead
    // of whatever the box shows, which can be a fallback.
    juce::String   chosenInputDevice;

    // The saved folder was missing at startup (external drive unplugged). The
    // app falls back to the default, but must not overwrite the user's choice
    // in the settings until they pick a folder themselves.
    bool           destFolderUnavailable = false;

    // Chunk folders whose merge failed. Scanned at every launch, wherever they
    // are — the promise "it will be recovered next time" used to depend on the
    // destination folder still being the same.
    juce::StringArray pendingChunkFolders;
    void scanForOrphans(const juce::File& destFolderOrNone);

    // Whether the inline warning currently shows an input-health message.
    bool           healthWarningShown = false;

    // Language-dependent UI refresh, without saving (used at startup too).
    void refreshLanguageUi();
    juce::String dspErrorText(const juce::String& code) const;

    void handleRecordButtonClicked();
    void updateAnalyticsContext();
    void applyLanguageChange();
#if JUCE_MAC
    void installMacMenu();
#endif
    void askAnalyticsConsentIfNeeded();

    // Walks the orphan list one dialog at a time. Only the first was ever
    // offered, so after two crashes the second take needed another relaunch.
    void promptForOrphans(juce::Array<juce::File> orphans, int index);

    // The destination folder lives under ~/Downloads, which macOS guards with
    // TCC. Every distinct probe (stat, statfs, readdir) raises its own consent
    // prompt, and startup used to fire four of them — so the user was asked
    // three or four times for the same folder. Everything that needs the
    // folder now goes through here, once.
    void openDestFolderOnce();
    juce::File savedDestFolder;

    // One notification for the end of a take: plain "saved" when all went
    // well, or a persistent warning that also mentions the lost audio.
    void showTakeResult(const juce::File& file);

    // Guards async dialog callbacks against a destroyed MainComponent.
    std::shared_ptr<std::atomic<bool>> uiAlive = std::make_shared<std::atomic<bool>>(true);

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
        idPortal,
        idAnalytics
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
