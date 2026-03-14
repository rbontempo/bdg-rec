#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(960, 600);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1c));
    g.setColour(juce::Colour(0xfff0f0f0));
    g.setFont(24.0f);
    g.drawText("BDG REC", getLocalBounds(), juce::Justification::centred);
}

void MainComponent::resized() {}
