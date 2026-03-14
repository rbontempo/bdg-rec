#include "OutputPanel.h"

//==============================================================================
// ToggleRow paint + click
//==============================================================================
void OutputPanel::ToggleRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float pillW = 32.0f;
    const float pillH = 18.0f;
    const float pillX = bounds.getRight() - pillW;
    const float pillY = (bounds.getHeight() - pillH) * 0.5f;

    // Label
    g.setFont(juce::FontOptions().withHeight(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.70f));
    g.drawText(labelText,
               juce::Rectangle<float>(0.0f, 0.0f, pillX - 8.0f, bounds.getHeight()),
               juce::Justification::centredLeft, true);

    // Pill background
    juce::Colour pillBg = value ? BdgColours::primary : juce::Colours::white.withAlpha(0.15f);
    g.setColour(pillBg);
    g.fillRoundedRectangle(pillX, pillY, pillW, pillH, pillH * 0.5f);

    // Thumb
    const float thumbDiam = pillH - 4.0f;
    const float thumbY    = pillY + 2.0f;
    const float thumbX    = value ? (pillX + pillW - thumbDiam - 2.0f)
                                  : (pillX + 2.0f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX, thumbY, thumbDiam, thumbDiam);
}

void OutputPanel::ToggleRow::mouseUp(const juce::MouseEvent& /*e*/)
{
    value = !value;
    repaint();
    if (onToggle) onToggle(value);
}

//==============================================================================
// OutputPanel
//==============================================================================
OutputPanel::OutputPanel()
{
    // Default destination folder
    destFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    // Folder button
    folderButton.onClick = [this]() { openFolderChooser(); };
    addAndMakeVisible(folderButton);

    // Treatment toggles
    normalizeRow.onToggle = [this](bool v) { normalizeOn = v; };
    noiseRow.onToggle     = [this](bool v) { noiseReductionOn = v; };
    compressorRow.onToggle= [this](bool v) { compressorOn = v; };

    addAndMakeVisible(normalizeRow);
    addAndMakeVisible(noiseRow);
    addAndMakeVisible(compressorRow);
}

void OutputPanel::openFolderChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Selecionar pasta de destino",
        destFolder,
        "",
        true);

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.isDirectory())
            {
                destFolder = result;
                repaint();
            }
        });
}

