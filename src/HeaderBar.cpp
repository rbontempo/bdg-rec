#include "HeaderBar.h"
#include "BinaryData.h"

HeaderBar::HeaderBar()
{
    iconImage = juce::ImageCache::getFromMemory(
        BinaryData::iconebdg_png, BinaryData::iconebdg_pngSize);
    setSize(960, preferredHeight);
}

void HeaderBar::paint(juce::Graphics& g)
{
    const int h = getHeight();
    const int w = getWidth();

    // === Logo icon from image ===
    const float iconSize = 22.0f;
    const float iconX = 14.0f;
    const float iconY = (h - iconSize) * 0.5f;

    if (iconImage.isValid())
    {
        g.drawImage(iconImage,
                    juce::Rectangle<float>(iconX, iconY, iconSize, iconSize),
                    juce::RectanglePlacement::centred);
    }

    // === "BDG" + " REC" text ===
    const float textX = iconX + iconSize + 8.0f;
    const float textY = 0.0f;
    const float textH = (float)h;

    juce::Font boldFont(juce::FontOptions().withHeight(15.0f).withStyle("Bold"));

    // "BDG" in pink
    g.setFont(boldFont);
    g.setColour(BdgColours::primary);
    // Measure "BDG" width using GlyphArrangement
    juce::GlyphArrangement ga;
    ga.addLineOfText(boldFont, "BDG", 0.0f, 0.0f);
    const float bdgWidth = ga.getBoundingBox(0, -1, true).getWidth();

    g.drawText("BDG", juce::Rectangle<float>(textX, textY, bdgWidth + 4.0f, textH),
               juce::Justification::centredLeft, false);

    // " rec" in white
    g.setColour(BdgColours::textPrimary);
    g.drawText(" rec", juce::Rectangle<float>(textX + bdgWidth + 2.0f, textY, 52.0f, textH),
               juce::Justification::centredLeft, false);

    // === Language selector: PT | EN ===
    {
        bool isPt = (Strings::getLanguage() == Language::PT);

        g.setFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));

        // PT button
        g.setColour(isPt ? BdgColours::primary : BdgColours::textMuted);
        g.drawText("PT", ptRect.toFloat(), juce::Justification::centred, false);

        // Separator
        float sepX = (float)ptRect.getRight() + 1.0f;
        g.setColour(BdgColours::border);
        g.drawLine(sepX, (float)h * 0.3f, sepX, (float)h * 0.7f, 1.0f);

        // EN button
        g.setColour(!isPt ? BdgColours::primary : BdgColours::textMuted);
        g.drawText("EN", enRect.toFloat(), juce::Justification::centred, false);
    }

    // === Version label: right-aligned ===
    juce::Font mutedFont(juce::FontOptions().withHeight(12.0f));
    g.setFont(mutedFont);
    g.setColour(BdgColours::textMuted);
    g.drawText(juce::String("v") + JUCE_APPLICATION_VERSION_STRING,
               juce::Rectangle<float>(0.0f, 0.0f, (float)w - 14.0f, (float)h),
               juce::Justification::centredRight, false);

    // === Bottom border line ===
    g.setColour(BdgColours::border);
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
}

void HeaderBar::resized()
{
    const int h = getHeight();
    const int w = getWidth();
    // Position PT|EN to the left of "v1.0" (which is ~30px from right)
    const int btnW = 24;
    const int btnH = 20;
    const int y = (h - btnH) / 2;
    const int enX = w - 14 - 30 - btnW;      // left of "v1.0"
    const int ptX = enX - btnW - 4;           // left of separator

    ptRect = { ptX, y, btnW, btnH };
    enRect = { enX, y, btnW, btnH };
}

void HeaderBar::mouseMove(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();
    if (ptRect.contains(pos) || enRect.contains(pos))
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void HeaderBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    if (ptRect.contains(pos) && Strings::getLanguage() != Language::PT)
    {
        Strings::setLanguage(Language::PT);
        repaint();
        if (onLanguageChanged) onLanguageChanged();
    }
    else if (enRect.contains(pos) && Strings::getLanguage() != Language::EN)
    {
        Strings::setLanguage(Language::EN);
        repaint();
        if (onLanguageChanged) onLanguageChanged();
    }
}
