#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay() {}

juce::Colour WaveformDisplay::vuColor(float rms) const
{
    if (rms < 0.75f)   return BdgColours::vuGreen;
    if (rms < 0.875f)  return BdgColours::vuYellow;
    return BdgColours::vuRed;
}

void WaveformDisplay::setRecording(bool shouldRecord)
{
    isRecording = shouldRecord;
    rmsSamples.clear();
    repaint();
}

void WaveformDisplay::pushRmsSample(float rms)
{
    // Keep only enough samples to fill the width
    const int barWidth = 3;
    const int gap      = 1;
    const int stride   = barWidth + gap;
    const int maxSamples = (getWidth() > 0) ? (getWidth() / stride) + 2 : 256;

    rmsSamples.push_back(rms);
    while ((int)rmsSamples.size() > maxSamples)
        rmsSamples.erase(rmsSamples.begin());

    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (!isRecording)
    {
        // --- Idle state ---
        g.setColour(BdgColours::bgPanel);
        g.fillRoundedRectangle(bounds, 4.0f);

        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY() - 14.0f;

        // Outer dashed circle (56px diameter)
        {
            const float r = 28.0f;
            juce::Path circle;
            circle.addEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            juce::PathStrokeType stroke(2.0f);
            float dashes[] = { 4.0f, 4.0f };
            juce::Path dashed;
            stroke.createDashedStroke(dashed, circle, dashes, 2);

            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.fillPath(dashed);
        }

        // Inner solid circle (16px diameter)
        {
            const float r = 8.0f;
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);
        }

        // "Pronto para gravar" text
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.drawText("Pronto para gravar",
                   bounds.withY(cy + 32.0f).withHeight(20.0f),
                   juce::Justification::centred, false);
    }
    else
    {
        // --- Recording state ---
        g.setColour(BdgColours::bgWaveform);
        g.fillRoundedRectangle(bounds, 4.0f);

        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        // Center horizontal baseline
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine((int)(h * 0.5f), bounds.getX(), bounds.getRight());

        if (rmsSamples.empty())
            return;

        const int barWidth = 3;
        const int gap      = 1;
        const int stride   = barWidth + gap;

        // Draw bars right-to-left from the most recent sample
        int numBars = (int)rmsSamples.size();
        float x = w - (float)barWidth;

        for (int i = numBars - 1; i >= 0 && x >= -barWidth; --i, x -= stride)
        {
            float rms = rmsSamples[(size_t)i];
            float barH = std::max(2.0f, rms * h * 0.80f);
            float barY = (h - barH) * 0.5f;

            float alpha = 0.6f + rms * 0.4f;
            g.setColour(vuColor(rms).withAlpha(alpha));
            g.fillRoundedRectangle(x, barY, (float)barWidth, barH, 1.0f);
        }
    }
}
