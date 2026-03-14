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
    clearActions();
    showToast();
}

void ToastComponent::showError(const juce::String& msg)
{
    message  = msg;
    subtitle = "";
    type     = ToastType::Error;
    clearActions();
    showToast();
}

void ToastComponent::showWithActions(const juce::String& msg,
                                      const juce::String& a1Label, std::function<void()> onA1,
                                      const juce::String& a2Label, std::function<void()> onA2)
{
    message      = msg;
    subtitle     = "";
    type         = ToastType::Error;
    hasActions   = true;
    action1Label = a1Label;
    action2Label = a2Label;
    onAction1    = std::move(onA1);
    onAction2    = std::move(onA2);

    stopTimer(); // no auto-dismiss when actions are present
    setInterceptsMouseClicks(true, false);
    setVisible(true);
    toFront(false);
    repaint();
}

void ToastComponent::clearActions()
{
    hasActions   = false;
    action1Label = "";
    action2Label = "";
    onAction1    = nullptr;
    onAction2    = nullptr;
    setInterceptsMouseClicks(false, false);
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

//==============================================================================
void ToastComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!hasActions) return;

    if (action1Rect.contains(e.getPosition()))
    {
        auto cb = onAction1;
        clearActions();
        setVisible(false);
        if (cb) cb();
    }
    else if (action2Rect.contains(e.getPosition()))
    {
        auto cb = onAction2;
        clearActions();
        setVisible(false);
        if (cb) cb();
    }
}

//==============================================================================
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

        juce::Rectangle<float> accentRect(bounds.getX(), bounds.getY(),
                                          4.0f, bounds.getHeight());
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

    if (hasActions)
    {
        // Main message in top portion
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white);
        g.drawText(message,
                   juce::Rectangle<float>(textX, 8.0f, textW, 20.0f),
                   juce::Justification::centredLeft, true);

        // Action buttons row
        const int btnY      = (int)bounds.getHeight() - 28;
        const int btnH      = 22;
        const int btnPad    = 6;

        // Action 1 (pink)
        {
            juce::Font f1(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
            g.setFont(f1);
            g.setColour(BdgColours::primary);
            juce::String a1 = action1Label;
            juce::GlyphArrangement ga1;
            ga1.addFittedText(f1, a1, 0, 0, 1000, 20, juce::Justification::left, 1);
            int a1W = (int)ga1.getBoundingBox(0, -1, true).getWidth() + btnPad * 2;
            action1Rect = juce::Rectangle<int>((int)textX, btnY, a1W, btnH);
            g.drawText(a1, action1Rect.toFloat(), juce::Justification::centredLeft, false);
        }

        // Action 2 (muted)
        {
            juce::Font f2(juce::FontOptions().withHeight(11.0f));
            g.setFont(f2);
            g.setColour(BdgColours::textMuted);
            juce::String a2 = action2Label;
            juce::GlyphArrangement ga2;
            ga2.addFittedText(f2, a2, 0, 0, 1000, 20, juce::Justification::left, 1);
            int a2X = action1Rect.getRight() + 12;
            int a2W = (int)ga2.getBoundingBox(0, -1, true).getWidth() + btnPad * 2;
            action2Rect = juce::Rectangle<int>(a2X, btnY, a2W, btnH);
            g.drawText(a2, action2Rect.toFloat(), juce::Justification::centredLeft, false);
        }
    }
    else
    {
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
}
