#include "ToastComponent.h"

ToastComponent::ToastComponent()
{
    setVisible(false);
    setInterceptsMouseClicks(false, false);
}

ToastComponent::~ToastComponent()
{
    stopTimer();
}

void ToastComponent::showSuccess(const juce::String& msg, const juce::String& sub)
{
    message  = msg;
    subtitle = sub;
    type     = ToastType::Success;
    showToast();
}

void ToastComponent::showError(const juce::String& msg)
{
    message  = msg;
    subtitle = "";
    type     = ToastType::Error;
    showToast();
}

void ToastComponent::showToast()
{
    stopTimer();
    setVisible(true);
    toFront(false);
    repaint();
    startTimer(5000);
}

void ToastComponent::timerCallback()
{
    stopTimer();
    setVisible(false);
}

void ToastComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float corner = 8.0f;

    // Background
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);

    // Border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // Left accent bar (4px wide, clipped to rounded rect)
    {
        juce::Colour accentCol = (type == ToastType::Success)
                                 ? juce::Colour(0xff22c55e)  // green
                                 : juce::Colour(0xffef4444); // red

        juce::Path accentPath;
        // Full rounded rect, then intersect with left strip
        juce::Rectangle<float> accentRect(bounds.getX(), bounds.getY(),
                                          4.0f, bounds.getHeight());
        // Clip properly: fill full rounded rect first in clip region
        {
            juce::Graphics::ScopedSaveState ss(g);
            juce::Path clip;
            clip.addRoundedRectangle(bounds, corner);
            g.reduceClipRegion(clip);
            g.setColour(accentCol);
            g.fillRect(accentRect);
        }
    }

    // Text area starts after the accent bar
    const float textX = 14.0f;
    const float textW = bounds.getWidth() - textX - 10.0f;

    bool hasSub = subtitle.isNotEmpty();

    if (hasSub)
    {
        // Main message
        g.setFont(juce::FontOptions().withHeight(12.0f).withStyle("Bold"));
        g.setColour(juce::Colours::white);
        g.drawText(message,
                   juce::Rectangle<float>(textX, 10.0f, textW, 18.0f),
                   juce::Justification::centredLeft, true);

        // Subtitle
        g.setFont(juce::FontOptions().withHeight(10.0f));
        g.setColour(BdgColours::textMuted);
        g.drawText(subtitle,
                   juce::Rectangle<float>(textX, 30.0f, textW, 16.0f),
                   juce::Justification::centredLeft, true);
    }
    else
    {
        // Centered message
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white);
        g.drawText(message,
                   juce::Rectangle<float>(textX, 0.0f, textW, bounds.getHeight()),
                   juce::Justification::centredLeft, true);
    }
}
