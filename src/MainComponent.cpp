#include "MainComponent.h"
#include "Strings.h"

MainComponent::MainComponent()
{
    setLookAndFeel(&bdgLookAndFeel);

    // Task 18 – init ApplicationProperties
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = "BDG_REC";
        opts.folderName      = "BDG";
        opts.filenameSuffix  = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(opts);
    }

    audioEngine.initialise();
    audioEngine.addListener(this);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(inputPanel);
    addAndMakeVisible(recordingPanel);
    addAndMakeVisible(outputPanel);

    // Overlay on top
    addChildComponent(dspOverlay);

    // Wire up record button
    recordingPanel.onRecordClicked = [this]() { handleRecordButtonClicked(); };

    // Task 18 – save on every settings change
    inputPanel.onSettingsChanged  = [this]() { saveSettings(); };
    outputPanel.onSettingsChanged = [this]() { saveSettings(); };

    // Task 18 – load saved settings and apply to UI
    loadSettings();

    // Check for orphaned recordings from a previous crash
    juce::MessageManager::callAsync([this]() {
        auto orphans = audioEngine.findOrphanedRecordings(outputPanel.getDestFolder());
        if (!orphans.isEmpty())
        {
            auto folder = orphans.getFirst();
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("BDG REC")
                .withMessage(Strings::get().gravacaoAnterior)
                .withButton(Strings::get().recuperar)
                .withButton(Strings::get().descartar)
                .withButton(Strings::get().ignorar);

            juce::AlertWindow::showAsync(options, [this, folder](int result)
            {
                if (result == 1) // Recuperar
                {
                    auto recovered = audioEngine.recoverRecording(folder);
                    if (recovered.existsAsFile())
                        recordingPanel.getInlineWarning().show(
                            Strings::get().recuperado + recovered.getFileName(), InlineWarning::Info);
                    else
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                            "BDG REC", Strings::get().falhaRecuperacao);
                }
                else if (result == 2) // Descartar
                {
                    audioEngine.discardRecording(folder);
                }
            });
        }
    });

    setSize(720, 420);
}

MainComponent::~MainComponent()
{
    audioEngine.removeListener(this);
    setLookAndFeel(nullptr);

    // Flush settings to disk
    appProperties.closeFiles();
}

//==============================================================================
// Task 18 – Settings persistence
//==============================================================================
void MainComponent::saveSettings()
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("inputDevice",     inputPanel.getDeviceComboText());
        props->setValue("volume",          (int)inputPanel.getVolumeValue());
        props->setValue("destFolder",      outputPanel.getDestFolder().getFullPathName());
        props->setValue("normalize",       outputPanel.isNormalizeOn());
        props->setValue("noiseReduction",  outputPanel.isNoiseReductionOn());
        props->setValue("compressor",      outputPanel.isCompressorOn());
        props->setValue("deEsser",         outputPanel.isDeEsserOn());
        props->saveIfNeeded();
    }
}

void MainComponent::loadSettings()
{
    if (auto* props = appProperties.getUserSettings())
    {
        // Input device
        juce::String device = props->getValue("inputDevice", "");
        if (device.isNotEmpty())
            inputPanel.setDevice(device);

        // Volume (default 100)
        int vol = props->getIntValue("volume", 100);
        inputPanel.setVolume(vol);

        // Destination folder
        juce::String folderPath = props->getValue("destFolder", "");
        if (folderPath.isNotEmpty())
        {
            juce::File folder(folderPath);
            if (folder.isDirectory())
                outputPanel.setDestFolder(folder);
        }

        // Treatment toggles
        outputPanel.setNormalize     (props->getBoolValue("normalize",      false));
        outputPanel.setNoiseReduction(props->getBoolValue("noiseReduction", false));
        outputPanel.setCompressor    (props->getBoolValue("compressor",     false));
        outputPanel.setDeEsser       (props->getBoolValue("deEsser",        false));
    }
}

//==============================================================================
// Task 19 – Device hot-plug
//==============================================================================
void MainComponent::devicesChanged()
{
    // Refresh the input device ComboBox
    inputPanel.refreshDeviceList();

    // If we were recording and the device is gone, show error
    if (isRecording && audioEngine.getCurrentInputDeviceName().isEmpty())
    {
        isRecording = false;
        recordingPanel.stopRecording();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG REC", Strings::get().dispositivoDesconectado);
    }
}

//==============================================================================
// Task 2: Disk space monitoring
//==============================================================================
void MainComponent::diskSpaceWarning(int remainingMinutes)
{
    if (remainingMinutes <= 10 && !diskWarningShown)
    {
        diskWarningShown = true;
        juce::MessageManager::callAsync([this, remainingMinutes]() {
            recordingPanel.getInlineWarning().show(
                Strings::get().discoBaixo + juce::String(remainingMinutes) + "min.",
                InlineWarning::Warning, 0); // no auto-hide during recording
        });
    }
}

void MainComponent::recordingAutoStopped()
{
    juce::MessageManager::callAsync([this]() {
        isRecording = false;
        recordingPanel.stopRecording();
        recordingPanel.getInlineWarning().hide();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG REC", Strings::get().gravacaoParadaDisco);
    });
}

