#include "VuMeter.h"
#include "BdgColours.h"

VuMeter::VuMeter() {}

// Convert linear RMS (0-1) to a 0-1 display value using dB scale.
// Maps -60 dB → 0.0, 0 dB → 1.0
static float rmsToDisplay(float rms)
{
    if (rms <= 0.0f) return 0.0f;
    const float minDb = -60.0f;
    float db = 20.0f * std::log10(rms);
    if (db <= minDb) return 0.0f;
    return (db - minDb) / -minDb; // normalize to 0-1
}

void VuMeter::setLevels(float l, float r)
{
    levelL = rmsToDisplay(juce::jlimit(0.0f, 1.0f, l));
    levelR = rmsToDisplay(juce::jlimit(0.0f, 1.0f, r));

    static int dbgCount = 0;
    if (++dbgCount % 20 == 0)
        DBG("VuMeter: in L=" + juce::String(l, 4) + " R=" + juce::String(r, 4)
            + " display L=" + juce::String(levelL, 3) + " R=" + juce::String(levelR, 3)
            + " bounds=" + juce::String(getWidth()) + "x" + juce::String(getHeight()));

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

    // dB positions: -24, -12, -9, -6, -3, 0
    // Map dB to linear 0-1 using simple linear scale relative to 0 dB
    // We use a linear mapping where 0 dB = 1.0 and -24 dB proportionally.
    // Since bars are linear amplitude, map dB: amplitude = 10^(dB/20)
    // But for display labels we just evenly space them across the bar.
    // Labels: -24, -12, -9, -6, -3, 0
    const int dbValues[]    = {-24, -12, -9, -6, -3, 0};
    const int numLabels     = 6;

    g.setFont(juce::FontOptions().withHeight(8.0f));
    g.setColour(juce::Colour(0x40ffffff)); // ~25% white

    for (int i = 0; i < numLabels; ++i)
    {
        // Amplitude = 10^(dB/20), linear position on bar
        float amp = std::pow(10.0f, dbValues[i] / 20.0f);
        float xPos = barX + amp * barW;

        juce::String label = (dbValues[i] == 0) ? "0" : juce::String(dbValues[i]);

        // Center the text around xPos
        float textW = 20.0f;
        g.drawText(label,
                   juce::Rectangle<float>(xPos - textW * 0.5f, scaleY, textW, scaleH),
                   juce::Justification::centred, false);

        // Tick mark
        g.setColour(juce::Colour(0x20ffffff));
        g.drawVerticalLine((int)(barX + amp * barW), scaleY - 2.0f, scaleY);
        g.setColour(juce::Colour(0x40ffffff));
    }
}
