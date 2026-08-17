#include "MainComponent.h"
#include "Strings.h"
#include "BinaryData.h"
#include "BdgDialog.h"

MainComponent::MainComponent()
{
    setLookAndFeel(&bdgLookAndFeel);

    // Native menu bar
#if JUCE_MAC
    {
        juce::PopupMenu appleMenu;
        appleMenu.addItem(idAbout, Strings::get().menuAbout);
        appleMenu.addItem(idCheckUpdates, Strings::get().menuCheckUpdates);
        appleMenu.addSeparator();

        juce::PopupMenu langMenu;
        bool isPt = (Strings::getLanguage() == Language::PT);
        langMenu.addItem(idLangPT, "PT", true, isPt);
        langMenu.addItem(idLangEN, "EN", true, !isPt);
        appleMenu.addSubMenu(Strings::get().menuLanguage, langMenu);

        juce::MenuBarModel::setMacMainMenu(this, &appleMenu);
    }
#elif JUCE_WINDOWS
    menuBarComponent = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBarComponent.get());
#endif

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
    addChildComponent(inlineWarning);

    // Wire up record button
    recordingPanel.onRecordClicked = [this]() { handleRecordButtonClicked(); };

    // Wire up language switcher
    headerBar.onLanguageChanged = [this]() { applyLanguageChange(); };

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

    // Check for orphaned recordings from a previous crash.
    // Only scan if the user has previously configured a custom folder
    // (i.e. settings file exists with a destFolder entry). This avoids
    // triggering the macOS folder-access permission dialog for ~/Downloads
    // on the very first launch.
    if (auto* props = appProperties.getUserSettings())
    {
        if (props->getValue("destFolder", "").isNotEmpty())
        {
            juce::MessageManager::callAsync([this]() {
                auto orphans = audioEngine.findOrphanedRecordings(outputPanel.getDestFolder());
                promptForOrphans(orphans, 0);
            });
        }
    }

    // Update checker — weekly GitHub release check
    updateChecker.checkIfDue(appProperties, [this](juce::String newVersion) {
        showUpdateDialog(newVersion);
    });

    // Analytics — nothing is collected or sent until the user has answered
    // the consent prompt; on later runs the stored answer applies silently.
    analyticsReporter.initialise(appProperties, "https://rec.bdg.fm/api/events.php");
    updateAnalyticsContext();

    if (analyticsReporter.hasAskedConsent())
        analyticsReporter.trackEvent("app_open");
    else
        askAnalyticsConsentIfNeeded();

    setSize(720, 420);
}

MainComponent::~MainComponent()
{
    // Stop any in-flight async dialog callback from touching a dead object.
    uiAlive->store(false);

#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
    audioEngine.removeListener(this);
    setLookAndFeel(nullptr);

    analyticsReporter.flush();

    // Flush settings to disk
    appProperties.closeFiles();
}

//==============================================================================
// Crash recovery
//==============================================================================
void MainComponent::promptForOrphans(juce::Array<juce::File> orphans, int index)
{
    if (index >= orphans.size())
        return;

    auto folder = orphans[index];
    auto aliveFlag = uiAlive;

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("BDG rec")
        .withMessage(Strings::get().gravacaoAnterior)
        .withButton(Strings::get().recuperar)
        .withButton(Strings::get().descartar)
        .withButton(Strings::get().ignorar);

    juce::AlertWindow::showAsync(options, [this, aliveFlag, orphans, index, folder](int result)
    {
        if (! aliveFlag->load())
            return;

        if (result == 1) // Recuperar
        {
            auto recovered = audioEngine.recoverRecording(folder);
            if (recovered.existsAsFile())
                inlineWarning.show(
                    Strings::get().recuperado + recovered.getFileName(), InlineWarning::Info);
            else
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG rec", Strings::get().falhaRecuperacao);
        }
        else if (result == 2) // Descartar
        {
            audioEngine.discardRecording(folder);
        }

        // Move on to the next one, if any.
        promptForOrphans(orphans, index + 1);
    });
}