//==============================================================================
// Recording control
//==============================================================================
void MainComponent::handleRecordButtonClicked()
{
    DBG("handleRecordButtonClicked: isRecording=" + juce::String(isRecording ? "true" : "false"));
    if (!isRecording)
    {
        // Task 19 – Validate device
        if (audioEngine.getCurrentInputDeviceName().isEmpty())
        {
            recordingPanel.getInlineWarning().show(Strings::get().selecioneMic, InlineWarning::Warning);
            return;
        }

        // Task 19 – Validate folder
        juce::File folder = outputPanel.getDestFolder();
        if (!folder.exists() || !folder.isDirectory())
        {
            recordingPanel.getInlineWarning().show(Strings::get().configurePasta, InlineWarning::Warning);
            return;
        }

        // Check disk space before recording (need at least 1 hour = ~520MB)
        {
            auto freeBytes = folder.getBytesFreeOnVolume();
            double sampleRate = 48000.0;
            if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
                sampleRate = dev->getCurrentSampleRate();
            int bytesPerSec = (int)(sampleRate * 3.0); // 24-bit mono
            int remainingMin = (int)(freeBytes / bytesPerSec / 60);

            if (remainingMin < 60)
            {
                recordingPanel.getInlineWarning().show(
                    Strings::get().discoInsuficiente + " (~" + juce::String(remainingMin) + Strings::get().liberarEspaco,
                    InlineWarning::Error);
                return;
            }
        }

        // Task 19 – startRecording error handling
        DBG("handleRecordButtonClicked: folder=" + folder.getFullPathName());
        DBG("handleRecordButtonClicked: device=" + audioEngine.getCurrentInputDeviceName());
        if (audioEngine.startRecording(folder))
        {
            DBG("Recording started successfully");
            isRecording = true;
            diskWarningShown = false;
            recordingPanel.startRecording(folder);
        }
        else
        {
            DBG("Recording FAILED to start");
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "BDG REC", Strings::get().falhaIniciar);
        }
    }
    else
    {
        // Stop recording
        lastRecordedFile = audioEngine.stopRecording();
        isRecording = false;
        recordingPanel.stopRecording();

        // Run DSP if any treatment is enabled
        bool doNorm  = outputPanel.isNormalizeOn();
        bool doNoise = outputPanel.isNoiseReductionOn();
        bool doComp  = outputPanel.isCompressorOn();
        bool doDeEss = outputPanel.isDeEsserOn();

        if ((doNorm || doNoise || doComp || doDeEss) && lastRecordedFile.existsAsFile())
        {
            // Task 19 – wrap processRecording in try/catch
            try
            {
                audioEngine.processRecording(lastRecordedFile, doNorm, doNoise, doComp, doDeEss);
            }
            catch (const std::exception& e)
            {
                dspOverlay.hide();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG REC", Strings::get().erroProcessamento + e.what());
            }
            catch (...)
            {
                dspOverlay.hide();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG REC", Strings::get().erroDesconhecido);
            }
        }
        else if (lastRecordedFile.existsAsFile())
        {
            // No processing — report success inline
            recordingPanel.getInlineWarning().show(
                Strings::get().salvo + lastRecordedFile.getFileName(), InlineWarning::Info);
        }
    }
}

//==============================================================================
// AudioEngine::Listener
//==============================================================================
void MainComponent::dspStarted()
{
    juce::MessageManager::callAsync([this]()
    {
        dspOverlay.show(outputPanel.isNormalizeOn(),
                        outputPanel.isNoiseReductionOn(),
                        outputPanel.isCompressorOn(),
                        outputPanel.isDeEsserOn());
        resized(); // ensure overlay covers full window
    });
}

void MainComponent::dspStepChanged(const juce::String& step)
{
    juce::MessageManager::callAsync([this, step]()
    {
        dspOverlay.setCurrentStep(step);
    });
}

void MainComponent::dspFinished(const juce::File& file)
{
    juce::MessageManager::callAsync([this, file]()
    {
        dspOverlay.hide();
        recordingPanel.getInlineWarning().show(
            Strings::get().salvo + file.getFileName(), InlineWarning::Info);
    });
}

void MainComponent::dspError(const juce::String& error)
{
    juce::MessageManager::callAsync([this, error]()
    {
        dspOverlay.hide();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG REC", Strings::get().erroProcessamento + error);
    });
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(BdgColours::bgWindow);
}

void MainComponent::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    const int headerHeight = HeaderBar::preferredHeight;
    const int padding      = 12;
    const int gap          = 12;

    // Header: full width
    headerBar.setBounds(0, 0, w, headerHeight);

    // Three columns below header
    const int contentY = headerHeight + padding;
    const int contentH = h - contentY - padding;
    const int totalGap = gap * 2;
    const int colW     = (w - padding * 2 - totalGap) / 3;

    inputPanel    .setBounds(padding,                    contentY, colW, contentH);
    recordingPanel.setBounds(padding + colW + gap,       contentY, colW, contentH);
    outputPanel   .setBounds(padding + (colW + gap) * 2, contentY, colW, contentH);

    // Overlay: covers entire window
    dspOverlay.setBounds(0, 0, w, h);

}
