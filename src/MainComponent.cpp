#include "MainComponent.h"

MainComponent::MainComponent()
{
    setLookAndFeel(&bdgLookAndFeel);

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

    setSize(960, 600);
}

MainComponent::~MainComponent()
{
    audioEngine.removeListener(this);
    setLookAndFeel(nullptr);
}

void MainComponent::handleRecordButtonClicked()
{
    if (!isRecording)
    {
        // Validate folder
        juce::File folder = outputPanel.getDestFolder();
        if (!folder.exists() || !folder.isDirectory())
        {
            toastComponent.showError("Pasta de destino invalida.");
            return;
        }

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
            audioEngine.processRecording(lastRecordedFile, doNorm, doNoise, doComp);
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