//==============================================================================
// Language
//==============================================================================
void MainComponent::applyLanguageChange()
{
    // One place for every widget that has to be told. The header button and
    // the menu item used to do this separately and had already drifted apart,
    // and neither refreshed InputPanel's buttons or the DSP step names —
    // repaint() does not change the text of a TextButton.
    headerBar.repaint();
    inputPanel.updateLanguage();
    recordingPanel.repaint();
    outputPanel.updateLanguage();
    dspOverlay.updateLanguage();
    menuItemsChanged();
    updateAnalyticsContext();   // locale is part of the analytics context
    saveSettings();
}

//==============================================================================
// Analytics
//==============================================================================
void MainComponent::updateAnalyticsContext()
{
    analyticsReporter.setContext(
        juce::SystemStats::getOperatingSystemName(),
        juce::String(JUCE_APPLICATION_VERSION_STRING),
        // Device *category*, never the name — "AirPods do Renato" is personal data.
        audioEngine.getCurrentInputDeviceCategory(),
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

        // Language (auto-detect system locale on first launch)
        juce::String lang = props->getValue("language", "");
        if (lang.isEmpty())
        {
            auto sysLocale = juce::SystemStats::getUserLanguage();
            lang = sysLocale.startsWith("pt") ? "pt" : "en";
        }
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

    // A disconnect during recording is stopped and reported by the engine
    // through recordingFinished(StopReason::DeviceLost), which also logs the
    // analytics event and carries the salvaged file — nothing to do here.
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
            inlineWarning.show(
                Strings::get().discoBaixo + juce::String(remainingMinutes) + "min.",
                InlineWarning::Warning, 0); // no auto-hide during recording
        });
    }
}

void MainComponent::recordingSaveFailed(const juce::File& preservedChunkFolder)
{
    auto aliveFlag = uiAlive;
    juce::MessageManager::callAsync([this, aliveFlag, preservedChunkFolder]() {
        if (! aliveFlag->load()) return;

        isRecording = false;
        inputPanel.setRecordingActive(false);
        recordingPanel.stopRecording();
        inlineWarning.hide();

        analyticsReporter.trackEvent("error", [&]() {
            auto extra = new juce::DynamicObject();
            extra->setProperty("error_code", "concat_failed");
            extra->setProperty("message", "Final file could not be written; chunks preserved");
            return juce::var(extra);
        }());

        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "BDG rec", Strings::get().falhaSalvar);

        // Point the user at the folder that still holds their audio.
        preservedChunkFolder.revealToUser();
    });
}