//==============================================================================
void OutputPanel::paint(juce::Graphics& g)
{
    const float corner  = 8.0f;
    const float padding = 14.0f;
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);

    // Panel border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // === Header area ===
    const float headerH = 40.0f;

    // Headphones icon
    {
        const float iconW = 18.0f;
        const float iconH = 14.0f;
        const float ix = padding;
        const float iy = (headerH - iconH) * 0.5f;

        g.setColour(BdgColours::primary);

        juce::Path band;
        const float arcCX = ix + iconW * 0.5f;
        const float arcCY = iy + iconH * 0.6f;
        const float arcR  = iconW * 0.45f;
        band.addCentredArc(arcCX, arcCY, arcR, arcR, 0.0f,
                           juce::MathConstants<float>::pi,
                           2.0f * juce::MathConstants<float>::pi, true);
        g.strokePath(band, juce::PathStrokeType(1.5f,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));

        const float cupW = 4.0f;
        const float cupH = 7.0f;
        const float cupY = iy + iconH - cupH;
        g.fillRoundedRectangle(ix, cupY, cupW, cupH, 1.5f);
        g.fillRoundedRectangle(ix + iconW - cupW, cupY, cupW, cupH, 1.5f);
    }

    // "OUTPUT" label
    g.setFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setColour(BdgColours::textPrimary);
    g.drawText("OUTPUT",
               juce::Rectangle<float>(padding + 26.0f, 0.0f, 100.0f, headerH),
               juce::Justification::centredLeft, false);

    // Header separator
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);

    // === Content area starts after header ===
    const float contentX = padding;
    const float contentW = bounds.getWidth() - padding * 2.0f;
    float y = headerH + 14.0f;

    // --- FORMATO label ---
    g.setFont(juce::FontOptions().withHeight(10.0f));
    g.setColour(BdgColours::textMuted);
    g.drawText("FORMATO",
               juce::Rectangle<float>(contentX, y, contentW, 14.0f),
               juce::Justification::centredLeft, false);
    y += 16.0f;

    // Format display box
    {
        juce::Rectangle<float> box(contentX, y, contentW, 36.0f);
        g.setColour(BdgColours::bgInput);
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(BdgColours::border);
        g.drawRoundedRectangle(box.reduced(0.5f), 8.0f, 1.0f);

        g.setFont(juce::FontOptions().withHeight(12.0f).withStyle("Regular"));
        g.setColour(juce::Colours::white);
        g.drawText("48kHz / MONO / 24 bits / WAV",
                   box.withTrimmedLeft(10.0f),
                   juce::Justification::centredLeft, false);
    }
    y += 36.0f + 14.0f;

    // --- PASTA DE DESTINO label ---
    g.setFont(juce::FontOptions().withHeight(10.0f));
    g.setColour(BdgColours::textMuted);
    g.drawText("PASTA DE DESTINO",
               juce::Rectangle<float>(contentX, y, contentW, 14.0f),
               juce::Justification::centredLeft, false);
    y += 16.0f;

    // Folder button background (painted here, the FolderButton component
    // sits on top for click detection)
    {
        juce::Rectangle<float> box(contentX, y, contentW, 36.0f);
        g.setColour(BdgColours::bgInput);
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(BdgColours::border);
        g.drawRoundedRectangle(box.reduced(0.5f), 8.0f, 1.0f);

        // Folder icon (simple shape)
        {
            const float fx = contentX + 10.0f;
            const float fy = y + (36.0f - 14.0f) * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(0.60f));

            // Tab on top-left of folder
            juce::Path folder;
            folder.addRoundedRectangle(fx, fy + 3.0f, 14.0f, 11.0f, 1.5f);
            folder.addRoundedRectangle(fx, fy, 6.0f, 4.0f, 1.0f);
            g.fillPath(folder);
        }

        // Path text (truncated from left)
        juce::String pathStr = destFolder.getFullPathName();
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.drawText(pathStr,
                   juce::Rectangle<float>(contentX + 30.0f, y, contentW - 38.0f, 36.0f),
                   juce::Justification::centredLeft, true);
    }
    y += 36.0f + 14.0f;

    // === TREATMENT sub-panel ===
    const float treatH = 16.0f + 1.0f + (34.0f * 3.0f) + 12.0f; // header + rows
    juce::Rectangle<float> treatRect(contentX, y, contentW, treatH);

    // Background (3% white)
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.fillRoundedRectangle(treatRect, 8.0f);

    // Border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(treatRect.reduced(0.5f), 8.0f, 1.0f);

    // Treatment header row
    const float ty = treatRect.getY();
    const float tx = treatRect.getX() + 10.0f;
    float ty2 = ty + 10.0f;

    // Scissors / X icon
    {
        const float sx = tx;
        const float sy = ty2;
        const float sz = 12.0f;
        g.setColour(juce::Colours::white.withAlpha(0.50f));
        juce::Path scissors;
        scissors.startNewSubPath(sx,       sy);
        scissors.lineTo(sx + sz, sy + sz);
        scissors.startNewSubPath(sx + sz,  sy);
        scissors.lineTo(sx,      sy + sz);
        g.strokePath(scissors,
                     juce::PathStrokeType(1.5f,
                     juce::PathStrokeType::beveled,
                     juce::PathStrokeType::rounded));
    }

    // "TRATAMENTO" label
    g.setFont(juce::FontOptions().withHeight(10.0f).withStyle("Bold"));
    g.setColour(juce::Colours::white.withAlpha(0.50f));
    g.drawText("TRATAMENTO",
               juce::Rectangle<float>(tx + 16.0f, ty2, contentW - 26.0f, 12.0f),
               juce::Justification::centredLeft, false);

    ty2 += 14.0f + 6.0f;

    // Separator inside treatment box
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)ty2, treatRect.getX() + 1.0f, treatRect.getRight() - 1.0f);
}

void OutputPanel::resized()
{
    const float padding  = 14.0f;
    const float headerH  = 40.0f;
    const float contentW = getWidth() - padding * 2.0f;

    float y = headerH + 14.0f;
    // FORMATO label (14px) + box (36px) + gap (14px)
    y += 14.0f + 16.0f + 36.0f + 14.0f;

    // PASTA label (14px) + box (36px) + gap (14px)
    y += 14.0f + 16.0f;

    // Folder button overlaps the folder box
    folderButton.setBounds((int)padding, (int)y, (int)contentW, 36);
    y += 36.0f + 14.0f;

    // Treatment sub-panel: header 30px, then three rows 34px each, padding 12
    const float treatHeaderH = 30.0f + 6.0f; // icon row + separator
    const float rowH = 34.0f;
    const float treatPad = 12.0f;
    const float rowsStartY = y + treatHeaderH + treatPad;

    normalizeRow  .setBounds((int)(padding + treatPad), (int)(rowsStartY),
                             (int)(contentW - treatPad * 2), (int)rowH);
    noiseRow      .setBounds((int)(padding + treatPad), (int)(rowsStartY + rowH),
                             (int)(contentW - treatPad * 2), (int)rowH);
    compressorRow .setBounds((int)(padding + treatPad), (int)(rowsStartY + rowH * 2),
                             (int)(contentW - treatPad * 2), (int)rowH);
}
