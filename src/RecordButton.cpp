#include "RecordButton.h"

RecordButton::RecordButton()
{
    setSize(64, 64);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void RecordButton::setRecordingState(bool recording)
{
    isRecordingState = recording;
    repaint();
}

void RecordButton::mouseEnter(const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void RecordButton::mouseExit(const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void RecordButton::mouseUp(const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() >= 1 && getLocalBounds().contains(e.getPosition()))
        if (onClick) onClick();
}

void RecordButton::paint(juce::Graphics& g)
{
    const float scale = hovered ? 1.06f : 1.0f;
    const float size  = 64.0f;
    const float cx    = getWidth()  * 0.5f;
    const float cy    = getHeight() * 0.5f;
    const float half  = (size * scale) * 0.5f;

    auto bounds = juce::Rectangle<float>(cx - half, cy - half, size * scale, size * scale);

    if (!isRecordingState)
    {
        // --- Idle: circle ---
        // Shadow / glow
        {
            const float glowR = half + 8.0f;
            juce::ColourGradient glow(juce::Colour(0xffE5067D).withAlpha(0.35f),
                                      cx, cy,
                                      juce::Colours::transparentBlack,
                                      cx + glowR, cy,
                                      true);
            g.setGradientFill(glow);
            g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
        }

        // Pink circle fill
        g.setColour(BdgColours::primary);
        g.fillEllipse(bounds);

        // White inner circle (20px)
        const float innerR = 10.0f * scale;
        g.setColour(juce::Colours::white);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    }
    else
    {
        // --- Recording: rounded rect (stop button) ---
        // Stronger glow
        {
            const float glowR = half + 14.0f;
            juce::ColourGradient glow(juce::Colour(0xffE5067D).withAlpha(0.60f),
                                      cx, cy,
                                      juce::Colours::transparentBlack,
                                      cx + glowR, cy,
                                      true);
            g.setGradientFill(glow);
            g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
        }

        // Pink rounded rect fill
        g.setColour(BdgColours::primary);
        g.fillRoundedRectangle(bounds, 12.0f);

        // White inner square (20px, corner 3px)
        const float innerHalf = 10.0f * scale;
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(cx - innerHalf, cy - innerHalf,
                               innerHalf * 2.0f, innerHalf * 2.0f, 3.0f);
    }
}
