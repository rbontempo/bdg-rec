#include "InputPanel.h"

InputPanel::InputPanel() {}

void InputPanel::paint(juce::Graphics& g)
{
    const float corner = 8.0f;
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);

    // Panel border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // === Header area (40px tall) ===
    const float headerH = 40.0f;
    const float padding = 14.0f;

    // Mic icon (simple path: capsule body + stand arm + base)
    {
        const float iconW = 14.0f;
        const float iconH = 18.0f;
        const float ix = padding;
        const float iy = (headerH - iconH) * 0.5f;

        g.setColour(BdgColours::primary);

        // Mic capsule (rounded rect)
        juce::Path mic;
        const float capW = iconW * 0.6f;
        const float capH = iconH * 0.55f;
        const float capX = ix + (iconW - capW) * 0.5f;
        mic.addRoundedRectangle(capX, iy, capW, capH, capW * 0.5f);
        g.fillPath(mic);

        // Mic arm (arc below capsule, drawn as a thin rounded rect arc approximation)
        juce::Path arm;
        const float armCX = ix + iconW * 0.5f;
        const float armTopY = iy + capH * 0.7f;
        const float armR = iconW * 0.4f;
        arm.addCentredArc(armCX, armTopY, armR, armR, 0.0f,
                          juce::MathConstants<float>::pi, 2.0f * juce::MathConstants<float>::pi, true);
        g.strokePath(arm, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Stand (vertical line + base)
        const float standX = armCX;
        const float standTop = armTopY + armR;
        const float standBot = iy + iconH - 1.0f;
        g.drawLine(standX, standTop, standX, standBot, 1.5f);

        // Base line
        const float baseHalfW = iconW * 0.35f;
        g.drawLine(standX - baseHalfW, standBot, standX + baseHalfW, standBot, 1.5f);
    }

    // "INPUT" label
    juce::Font headerFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setFont(headerFont);
    g.setColour(BdgColours::textPrimary);
    g.drawText("INPUT",
               juce::Rectangle<float>(padding + 22.0f, 0.0f, 100.0f, headerH),
               juce::Justification::centredLeft, false);

    // Header separator line
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
}
