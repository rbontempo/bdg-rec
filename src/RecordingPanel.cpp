#include "RecordingPanel.h"

RecordingPanel::RecordingPanel(AudioEngine& engine)
    : audioEngine(engine)
{
    // Timer label
    timerLabel.setText("00:00:00", juce::dontSendNotification);
    timerLabel.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));
    timerLabel.setColour(juce::Label::textColourId, BdgColours::textPrimary);
    timerLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timerLabel);

    // Disk space label
    diskSpaceLabel.setText("--G", juce::dontSendNotification);
    diskSpaceLabel.setFont(juce::FontOptions().withHeight(12.0f));
    diskSpaceLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.60f));
    diskSpaceLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(diskSpaceLabel);

    addAndMakeVisible(waveformDisplay);
    addAndMakeVisible(recordButton);

    recordButton.onClick = [this]()
    {
        if (onRecordClicked) onRecordClicked();
    };

    audioEngine.addListener(this);

    // 500ms tick for blink + countdown
    startTimer(500);
}

RecordingPanel::~RecordingPanel()
{
    audioEngine.removeListener(this);
    stopTimer();
}

//==============================================================================
void RecordingPanel::startRecording(const juce::File& destFolder)
{
    isRecording = true;
    elapsedSecs = 0;
    diskUpdateTick = 0;
    currentDestFolder = destFolder;
    waveformDisplay.setRecording(true);
    recordButton.setRecordingState(true);
    updateTimerLabel();
    updateDiskSpace();
    repaint();
}

void RecordingPanel::stopRecording()
{
    isRecording = false;
    waveformDisplay.setRecording(false);
    recordButton.setRecordingState(false);
    timerLabel.setVisible(true);
    repaint();
}

//==============================================================================
void RecordingPanel::diskSpaceWarning(int remainingMinutes)
{
    if (remainingMinutes > 10) diskWarningLevel = 0;
    else if (remainingMinutes >= 2) diskWarningLevel = 1;
    else diskWarningLevel = 2;
    repaint();
}

//==============================================================================
void RecordingPanel::audioLevelsChanged(float rmsL, float rmsR)
{
    // Use average of L and R channels
    float rms = (rmsL + rmsR) * 0.5f;
    juce::MessageManager::callAsync([this, rms]()
    {
        if (isRecording)
            waveformDisplay.pushRmsSample(rms);
    });
}

//==============================================================================
void RecordingPanel::timerCallback()
{
    timerTick++;

    if (isRecording)
    {
        // Increment elapsed every 2 ticks (1 second)
        if (timerTick % 2 == 0)
        {
            elapsedSecs++;
            updateTimerLabel();

            // Disk space every 5 seconds
            diskUpdateTick++;
            if (diskUpdateTick >= 5)
            {
                diskUpdateTick = 0;
                updateDiskSpace();
            }
        }

        // Blink: toggle timer label visibility every tick (500ms)
        bool visible = (timerTick % 2 == 0);
        timerLabel.setVisible(visible);
        repaint(); // for the disk space bar area
    }
}

//==============================================================================
void RecordingPanel::updateTimerLabel()
{
    int h = elapsedSecs / 3600;
    int m = (elapsedSecs % 3600) / 60;
    int s = elapsedSecs % 60;

    timerLabel.setText(
        juce::String::formatted("%02d:%02d:%02d", h, m, s),
        juce::dontSendNotification);
}

void RecordingPanel::updateDiskSpace()
{
    if (!currentDestFolder.exists())
        currentDestFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    juce::int64 freeBytes = currentDestFolder.getBytesFreeOnVolume();
    float freeGB = (float)freeBytes / (1024.0f * 1024.0f * 1024.0f);

    diskSpaceLabel.setText(juce::String(freeGB, 1) + "G",
                           juce::dontSendNotification);
}

