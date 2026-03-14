#include "OutputPanel.h"

OutputPanel::OutputPanel() {}

void OutputPanel::paint(juce::Graphics& g)
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

    // Headphones icon (two circles connected by an arc)
    {
        const float iconW = 18.0f;
        const float iconH = 14.0f;
        const float ix = padding;
        const float iy = (headerH - iconH) * 0.5f;

        g.setColour(BdgColours::primary);

        // Outer arc (headband)
        juce::Path band;
        const float arcCX = ix + iconW * 0.5f;
        const float arcCY = iy + iconH * 0.6f;
        const float arcR  = iconW * 0.45f;
        band.addCentredArc(arcCX, arcCY, arcR, arcR, 0.0f,
                           juce::MathConstants<float>::pi, 2.0f * juce::MathConstants<float>::pi, true);
        g.strokePath(band, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Left ear cup
        const float cupW = 4.0f;
        const float cupH = 7.0f;
        const float leftCupX = ix;
        const float cupY = iy + iconH - cupH;
        g.fillRoundedRectangle(leftCupX, cupY, cupW, cupH, 1.5f);

        // Right ear cup
        const float rightCupX = ix + iconW - cupW;
        g.fillRoundedRectangle(rightCupX, cupY, cupW, cupH, 1.5f);
    }

    // "OUTPUT" label
    juce::Font headerFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setFont(headerFont);
    g.setColour(BdgColours::textPrimary);
    g.drawText("OUTPUT",
               juce::Rectangle<float>(padding + 26.0f, 0.0f, 100.0f, headerH),
               juce::Justification::centredLeft, false);

    // Header separator line
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
}
