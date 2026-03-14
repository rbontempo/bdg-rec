#include "MainComponent.h"

MainComponent::MainComponent()
{
    setLookAndFeel(&bdgLookAndFeel);

    audioEngine.initialise();

    addAndMakeVisible(headerBar);
    addAndMakeVisible(inputPanel);
    addAndMakeVisible(recordingPanel);
    addAndMakeVisible(outputPanel);

    setSize(960, 600);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
}

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

    // Header: full width, 48px tall
    headerBar.setBounds(0, 0, w, headerHeight);

    // Three columns below header
    const int contentY = headerHeight + padding;
    const int contentH = h - contentY - padding;
    const int totalGap = gap * 2;  // 2 gaps between 3 columns
    const int colW     = (w - padding * 2 - totalGap) / 3;

    inputPanel    .setBounds(padding,                    contentY, colW, contentH);
    recordingPanel.setBounds(padding + colW + gap,       contentY, colW, contentH);
    outputPanel   .setBounds(padding + (colW + gap) * 2, contentY, colW, contentH);
}