void MainComponent::recordingFinished(const juce::File& file, AudioEngine::StopReason reason)
{
    auto aliveFlag = uiAlive;
    juce::MessageManager::callAsync([this, aliveFlag, file, reason]() {
        if (! aliveFlag->load()) return;

        isRecording = false;
        inputPanel.setRecordingActive(false);
        recordingPanel.stopRecording();
        inlineWarning.hide();

        // Everything the four stop paths used to duplicate now lives here,
        // keyed on why the recording ended.
        switch (reason)
        {
            case AudioEngine::StopReason::DiskFull:
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG rec", Strings::get().gravacaoParadaDisco);
                break;

            case AudioEngine::StopReason::DeviceLost:
                analyticsReporter.trackEvent("error", [&]() {
                    auto extra = new juce::DynamicObject();
                    extra->setProperty("error_code", "device_lost");
                    extra->setProperty("message", "Device disconnected during recording");
                    return juce::var(extra);
                }());
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG rec", Strings::get().dispositivoDesconectado);
                break;

            case AudioEngine::StopReason::DeviceRateChanged:
                analyticsReporter.trackEvent("error", [&]() {
                    auto extra = new juce::DynamicObject();
                    extra->setProperty("error_code", "device_rate_changed");
                    extra->setProperty("message", "Device reopened at a different sample rate");
                    return juce::var(extra);
                }());
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "BDG rec", Strings::get().gravacaoParadaDispositivo);
                break;

            case AudioEngine::StopReason::UserRequested:
                break;
        }

        lastRecordedFile = file;

        // The engine counts what the write path could not accept when the disk
        // stalled. Silently handing over a take with holes in it is exactly the
        // kind of thing this audit was about.
        const auto dropped = audioEngine.getDroppedSamples();
        if (dropped > 0)
        {
            const double lostSecs = (double) dropped
                                  / juce::jmax(1.0, audioEngine.getLastSampleRate());
            inlineWarning.show(
                Strings::get().amostrasPerdidas.replace("%S", juce::String(lostSecs, 1)),
                InlineWarning::Warning, 0);

            analyticsReporter.trackEvent("error", [&]() {
                auto extra = new juce::DynamicObject();
                extra->setProperty("error_code", "samples_dropped");
                extra->setProperty("message", juce::String(dropped) + " samples dropped");
                return juce::var(extra);
            }());
        }

        analyticsReporter.trackEvent("recording_end", [&]() {
            auto extra = new juce::DynamicObject();
            extra->setProperty("duration_seconds", recordingPanel.getElapsedSeconds());
            return juce::var(extra);
        }());

        // Treatments only run for a normal stop; an interrupted take is handed
        // over as-is so the user still gets the audio.
        const bool doNorm  = outputPanel.isNormalizeOn();
        const bool doNoise = outputPanel.isNoiseReductionOn();
        const bool doComp  = outputPanel.isCompressorOn();
        const bool doDeEss = outputPanel.isDeEsserOn();

        if (reason == AudioEngine::StopReason::UserRequested
            && (doNorm || doNoise || doComp || doDeEss))
        {
            // No try/catch here: processRecording() only spawns the DSP thread,
            // and an exception thrown on that thread cannot travel back to this
            // call site. The guard that actually catches it lives inside
            // DspThread::run(), and failures arrive through dspError().
            audioEngine.processRecording(file, doNorm, doNoise, doComp, doDeEss);
        }
        else
        {
            inlineWarning.show(
                Strings::get().salvoNaPasta + file.getParentDirectory().getFileName(),
                InlineWarning::Info);
            file.revealToUser();

            analyticsReporter.trackEvent("export_complete", [&]() {
                auto extra = new juce::DynamicObject();
                extra->setProperty("file_size_mb", (double) file.getSize() / (1024.0 * 1024.0));
                return juce::var(extra);
            }());
        }
    });
}

