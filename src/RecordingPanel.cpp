#include "RecordingPanel.h"

RecordingPanel::RecordingPanel() {}

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
    const float headerH = 40.0f;
    const float padding = 14.0f;

    // Record circle icon (pink filled circle)
    {
        const float iconSize = 12.0f;
        const float ix = padding;
        const float iy = (headerH - iconSize) * 0.5f;
        g.setColour(BdgColours::primary);
        g.fillEllipse(ix, iy, iconSize, iconSize);
    }

    // "GRAVACAO" label
    juce::Font headerFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setFont(headerFont);
    g.setColour(BdgColours::textPrimary);
    g.drawText("GRAVACAO",
               juce::Rectangle<float>(padding + 20.0f, 0.0f, 120.0f, headerH),
               juce::Justification::centredLeft, false);

    // Header separator line
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
}
