#include "VuMeter.h"
#include "BdgColours.h"
#include "LevelScale.h"

VuMeter::VuMeter() {}

// The scale itself lives in LevelScale.h so the waveform display cannot drift
// away from it again — it used to draw raw linear amplitude while this meter
// used dB, and the two disagreed about the same signal.
static float dbToDisplay(float db)   { return LevelScale::fromDb(db); }
static float rmsToDisplay(float rms) { return LevelScale::fromRms(rms); }

void VuMeter::setLevels(float l, float r)
{
    levelL = rmsToDisplay(juce::jlimit(0.0f, 1.0f, l));
    levelR = rmsToDisplay(juce::jlimit(0.0f, 1.0f, r));

    repaint();
}

void VuMeter::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();

    // Layout constants
    const float labelW   = 14.0f;
    const float barH     = 10.0f;
    const float rowGap   = 6.0f;
    const float scaleH   = 12.0f;
    const float barX     = labelW;
    const float barW     = w - labelW;
    const float corner   = 2.0f;

    juce::Font monoFont(juce::FontOptions().withHeight(10.0f));
    g.setFont(monoFont);

    // Helper to draw one bar row
    auto drawBar = [&](float yTop, float level, const char* channelLabel)
    {
        // Channel label
        g.setColour(BdgColours::textMuted);
        g.drawText(juce::String(channelLabel),
                   juce::Rectangle<float>(0.0f, yTop, labelW, barH),
                   juce::Justification::centredLeft, false);

        // Bar background
        juce::Rectangle<float> bgRect(barX, yTop, barW, barH);
        g.setColour(juce::Colour(0x0fffffff)); // ~6% white
        g.fillRoundedRectangle(bgRect, corner);

        // Filled portion
        const float fillW = level * barW;

        if (fillW > 0.0f)
        {
            // Green portion: 0 to 75%
            const float greenEnd  = barW * 0.750f;
            const float yellowEnd = barW * 0.875f;

            if (fillW > 0.0f)
            {
                float gW = juce::jmin(fillW, greenEnd);
                g.setColour(BdgColours::vuGreen);
                g.fillRoundedRectangle(barX, yTop, gW, barH, corner);
            }

            if (fillW > greenEnd)
            {
                float yW = juce::jmin(fillW - greenEnd, yellowEnd - greenEnd);
                g.setColour(BdgColours::vuYellow);
                // Overlap slightly to avoid gap due to rounding
                g.fillRect(juce::Rectangle<float>(barX + greenEnd, yTop, yW, barH));
            }

            if (fillW > yellowEnd)
            {
                float rW = fillW - yellowEnd;
                g.setColour(BdgColours::vuRed);
                g.fillRect(juce::Rectangle<float>(barX + yellowEnd, yTop, rW, barH));
            }
        }
    };

    const float rowL = 0.0f;
    const float rowR = barH + rowGap;

    drawBar(rowL, levelL, "L");
    drawBar(rowR, levelR, "R");

    // Scale labels below both bars
    const float scaleY = rowR + barH + 3.0f;

    // Chosen to sit evenly along the -60..0 dB scale the bar actually uses.
    // For reference, the colour zones land at -15 dB (green→yellow, 75%)
    // and -7.5 dB (yellow→red, 87.5%).
    const int dbValues[]    = {-48, -36, -24, -12, -6, 0};
    const int numLabels     = 6;

    g.setFont(juce::FontOptions().withHeight(8.0f));
    g.setColour(juce::Colour(0x40ffffff)); // ~25% white

    for (int i = 0; i < numLabels; ++i)
    {
        // Same mapping the bar fill uses, so label and level agree.
        const float xPos = barX + dbToDisplay((float) dbValues[i]) * barW;

        juce::String label = (dbValues[i] == 0) ? "0" : juce::String(dbValues[i]);

        // Centre the text on the tick, but keep it inside the component: "0"
        // sits at the far right of the scale, so half of it fell outside and
        // was clipped away.
        const float textW = 20.0f;
        const float textX = juce::jlimit(0.0f, w - textW, xPos - textW * 0.5f);

        g.drawText(label,
                   juce::Rectangle<float>(textX, scaleY, textW, scaleH),
                   juce::Justification::centred, false);

        // Tick mark
        g.setColour(juce::Colour(0x20ffffff));
        g.drawVerticalLine((int) xPos, scaleY - 2.0f, scaleY);
        g.setColour(juce::Colour(0x40ffffff));
    }
}