//==============================================================================
// Analytics consent (first run)
//==============================================================================
void MainComponent::askAnalyticsConsentIfNeeded()
{
    if (analyticsReporter.hasAskedConsent())
        return;

    juce::MessageManager::callAsync([this]() {
        auto opts = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::QuestionIcon)
                        .withTitle(Strings::get().consentTitulo)
                        .withMessage(Strings::get().consentCorpo)
                        .withButton(Strings::get().consentAceitar)   // -> 1
                        .withButton(Strings::get().consentRecusar);  // -> 0 (also Esc/close)

        auto aliveFlag = uiAlive;
        juce::AlertWindow::showAsync(opts, [this, aliveFlag](int result)
        {
            if (! aliveFlag->load())
                return;

            // JUCE gives the first button id 1 and the last id 0, and binds 0
            // to Escape and the close box (see LookAndFeel_V2::createAlertWindow,
            // and the recovery dialog above). Testing for 1 makes every other
            // outcome — Escape, closing the window — a refusal, which is the
            // only safe default for a consent prompt.
            const bool allowed = (result == 1);
            analyticsReporter.setConsent(allowed);

            // Consent decides whether the session-open event is collected at
            // all, so it is only recorded once the answer is known.
            if (allowed)
            {
                updateAnalyticsContext();
                analyticsReporter.trackEvent("app_open");
            }
        });
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
            inlineWarning.show(Strings::get().selecioneMic, InlineWarning::Warning);
            return;
        }

        // Task 19 – Validate folder
        juce::File folder = outputPanel.getDestFolder();
        if (!folder.exists() || !folder.isDirectory())
        {
            inlineWarning.show(Strings::get().configurePasta, InlineWarning::Warning);
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
                inlineWarning.show(
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
            inputPanel.setRecordingActive(true);

            // Persist destFolder now. Crash recovery only scans when this key
            // exists (to avoid provoking the macOS folder-permission dialog on
            // a first launch), and a user who never opened settings would
            // otherwise never get their interrupted take offered back — the
            // very promise the save-failure message makes.
            saveSettings();
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
        // Fire and forget: merging the chunks can take a while on a long take,
        // so it runs in the background and everything that follows a stop is
        // handled in recordingFinished().
        audioEngine.stopRecordingAsync(AudioEngine::StopReason::UserRequested);
        recordingPanel.stopRecording();
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
        inlineWarning.show(
            Strings::get().salvoNaPasta + file.getParentDirectory().getFileName(), InlineWarning::Info);
        file.revealToUser();

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
#if JUCE_WINDOWS
    const int menuBarHeight = 24;
    if (menuBarComponent)
        menuBarComponent->setBounds(0, 0, w, menuBarHeight);
    headerBar.setBounds(0, menuBarHeight, w, headerHeight);
    const int contentY = menuBarHeight + headerHeight + padding;
#else
    headerBar.setBounds(0, 0, w, headerHeight);
    const int contentY = headerHeight + padding;
#endif
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
                    .replace("%NEW%", newVersion, false)
                    .replace("%CUR%", currentVersion, false);

    BdgDialogContent::launch(s.updateAvailableTitle,
        new BdgDialogContent({}, body, {
            { s.updateDownload, true,  []() { juce::URL("https://rec.bdg.fm").launchInDefaultBrowser(); } },
            { s.updateIgnore,   false, {} },
        }));
}

//==============================================================================
// MenuBarModel
//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    auto& s = Strings::get();
#if JUCE_MAC
    return { s.menuHelp };
#else
    return { s.menuBdgRec, s.menuHelp };
#endif
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String&)
{
    auto& s = Strings::get();
    juce::PopupMenu menu;

#if JUCE_MAC
    // On macOS, index 0 = Help (BDG rec items are in the Apple menu)
    if (topLevelMenuIndex == 0)
    {
        menu.addItem(idWebsite, s.menuWebsite);
        menu.addItem(idPortal, s.menuPortal);
        menu.addSeparator();
        // Lives here rather than in the Apple menu because that one is built
        // once at startup, which would freeze the tick mark.
        menu.addItem(idAnalytics, s.menuAnalytics, true, analyticsReporter.isEnabled());
    }
#else
    if (topLevelMenuIndex == 0) // BDG rec
    {
        menu.addItem(idAbout, s.menuAbout);
        menu.addItem(idCheckUpdates, s.menuCheckUpdates);
        menu.addSeparator();

        juce::PopupMenu langMenu;
        bool isPt = (Strings::getLanguage() == Language::PT);
        langMenu.addItem(idLangPT, "PT", true, isPt);
        langMenu.addItem(idLangEN, "EN", true, !isPt);
        menu.addSubMenu(s.menuLanguage, langMenu);

        menu.addSeparator();
        menu.addItem(idAnalytics, s.menuAnalytics, true, analyticsReporter.isEnabled());
        menu.addSeparator();
        menu.addItem(idQuit, s.menuQuit);
    }
    else if (topLevelMenuIndex == 1) // Help
    {
        menu.addItem(idWebsite, s.menuWebsite);
        menu.addItem(idPortal, s.menuPortal);
    }
#endif

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int)
{
    switch (menuItemID)
    {
        case idAbout:
            showAboutDialog();
            break;
        case idCheckUpdates:
            updateChecker.forceCheck();
            break;
        case idLangPT:
            Strings::setLanguage(Language::PT);
            applyLanguageChange();
            break;
        case idLangEN:
            Strings::setLanguage(Language::EN);
            applyLanguageChange();
            break;
        case idQuit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        case idWebsite:
            juce::URL("https://www.bichodegoiaba.com.br").launchInDefaultBrowser();
            break;
        case idPortal:
            juce::URL("https://cliente.bichodegoiaba.com.br/").launchInDefaultBrowser();
            break;
        case idAnalytics:
            // The consent dialog tells the user they can change their mind
            // from the menu — this is that control.
            analyticsReporter.setConsent(! analyticsReporter.isEnabled());
            menuItemsChanged();
            break;
        default:
            break;
    }
}

void MainComponent::showAboutDialog()
{
    auto& s = Strings::get();

    BdgDialogContent::launch(s.menuAbout,
        new BdgDialogContent(juce::String("v") + JUCE_APPLICATION_VERSION_STRING,
                             s.aboutBody,
                             { { "OK", true, {} } }));
}
