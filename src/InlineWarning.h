#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "Strings.h"

// Notification card (Windows/Slack style) — top-right of parent.
// Two-line layout: bold title + message body, with X close button.
class InlineWarning : public juce::Component,
                      public juce::Timer
{
public:
    enum Level { Info, Warning, Error };

    InlineWarning()
    {
        setVisible(false);
        setAlwaysOnTop(true);
    }

    void show(const juce::String& msg, Level lvl = Warning, int /*autoHideMs*/ = 5000)
    {
        message = msg;
        level = lvl;
        alpha = 1.0f;
        fading = false;
        stopTimer();

        // Derive title from level (bilingual)
        bool isEN = Strings::getLanguage() == Language::EN;
        switch (lvl)
        {
            case Info:    title = "OK";                          break;
            case Warning: title = isEN ? "Warning" : "Aviso";   break;
            case Error:   title = isEN ? "Error"   : "Erro";    break;
        }

        if (auto* parent = getParentComponent())
        {
            auto pb = parent->getLocalBounds();
            const float cardW = 300.0f;
            const float iconAreaW = 42.0f;  // accent bar + icon + gap
            const float closeW = 32.0f;
            const float textAreaW = cardW - iconAreaW - closeW;

            // Measure how many lines the message needs
            juce::Font bodyFont(juce::FontOptions().withHeight(12.0f));
            juce::TextLayout layout;
            juce::AttributedString attrStr;
            attrStr.append(message, bodyFont, juce::Colours::white);
            attrStr.setWordWrap(juce::AttributedString::WordWrap::byWord);
            layout.createLayout(attrStr, textAreaW);
            const float textH = layout.getHeight();

            const float titleH = 22.0f;
            const float padV = 12.0f;
            const float cardH = juce::jmax(64.0f, titleH + textH + padV * 2.0f);

            int x = (pb.getWidth() - (int)cardW) / 2;
            int y = (pb.getHeight() - (int)cardH) / 2;
            setBounds(x, y, (int)cardW, (int)cardH);
        }

        setVisible(true);
        repaint();
    }

    void hide()
    {
        stopTimer();
        fading = false;
        setVisible(false);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        // Click anywhere on the card to dismiss
        fading = true;
        startTimerHz(30);
    }

    void timerCallback() override
    {
        alpha -= 0.08f;
        if (alpha <= 0.0f)
        {
            alpha = 0.0f;
            hide();
            return;
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f * alpha));
        g.fillRoundedRectangle(bounds.translated(1.0f, 2.0f), 8.0f);

        // Accent + background color by level
        juce::Colour accent;
        juce::Colour bgColour;
        switch (level)
        {
            case Info:    accent = BdgColours::vuGreen;  bgColour = juce::Colour(0xff1a2e1f); break; // dark green
            case Warning: accent = BdgColours::vuYellow; bgColour = juce::Colour(0xff2e2a1a); break; // dark yellow
            case Error:   accent = BdgColours::vuRed;    bgColour = juce::Colour(0xff2e1a1a); break; // dark red
        }

        // Card background
        g.setColour(bgColour.withAlpha(0.98f * alpha));
        g.fillRoundedRectangle(bounds, 8.0f);

        // Border tinted by accent
        g.setColour(accent.withAlpha(0.20f * alpha));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        // Left accent bar
        const float barW = 4.0f;
        juce::Path barPath;
        barPath.addRoundedRectangle(bounds.getX(), bounds.getY(),
                                     barW, bounds.getHeight(),
                                     8.0f, 8.0f, true, false, true, false);
        g.setColour(accent.withAlpha(alpha));
        g.fillPath(barPath);

        // Icon — checkmark for Info, triangle for Warning, circle for Error
        const float iconX = bounds.getX() + barW + 14.0f;
        const float iconCY = bounds.getY() + 22.0f; // align with title

        switch (level)
        {
            case Info:
            {
                // Filled circle with checkmark
                g.setColour(accent.withAlpha(alpha));
                g.fillEllipse(iconX, iconCY - 9.0f, 18.0f, 18.0f);
                g.setColour(juce::Colours::white.withAlpha(alpha));
                juce::Path check;
                check.startNewSubPath(iconX + 5.0f, iconCY);
                check.lineTo(iconX + 8.0f, iconCY + 3.5f);
                check.lineTo(iconX + 13.0f, iconCY - 3.5f);
                g.strokePath(check, juce::PathStrokeType(2.0f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));
                break;
            }
            case Warning:
            {
                // Triangle with !
                juce::Path tri;
                tri.addTriangle(iconX + 9.0f, iconCY - 9.0f,
                                iconX, iconCY + 9.0f,
                                iconX + 18.0f, iconCY + 9.0f);
                g.setColour(accent.withAlpha(alpha));
                g.fillPath(tri);
                g.setColour(BdgColours::bgPanel.withAlpha(alpha));
                g.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));
                g.drawText("!", juce::Rectangle<float>(iconX, iconCY - 4.0f, 18.0f, 14.0f),
                           juce::Justification::centred, false);
                break;
            }
            case Error:
            {
                // Filled circle with X
                g.setColour(accent.withAlpha(alpha));
                g.fillEllipse(iconX, iconCY - 9.0f, 18.0f, 18.0f);
                g.setColour(juce::Colours::white.withAlpha(alpha));
                const float cx = iconX + 9.0f;
                g.drawLine(cx - 4.0f, iconCY - 4.0f, cx + 4.0f, iconCY + 4.0f, 2.0f);
                g.drawLine(cx + 4.0f, iconCY - 4.0f, cx - 4.0f, iconCY + 4.0f, 2.0f);
                break;
            }
        }

        // Text area
        const float textX = iconX + 24.0f;
        const float textW = bounds.getRight() - 32.0f - textX;

        // Title (bold)
        g.setFont(juce::FontOptions().withHeight(13.0f).withStyle("Bold"));
        g.setColour(juce::Colours::white.withAlpha(0.95f * alpha));
        g.drawText(title,
                   juce::Rectangle<float>(textX, bounds.getY() + 10.0f, textW, 18.0f),
                   juce::Justification::centredLeft, false);

        // Message body (word-wrapped)
        {
            juce::Font bodyFont(juce::FontOptions().withHeight(12.0f));
            juce::AttributedString attrStr;
            attrStr.append(message, bodyFont, juce::Colours::white.withAlpha(0.70f * alpha));
            attrStr.setWordWrap(juce::AttributedString::WordWrap::byWord);
            juce::TextLayout layout;
            layout.createLayout(attrStr, textW);
            layout.draw(g, juce::Rectangle<float>(textX, bounds.getY() + 32.0f,
                                                   textW, bounds.getHeight() - 42.0f));
        }

        // X close button (top-right)
        const float xBtnX = bounds.getRight() - 22.0f;
        const float xBtnY = bounds.getY() + 12.0f;
        const float xS = 8.0f;

        g.setColour(juce::Colours::white.withAlpha(0.40f * alpha));
        g.drawLine(xBtnX - xS * 0.5f, xBtnY - xS * 0.5f,
                   xBtnX + xS * 0.5f, xBtnY + xS * 0.5f, 1.5f);
        g.drawLine(xBtnX + xS * 0.5f, xBtnY - xS * 0.5f,
                   xBtnX - xS * 0.5f, xBtnY + xS * 0.5f, 1.5f);
    }

private:
    juce::String message;
    juce::String title;
    Level level = Warning;
    float alpha = 1.0f;
    bool  fading = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InlineWarning)
};
