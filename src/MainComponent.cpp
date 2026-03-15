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

    if (auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice())
        outputPanel.setSampleRate((int)dev->getCurrentSampleRate());

    addAndMakeVisible(headerBar);
    addAndMakeVisible(inputPanel);
    addAndMakeVisible(recordingPanel);
    addAndMakeVisible(outputPanel);

    // Overlay on top
    addChildComponent(dspOverlay);

    // Wire up record button
    recordingPanel.onRecordClicked = [this]() { handleRecordButtonClicked(); };

    // Wire up language switcher
    headerBar.onLanguageChanged = [this]()
    {
        inputPanel.repaint();
        recordingPanel.repaint();
        outputPanel.updateLanguage();
        saveSettings();
    };

    // Task 18 – save on every settings change
    inputPanel.onSettingsChanged  = [this]() { saveSettings(); updateAnalyticsContext(); };
    outputPanel.onSettingsChanged = [this]()
    {
        recordingPanel.setDestFolder(outputPanel.getDestFolder());
        saveSettings();
    };

    // Task 18 – load saved settings and apply to UI
    loadSettings();

    // Sync disk space display with current folder
    recordingPanel.setDestFolder(outputPanel.getDestFolder());

    // Check for orphaned recordings from a previous crash
    juce::MessageManager::callAsync([this]() {
        auto orphans = audioEngine.findOrphanedRecordings(outputPanel.getDestFolder());
        if (!orphans.isEmpty())
        {
            auto folder = orphans.getFirst();
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("BDG rec")
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
                            "BDG rec", Strings::get().falhaRecuperacao);
                }
                else if (result == 2) // Descartar
                {
                    audioEngine.discardRecording(folder);
                }
            });
        }
    });

    // Update checker — weekly GitHub release check
    updateChecker.checkIfDue(appProperties, [this](juce::String newVersion) {
        showUpdateDialog(newVersion);
    });

    // Analytics
    analyticsReporter.initialise(appProperties, "https://rec.bdg.fm/api/events.php");
    updateAnalyticsContext();
    analyticsReporter.trackEvent("app_open");

    setSize(720, 420);
}

MainComponent::~MainComponent()
{
    audioEngine.removeListener(this);
    setLookAndFeel(nullptr);

    analyticsReporter.flush();

    // Flush settings to disk
    appProperties.closeFiles();
}

//==============================================================================
// Analytics
//==============================================================================
void MainComponent::updateAnalyticsContext()
{
    analyticsReporter.setContext(
        juce::SystemStats::getOperatingSystemName(),
        juce::String(JUCE_APPLICATION_VERSION_STRING),
        audioEngine.getCurrentInputDeviceName(),
        Strings::getLanguage() == Language::EN ? "en" : "pt-BR"
    );
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
        props->setValue("language",        Strings::getLanguage() == Language::EN ? "en" : "pt");
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
            {
                outputPanel.setDestFolder(folder);
                recordingPanel.setDestFolder(folder);
            }
        }

        // Treatment toggles
        outputPanel.setNormalize     (props->getBoolValue("normalize",      false));
        outputPanel.setNoiseReduction(props->getBoolValue("noiseReduction", false));
        outputPanel.setCompressor    (props->getBoolValue("compressor",     false));
        outputPanel.setDeEsser       (props->getBoolValue("deEsser",        false));

        // Language
        juce::String lang = props->getValue("language", "pt");
        Strings::setLanguage(lang == "en" ? Language::EN : Language::PT);
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
        analyticsReporter.trackEvent("error", [&]() {
            auto extra = new juce::DynamicObject();
            extra->setProperty("error_code", "device_lost");
            extra->setProperty("message", "Device disconnected during recording");
            return juce::var(extra);
        }());

        isRecording = false;
        recordingPanel.stopRecording();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG rec", Strings::get().dispositivoDesconectado);
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
            "BDG rec", Strings::get().gravacaoParadaDisco);
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
                "BDG rec", Strings::get().falhaIniciar);
        }
    }
    else
    {
        // Stop recording
        lastRecordedFile = audioEngine.stopRecording();
        isRecording = false;

        // Track recording end
        {
            auto extra = new juce::DynamicObject();
            extra->setProperty("duration_seconds", recordingPanel.getElapsedSeconds());
            analyticsReporter.trackEvent("recording_end", juce::var(extra));
        }

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
                    "BDG rec", Strings::get().erroProcessamento + e.what());
            }
            catch (...)
            {
                dspOverlay.hide();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG rec", Strings::get().erroDesconhecido);
            }
        }
        else if (lastRecordedFile.existsAsFile())
        {
            // No processing — report success inline
            recordingPanel.getInlineWarning().show(
                Strings::get().salvo + lastRecordedFile.getFileName(), InlineWarning::Info);

            // Track export complete (no DSP)
            {
                auto extra = new juce::DynamicObject();
                extra->setProperty("file_size_mb", (double)lastRecordedFile.getSize() / (1024.0 * 1024.0));
                analyticsReporter.trackEvent("export_complete", juce::var(extra));
            }
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

        // Track DSP applied
        {
            auto extra = new juce::DynamicObject();
            juce::Array<juce::var> effects;
            if (outputPanel.isNormalizeOn())      effects.add("normalize");
            if (outputPanel.isNoiseReductionOn()) effects.add("denoise");
            if (outputPanel.isCompressorOn())     effects.add("compress");
            if (outputPanel.isDeEsserOn())        effects.add("deesser");
            extra->setProperty("effects", effects);
            analyticsReporter.trackEvent("dsp_applied", juce::var(extra));
        }

        // Track export complete
        {
            auto extra = new juce::DynamicObject();
            extra->setProperty("file_size_mb", (double)file.getSize() / (1024.0 * 1024.0));
            analyticsReporter.trackEvent("export_complete", juce::var(extra));
        }
    });
}

void MainComponent::dspError(const juce::String& error)
{
    juce::MessageManager::callAsync([this, error]()
    {
        dspOverlay.hide();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG rec", Strings::get().erroProcessamento + error);

        analyticsReporter.trackEvent("error", [&]() {
            auto extra = new juce::DynamicObject();
            extra->setProperty("error_code", "dsp_crash");
            extra->setProperty("message", error);
            return juce::var(extra);
        }());
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

//==============================================================================
// Update checker
//==============================================================================
void MainComponent::showUpdateDialog(const juce::String& newVersion)
{
    auto& s = Strings::get();
    auto currentVersion = juce::String(JUCE_APPLICATION_VERSION_STRING);
    auto body = s.updateAvailableBody
                    .replace("%s", newVersion, false);
    // Replace second %s with current version
    body = body.replace("%s", currentVersion, false);

    auto options = juce::MessageBoxOptions()
                       .withTitle(s.updateAvailableTitle)
                       .withMessage(body)
                       .withButton(s.updateDownload)
                       .withButton(s.updateIgnore)
                       .withIconType(juce::MessageBoxIconType::InfoIcon);

    juce::AlertWindow::showAsync(options, [](int result) {
        if (result == 1) // "Download" button
            juce::URL("https://rec.bdg.fm").launchInDefaultBrowser();
    });
}
