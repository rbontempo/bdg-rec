#include "OutputPanel.h"
#include "Strings.h"

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
    // Default destination folder: Downloads on macOS, Documents on Windows
#if JUCE_MAC
    destFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads");
#else
    destFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
#endif

    // Folder button
    folderButton.onClick = [this]() { openFolderChooser(); };
    addAndMakeVisible(folderButton);

    // Treatment toggles
    normalizeRow.onToggle = [this](bool v)
    {
        normalizeOn = v;
        if (onSettingsChanged) onSettingsChanged();
    };
    noiseRow.onToggle = [this](bool v)
    {
        noiseReductionOn = v;
        if (onSettingsChanged) onSettingsChanged();
    };
    compressorRow.onToggle = [this](bool v)
    {
        compressorOn = v;
        if (onSettingsChanged) onSettingsChanged();
    };
    deEsserRow.onToggle = [this](bool v)
    {
        deEsserOn = v;
        if (onSettingsChanged) onSettingsChanged();
    };

    addAndMakeVisible(normalizeRow);
    addAndMakeVisible(noiseRow);
    addAndMakeVisible(compressorRow);
    addAndMakeVisible(deEsserRow);
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
                if (onSettingsChanged) onSettingsChanged();
            }
        });
}

//==============================================================================
// Layout constants — shared between paint() and resized()
//==============================================================================
namespace OutputLayout
{
    constexpr float padding   = 14.0f;
    constexpr float corner    = 8.0f;
    constexpr float headerH   = 40.0f;
    constexpr float labelH    = 14.0f;
    constexpr float labelGap  = 4.0f;   // gap between label and its box
    constexpr float boxH      = 36.0f;
    constexpr float sectionGap = 14.0f;
    constexpr float rowH      = 24.0f;
    constexpr float treatHeaderH = 30.0f;
    constexpr float treatSep   = 6.0f;
    constexpr float treatPad   = 6.0f;
}

// Returns the Y position of each element for a given panel width
struct OutputPositions
{
    float contentX, contentW;
    float formatLabelY, formatBoxY;
    float folderLabelY, folderBoxY;
    float treatY, treatRowsY;
    float treatH;

    static OutputPositions compute(float panelW)
    {
        using namespace OutputLayout;
        OutputPositions p;
        p.contentX = padding;
        p.contentW = panelW - padding * 2.0f;

        float y = headerH + sectionGap;

        // FORMATO
        p.formatLabelY = y;
        y += labelH + labelGap;
        p.formatBoxY = y;
        y += boxH + sectionGap;

        // PASTA DE DESTINO
        p.folderLabelY = y;
        y += labelH + labelGap;
        p.folderBoxY = y;
        y += boxH + sectionGap;

        // TRATAMENTO sub-panel
        p.treatY = y;
        p.treatRowsY = y + treatHeaderH + treatSep + treatPad;
        p.treatH = treatHeaderH + treatSep + treatPad + rowH * 4.0f + treatPad;

        return p;
    }
};

