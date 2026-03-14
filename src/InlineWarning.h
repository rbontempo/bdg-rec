#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

// Small inline warning bar that sits at the bottom of a panel.
// Shows a colored dot + message text. Auto-hides after a timeout.
class InlineWarning : public juce::Component,
                      public juce::Timer
{
public:
    enum Level { Info, Warning, Error };

    InlineWarning() { setVisible(false); }

    void show(const juce::String& msg, Level lvl = Warning, int autoHideMs = 5000)
    {
        message = msg;
        level = lvl;
        setVisible(true);
        repaint();
        if (autoHideMs > 0)
            startTimer(autoHideMs);
        else
            stopTimer();
    }

    void hide()
    {
        stopTimer();
        setVisible(false);
    }

    void timerCallback() override { hide(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f, 0.0f);

        // Background
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Dot color
        juce::Colour dotColour;
        switch (level)
        {
            case Info:    dotColour = BdgColours::vuGreen;  break;
            case Warning: dotColour = BdgColours::vuYellow; break;
            case Error:   dotColour = BdgColours::vuRed;    break;
        }

        // Dot
        const float dotSize = 6.0f;
        const float dotX = bounds.getX() + 8.0f;
        const float dotY = bounds.getCentreY() - dotSize * 0.5f;
        g.setColour(dotColour);
        g.fillEllipse(dotX, dotY, dotSize, dotSize);

        // Text
        g.setFont(juce::FontOptions().withHeight(10.0f));
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawText(message,
                   juce::Rectangle<float>(dotX + dotSize + 6.0f, bounds.getY(),
                                          bounds.getWidth() - dotSize - 22.0f, bounds.getHeight()),
                   juce::Justification::centredLeft, true);
    }

private:
    juce::String message;
    Level level = Warning;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InlineWarning)
};
