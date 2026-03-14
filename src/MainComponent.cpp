#include "MainComponent.h"

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

    // Overlay and toast on top (added last so they paint over everything)
    addChildComponent(dspOverlay);
    addChildComponent(toastComponent);

    // Wire up record button
    recordingPanel.onRecordClicked = [this]() { handleRecordButtonClicked(); };

    // Task 18 – save on every settings change
    inputPanel.onSettingsChanged  = [this]() { saveSettings(); };
    outputPanel.onSettingsChanged = [this]() { saveSettings(); };

    // Task 18 – load saved settings and apply to UI
    loadSettings();

    setSize(960, 600);
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
        toastComponent.showError("Dispositivo desconectado. Gravacao interrompida.");
    }
}

//==============================================================================
// Recording control
//==============================================================================
void MainComponent::handleRecordButtonClicked()
{
    if (!isRecording)
    {
        // Task 19 – Validate device
        if (audioEngine.getCurrentInputDeviceName().isEmpty())
        {
            toastComponent.showError("Selecione um microfone.");
            return;
        }

        // Task 19 – Validate folder
        juce::File folder = outputPanel.getDestFolder();
        if (!folder.exists() || !folder.isDirectory())
        {
            toastComponent.showError("Configure a pasta de destino.");
            return;
        }

        // Task 19 – startRecording error handling
        if (audioEngine.startRecording(folder))
        {
            isRecording = true;
            recordingPanel.startRecording(folder);
        }
        else
        {
            toastComponent.showError("Falha ao iniciar gravacao.");
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

        if ((doNorm || doNoise || doComp) && lastRecordedFile.existsAsFile())
        {
            // Task 19 – wrap processRecording in try/catch
            try
            {
                audioEngine.processRecording(lastRecordedFile, doNorm, doNoise, doComp);
            }
            catch (const std::exception& e)
            {
                toastComponent.showError(juce::String("Erro no processamento: ") + e.what());
            }
            catch (...)
            {
                toastComponent.showError("Erro desconhecido no processamento.");
            }
        }
        else if (lastRecordedFile.existsAsFile())
        {
            // No processing — just report success
            toastComponent.showSuccess("Gravacao concluida.",
                                       lastRecordedFile.getFileName());
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
                        outputPanel.isCompressorOn());
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
        toastComponent.showSuccess("Processamento concluido.", file.getFileName());
    });
}

void MainComponent::dspError(const juce::String& error)
{
    juce::MessageManager::callAsync([this, error]()
    {
        dspOverlay.hide();
        toastComponent.showError("Erro no processamento: " + error);
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

    // Toast: bottom-center
    const int toastX = (w - ToastComponent::toastWidth)  / 2;
    const int toastY =  h - ToastComponent::toastHeight - 20;
    toastComponent.setBounds(toastX, toastY,
                             ToastComponent::toastWidth,
                             ToastComponent::toastHeight);
}
