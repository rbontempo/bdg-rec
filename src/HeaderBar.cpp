#include "HeaderBar.h"

HeaderBar::HeaderBar()
{
    setSize(960, preferredHeight);
}

void HeaderBar::paint(juce::Graphics& g)
{
    const int h = getHeight();
    const int w = getWidth();

    // === Logo icon: 20x20 pink rounded rect at left ===
    const float iconSize = 20.0f;
    const float iconX = 14.0f;
    const float iconY = (h - iconSize) * 0.5f;

    g.setColour(BdgColours::primary);
    g.fillRoundedRectangle(iconX, iconY, iconSize, iconSize, 4.0f);

    // Waveform icon inside the pink box: 3 white vertical bars of varying heights
    {
        const float barW = 2.0f;
        const float gap  = 2.5f;
        const float cx   = iconX + iconSize * 0.5f;
        const float bars[3] = { 10.0f, 14.0f, 8.0f };
        const float startX = cx - (barW * 3 + gap * 2) * 0.5f;

        g.setColour(juce::Colours::white);
        for (int i = 0; i < 3; ++i)
        {
            const float bx = startX + i * (barW + gap);
            const float bh = bars[i];
            const float by = iconY + (iconSize - bh) * 0.5f;
            g.fillRoundedRectangle(bx, by, barW, bh, barW * 0.5f);
        }
    }

    // === "BDG" + " REC" text ===
    const float textX = iconX + iconSize + 8.0f;
    const float textY = 0.0f;
    const float textH = (float)h;

    juce::Font boldFont(juce::FontOptions().withHeight(15.0f).withStyle("Bold"));

    // "BDG" in pink
    g.setFont(boldFont);
    g.setColour(BdgColours::primary);
    // Measure "BDG" width using GlyphArrangement
    juce::GlyphArrangement ga;
    ga.addLineOfText(boldFont, "BDG", 0.0f, 0.0f);
    const float bdgWidth = ga.getBoundingBox(0, -1, true).getWidth();

    g.drawText("BDG", juce::Rectangle<float>(textX, textY, bdgWidth + 4.0f, textH),
               juce::Justification::centredLeft, false);

    // " REC" in white
    g.setColour(BdgColours::textPrimary);
    g.drawText(" REC", juce::Rectangle<float>(textX + bdgWidth + 2.0f, textY, 52.0f, textH),
               juce::Justification::centredLeft, false);

    // === Version label: right-aligned ===
    juce::Font mutedFont(juce::FontOptions().withHeight(12.0f));
    g.setFont(mutedFont);
    g.setColour(BdgColours::textMuted);
    g.drawText("v1.0",
               juce::Rectangle<float>(0.0f, 0.0f, (float)w - 14.0f, (float)h),
               juce::Justification::centredRight, false);

    // === Bottom border line ===
    g.setColour(BdgColours::border);
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
}
