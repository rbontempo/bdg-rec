#include "BdgLookAndFeel.h"

BdgLookAndFeel::BdgLookAndFeel()
{
    // ComboBox colours
    setColour(juce::ComboBox::backgroundColourId,  BdgColours::bgInput);
    setColour(juce::ComboBox::textColourId,        BdgColours::textPrimary);
    setColour(juce::ComboBox::outlineColourId,     BdgColours::border);
    setColour(juce::ComboBox::arrowColourId,       BdgColours::primary);
    setColour(juce::ComboBox::focusedOutlineColourId, BdgColours::primary);

    setColour(juce::PopupMenu::backgroundColourId,      BdgColours::bgPanel);
    setColour(juce::PopupMenu::textColourId,            BdgColours::textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, BdgColours::primary.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, BdgColours::textPrimary);

    // Slider colours
    setColour(juce::Slider::thumbColourId,            BdgColours::textPrimary);
    setColour(juce::Slider::trackColourId,            BdgColours::bgInput);
    setColour(juce::Slider::backgroundColourId,       BdgColours::bgInput);

    // ToggleButton
    setColour(juce::ToggleButton::textColourId,       BdgColours::textPrimary);
    setColour(juce::ToggleButton::tickColourId,       BdgColours::primary);
    setColour(juce::ToggleButton::tickDisabledColourId, BdgColours::textMuted);

    // Label
    setColour(juce::Label::textColourId,              BdgColours::textPrimary);
    setColour(juce::Label::backgroundColourId,        juce::Colours::transparentBlack);

    // TextButton
    setColour(juce::TextButton::buttonColourId,       BdgColours::bgInput);
    setColour(juce::TextButton::buttonOnColourId,     BdgColours::primary);
    setColour(juce::TextButton::textColourOffId,      BdgColours::textPrimary);
    setColour(juce::TextButton::textColourOnId,       BdgColours::textPrimary);
}

void BdgLookAndFeel::drawComboBox(juce::Graphics& g,
                                   int width, int height,
                                   bool /*isButtonDown*/,
                                   int /*buttonX*/, int /*buttonY*/,
                                   int /*buttonW*/, int /*buttonH*/,
                                   juce::ComboBox& /*box*/)
{
    const float cornerRadius = 6.0f;
    juce::Rectangle<float> bounds(0.0f, 0.0f, (float)width, (float)height);

    // Background
    g.setColour(BdgColours::bgInput);
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);

    // Arrow (pink chevron)
    const float arrowSize = 8.0f;
    const float cx = width - 16.0f;
    const float cy = height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - arrowSize * 0.5f, cy - arrowSize * 0.3f);
    arrow.lineTo(cx, cy + arrowSize * 0.3f);
    arrow.lineTo(cx + arrowSize * 0.5f, cy - arrowSize * 0.3f);
    g.setColour(BdgColours::primary);
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void BdgLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                       int x, int y, int width, int height,
                                       float sliderPos,
                                       float minSliderPos, float /*maxSliderPos*/,
                                       juce::Slider::SliderStyle style,
                                       juce::Slider& slider)
{
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        const float trackH = 4.0f;
        const float trackY = y + (height - trackH) * 0.5f;
        const float thumbRadius = 7.0f;

        // Track background (10% white)
        g.setColour(juce::Colour(0x1affffff));
        g.fillRoundedRectangle((float)x, trackY, (float)width, trackH, trackH * 0.5f);

        // Filled range (pink)
        const float fillWidth = sliderPos - x;
        if (fillWidth > 0.0f)
        {
            g.setColour(BdgColours::primary);
            g.fillRoundedRectangle((float)x, trackY, fillWidth, trackH, trackH * 0.5f);
        }

        // Thumb shadow
        const float thumbX = sliderPos;
        const float thumbY = y + height * 0.5f;
        g.setColour(juce::Colour(0x40000000));
        g.fillEllipse(thumbX - thumbRadius + 1.0f, thumbY - thumbRadius + 1.0f,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);

        // Thumb (white circle, 14px diameter)
        g.setColour(BdgColours::textPrimary);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);
    }
    else if (style == juce::Slider::LinearVertical)
    {
        const float trackW = 4.0f;
        const float trackX = x + (width - trackW) * 0.5f;
        const float thumbRadius = 7.0f;

        // Track background
        g.setColour(juce::Colour(0x1affffff));
        g.fillRoundedRectangle(trackX, (float)y, trackW, (float)height, trackW * 0.5f);

        // Filled range (pink, from bottom up)
        const float fillHeight = (y + height) - sliderPos;
        if (fillHeight > 0.0f)
        {
            g.setColour(BdgColours::primary);
            g.fillRoundedRectangle(trackX, sliderPos, trackW, fillHeight, trackW * 0.5f);
        }

        // Thumb shadow
        const float thumbX = x + width * 0.5f;
        g.setColour(juce::Colour(0x40000000));
        g.fillEllipse(thumbX - thumbRadius + 1.0f, sliderPos - thumbRadius + 1.0f,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);

        // Thumb
        g.setColour(BdgColours::textPrimary);
        g.fillEllipse(thumbX - thumbRadius, sliderPos - thumbRadius,
                      thumbRadius * 2.0f, thumbRadius * 2.0f);
    }
    else
    {
        // Fallback to default for other styles
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                          sliderPos, minSliderPos, minSliderPos,
                                          style, slider);
    }
}

void BdgLookAndFeel::drawToggleButton(juce::Graphics& g,
                                       juce::ToggleButton& button,
                                       bool /*shouldDrawButtonAsHighlighted*/,
                                       bool /*shouldDrawButtonAsDown*/)
{
    // Pill dimensions
    const float pillW = 32.0f;
    const float pillH = 18.0f;
    const float pillX = 2.0f;
    const float pillY = (button.getHeight() - pillH) * 0.5f;
    const float cornerRadius = pillH * 0.5f;

    const bool isOn = button.getToggleState();

    // Pill background
    g.setColour(isOn ? BdgColours::primary : juce::Colour(0x26ffffff));
    g.fillRoundedRectangle(pillX, pillY, pillW, pillH, cornerRadius);

    // Thumb circle
    const float thumbDiameter = pillH - 4.0f;
    const float thumbY = pillY + 2.0f;
    const float thumbX = isOn ? (pillX + pillW - thumbDiameter - 2.0f) : (pillX + 2.0f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX, thumbY, thumbDiameter, thumbDiameter);

    // Label text to the right of pill
    if (button.getButtonText().isNotEmpty())
    {
        g.setColour(BdgColours::textPrimary);
        g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
        const float textX = pillX + pillW + 8.0f;
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float>(textX, 0.0f,
                                          button.getWidth() - textX,
                                          (float)button.getHeight()),
                   juce::Justification::centredLeft, true);
    }
}