//==============================================================================
void OutputPanel::paint(juce::Graphics& g)
{
    using namespace OutputLayout;
    auto bounds = getLocalBounds().toFloat();
    auto pos = OutputPositions::compute(bounds.getWidth());

    // Panel background + border
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // === Header ===
    // Export/save icon (arrow pointing out of a tray)
    {
        const float iconS = 14.0f;
        const float ix = padding;
        const float iy = (headerH - iconS) * 0.5f;
        g.setColour(BdgColours::primary);

        juce::Path icon;
        // Tray (open top box)
        icon.startNewSubPath(ix, iy + iconS * 0.4f);
        icon.lineTo(ix, iy + iconS);
        icon.lineTo(ix + iconS, iy + iconS);
        icon.lineTo(ix + iconS, iy + iconS * 0.4f);
        g.strokePath(icon, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered,
                     juce::PathStrokeType::square));

        // Arrow pointing up (out of tray)
        float cx = ix + iconS * 0.5f;
        g.drawLine(cx, iy + iconS * 0.7f, cx, iy, 1.5f);
        // Arrowhead
        juce::Path arrow;
        arrow.startNewSubPath(cx - 3.0f, iy + 4.0f);
        arrow.lineTo(cx, iy);
        arrow.lineTo(cx + 3.0f, iy + 4.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered,
                     juce::PathStrokeType::rounded));
    }

    g.setFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setColour(BdgColours::textPrimary);
    g.drawText(Strings::get().saida, juce::Rectangle<float>(padding + 26.0f, 0.0f, 100.0f, headerH),
               juce::Justification::centredLeft, false);

    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, 1.0f, bounds.getRight() - 1.0f);

    // === FORMATO ===
    g.setFont(juce::FontOptions().withHeight(10.0f));
    g.setColour(BdgColours::textMuted);
    g.drawText(Strings::get().formato, juce::Rectangle<float>(pos.contentX, pos.formatLabelY, pos.contentW, labelH),
               juce::Justification::centredLeft, false);

    {
        juce::Rectangle<float> box(pos.contentX, pos.formatBoxY, pos.contentW, boxH);
        g.setColour(BdgColours::bgInput);
        g.fillRoundedRectangle(box, corner);
        g.setColour(BdgColours::border);
        g.drawRoundedRectangle(box.reduced(0.5f), corner, 1.0f);
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white);
        g.drawText("48kHz / MONO / 24 bits / WAV", box.withTrimmedLeft(10.0f),
                   juce::Justification::centredLeft, false);
    }

    // === PASTA DE DESTINO ===
    g.setFont(juce::FontOptions().withHeight(10.0f));
    g.setColour(BdgColours::textMuted);
    g.drawText(Strings::get().pastaDestino, juce::Rectangle<float>(pos.contentX, pos.folderLabelY, pos.contentW, labelH),
               juce::Justification::centredLeft, false);

    {
        juce::Rectangle<float> box(pos.contentX, pos.folderBoxY, pos.contentW, boxH);
        g.setColour(BdgColours::bgInput);
        g.fillRoundedRectangle(box, corner);
        g.setColour(BdgColours::border);
        g.drawRoundedRectangle(box.reduced(0.5f), corner, 1.0f);

        // Folder icon
        const float fx = pos.contentX + 10.0f;
        const float fy = pos.folderBoxY + (boxH - 14.0f) * 0.5f;
        g.setColour(juce::Colours::white.withAlpha(0.60f));
        juce::Path folder;
        folder.addRoundedRectangle(fx, fy + 3.0f, 14.0f, 11.0f, 1.5f);
        folder.addRoundedRectangle(fx, fy, 6.0f, 4.0f, 1.0f);
        g.fillPath(folder);

        // Path text
        g.setFont(juce::FontOptions().withHeight(12.0f));
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.drawText(destFolder.getFullPathName(),
                   juce::Rectangle<float>(pos.contentX + 30.0f, pos.folderBoxY, pos.contentW - 38.0f, boxH),
                   juce::Justification::centredLeft, true);
    }

    // === TRATAMENTO sub-panel ===
    {
        juce::Rectangle<float> treatRect(pos.contentX, pos.treatY, pos.contentW, pos.treatH);
        g.setColour(juce::Colours::white.withAlpha(0.03f));
        g.fillRoundedRectangle(treatRect, corner);
        g.setColour(BdgColours::border);
        g.drawRoundedRectangle(treatRect.reduced(0.5f), corner, 1.0f);

        // Sliders/EQ icon (3 horizontal lines with dots at different positions)
        const float sx = pos.contentX + treatPad;
        const float sy = pos.treatY + 9.0f;
        const float iconW = 12.0f;
        const float lineGap = 4.0f;
        g.setColour(juce::Colours::white.withAlpha(0.50f));

        for (int i = 0; i < 3; ++i)
        {
            float ly = sy + (float)i * lineGap;
            // Line
            g.drawLine(sx, ly, sx + iconW, ly, 1.2f);
            // Dot (at different positions per line)
            float dotX = sx + iconW * (i == 0 ? 0.7f : i == 1 ? 0.3f : 0.55f);
            g.fillEllipse(dotX - 1.5f, ly - 1.5f, 3.0f, 3.0f);
        }

        // "TRATAMENTO" label
        g.setFont(juce::FontOptions().withHeight(10.0f).withStyle("Bold"));
        g.setColour(juce::Colours::white.withAlpha(0.50f));
        g.drawText(Strings::get().tratamento,
                   juce::Rectangle<float>(sx + 16.0f, sy, pos.contentW - 26.0f, 12.0f),
                   juce::Justification::centredLeft, false);

        // Separator
        float sepY = pos.treatY + treatHeaderH;
        g.setColour(BdgColours::border);
        g.drawHorizontalLine((int)sepY, treatRect.getX() + 1.0f, treatRect.getRight() - 1.0f);
    }
}

//==============================================================================
// Settings persistence setters (Task 18)
//==============================================================================
void OutputPanel::setDestFolder(const juce::File& folder)
{
    destFolder = folder;
    repaint();
}

void OutputPanel::setNormalize(bool v)
{
    normalizeOn = v;
    normalizeRow.value = v;
    normalizeRow.repaint();
}

void OutputPanel::setNoiseReduction(bool v)
{
    noiseReductionOn = v;
    noiseRow.value = v;
    noiseRow.repaint();
}

void OutputPanel::setCompressor(bool v)
{
    compressorOn = v;
    compressorRow.value = v;
    compressorRow.repaint();
}

void OutputPanel::setDeEsser(bool v)
{
    deEsserOn = v;
    deEsserRow.value = v;
    deEsserRow.repaint();
}

//==============================================================================
void OutputPanel::resized()
{
    using namespace OutputLayout;
    auto pos = OutputPositions::compute((float)getWidth());

    // Folder button exactly over the painted folder box
    folderButton.setBounds((int)pos.contentX, (int)pos.folderBoxY, (int)pos.contentW, (int)boxH);

    // Toggle rows inside the treatment sub-panel
    const float rowX = pos.contentX + treatPad;
    const float rowW = pos.contentW - treatPad * 2.0f;
    float y = pos.treatRowsY;

    normalizeRow  .setBounds((int)rowX, (int)y, (int)rowW, (int)rowH);
    y += rowH;
    noiseRow      .setBounds((int)rowX, (int)y, (int)rowW, (int)rowH);
    y += rowH;
    compressorRow .setBounds((int)rowX, (int)y, (int)rowW, (int)rowH);
    y += rowH;
    deEsserRow    .setBounds((int)rowX, (int)y, (int)rowW, (int)rowH);
}