//==============================================================================
void RecordingPanel::paint(juce::Graphics& g)
{
    const float corner = 8.0f;
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);

    // Panel border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // === Header area ===
    const int headerH  = 44;
    const float padding = 14.0f;

    // Record circle icon (pink filled circle)
    {
        const float iconSize = 10.0f;
        const float ix = padding;
        const float iy = (headerH - iconSize) * 0.5f;
        g.setColour(BdgColours::primary);
        g.fillEllipse(ix, iy, iconSize, iconSize);
    }

    // "GRAVACAO" label
    g.setFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setColour(BdgColours::textPrimary);
    g.drawText("GRAVACAO",
               juce::Rectangle<float>(padding + 18.0f, 0.0f, 100.0f, (float)headerH),
               juce::Justification::centredLeft, false);

    // Header separator line
    g.setColour(BdgColours::border);
    g.drawHorizontalLine(headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);

    // === Disk space bar (bottom area) ===
    const int diskH = 36;
    auto diskArea = getLocalBounds().removeFromBottom(diskH).reduced(12, 0);
    paintDiskSpaceBar(g, diskArea);
}

void RecordingPanel::paintDiskSpaceBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();

    // HDD icon (simple rect + smaller rect on top)
    {
        const float iconX = r.getX();
        const float iconY = r.getCentreY() - 7.0f;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        // body
        g.fillRoundedRectangle(iconX, iconY + 3.0f, 14.0f, 10.0f, 2.0f);
        // top cap
        g.fillRoundedRectangle(iconX + 3.0f, iconY, 8.0f, 5.0f, 1.0f);
    }

    // "Espaco livre:" text
    const float textX = r.getX() + 20.0f;
    g.setFont(juce::FontOptions().withHeight(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    const float labelW = 76.0f;
    g.drawText("Espaco livre:",
               juce::Rectangle<float>(textX, r.getY(), labelW, r.getHeight()),
               juce::Justification::centredLeft, false);

    // Disk space GB label (right side)
    const float gbLabelW = 36.0f;
    g.setColour(juce::Colours::white.withAlpha(0.60f));
    // Monospace-ish via explicit font
    g.setFont(juce::FontOptions().withHeight(12.0f));
    g.drawText(diskSpaceLabel.getText(),
               juce::Rectangle<float>(r.getRight() - gbLabelW, r.getY(), gbLabelW, r.getHeight()),
               juce::Justification::centredRight, false);

    // Progress bar
    const float barX = textX + labelW + 4.0f;
    const float barW = r.getRight() - gbLabelW - 6.0f - barX;
    const float barH = 6.0f;
    const float barY = r.getCentreY() - barH * 0.5f;

    // Background track
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillRoundedRectangle(barX, barY, barW, barH, 3.0f);

    // Usage fill — estimate based on free space vs typical volume size
    // We use a simple proxy: show usage as max(0, 1 - freeGB / 500) capped at 1
    {
        // Use volume size via File API
        float usageFraction = 0.0f;
        if (currentDestFolder.exists() || currentDestFolder == juce::File())
        {
            juce::File folder = currentDestFolder.exists()
                ? currentDestFolder
                : juce::File::getSpecialLocation(juce::File::userHomeDirectory);

            juce::int64 freeBytes  = folder.getBytesFreeOnVolume();
            juce::int64 totalBytes = folder.getVolumeTotalSize();
            if (totalBytes > 0)
                usageFraction = 1.0f - (float)freeBytes / (float)totalBytes;
        }

        usageFraction = juce::jlimit(0.0f, 1.0f, usageFraction);
        if (usageFraction > 0.0f)
        {
            juce::Colour barColour;
            if (diskWarningLevel == 2)      barColour = BdgColours::vuRed;
            else if (diskWarningLevel == 1) barColour = BdgColours::vuYellow;
            else                            barColour = BdgColours::primary;

            g.setColour(barColour);
            g.fillRoundedRectangle(barX, barY, barW * usageFraction, barH, 3.0f);
        }
    }
}

void RecordingPanel::resized()
{
    const int headerH  = 44;
    const int diskH    = 36;
    const int buttonAreaH = 80;
    const int padding  = 12;

    auto area = getLocalBounds();

    // Header area (just for label bounds)
    auto headerArea = area.removeFromTop(headerH);
    // Timer label: right side of header
    timerLabel.setBounds(headerArea.removeFromRight(90).reduced(4, 8));

    // Bottom disk space area
    area.removeFromBottom(diskH);

    // RecordButton area (below waveform)
    auto buttonArea = area.removeFromBottom(buttonAreaH);
    recordButton.setBounds(buttonArea.withSizeKeepingCentre(64, 64));

    // Waveform fills the rest
    waveformDisplay.setBounds(area.reduced(padding, 4));
}
