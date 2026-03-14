#include "WaveformDisplay.h"
#include "BinaryData.h"
#include "Strings.h"

WaveformDisplay::WaveformDisplay()
{
    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::logobdgrec_png, BinaryData::logobdgrec_pngSize);
}

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
        // --- Idle state: show logo ---
        g.setColour(BdgColours::bgPanel);
        g.fillRoundedRectangle(bounds, 4.0f);

        if (logoImage.isValid())
        {
            // Scale logo to fit, max 120px wide, centered
            const float maxW = 120.0f;
            const float scale = std::min(maxW / logoImage.getWidth(), 1.0f);
            const float imgW = logoImage.getWidth() * scale;
            const float imgH = logoImage.getHeight() * scale;
            const float imgX = bounds.getCentreX() - imgW * 0.5f;
            const float imgY = bounds.getCentreY() - imgH * 0.5f - 10.0f;

            g.setOpacity(0.35f);
            g.drawImage(logoImage,
                        juce::Rectangle<float>(imgX, imgY, imgW, imgH),
                        juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        }

        // "Pronto para gravar" text below logo
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.drawText(Strings::prontoGravar,
                   bounds.removeFromBottom(30.0f),
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
