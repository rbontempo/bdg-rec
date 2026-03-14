#include "InputPanel.h"
#include "Strings.h"

//==============================================================================
InputPanel::InputPanel(AudioEngine& engine)
    : audioEngine(engine)
{
    // ---- Device ComboBox ----
    auto devices = audioEngine.getInputDevices();
    for (int i = 0; i < devices.size(); ++i)
        deviceCombo.addItem(devices[i], i + 1);

    // Pre-select the currently active device if known
    {
        juce::String currentName = audioEngine.getCurrentInputDeviceName();
        int idx = devices.indexOf(currentName);
        if (idx >= 0)
            deviceCombo.setSelectedItemIndex(idx, juce::dontSendNotification);
    }

    deviceCombo.onChange = [this]()
    {
        const juce::String name = deviceCombo.getText();
        if (name.isNotEmpty())
        {
            audioEngine.setInputDevice(name);
            if (onSettingsChanged) onSettingsChanged();
        }
    };

    addAndMakeVisible(deviceCombo);

    // ---- VU Meter ----
    addAndMakeVisible(vuMeter);
    audioEngine.addListener(this);

    // ---- Volume Slider ----
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 200.0, 1.0);
    volumeSlider.setValue(100.0);
    volumeSlider.onValueChange = [this]()
    {
        audioEngine.setGain(static_cast<float>(volumeSlider.getValue()) / 100.0f);
        if (onSettingsChanged) onSettingsChanged();
    };
    addAndMakeVisible(volumeSlider);

}

InputPanel::~InputPanel()
{
    audioEngine.removeListener(this);
}

//==============================================================================
void InputPanel::audioLevelsChanged(float l, float r)
{
    vuMeter.setLevels(l, r);
}

//==============================================================================
void InputPanel::setDevice(const juce::String& deviceName)
{
    auto devices = audioEngine.getInputDevices();
    int idx = devices.indexOf(deviceName);
    if (idx >= 0)
        deviceCombo.setSelectedItemIndex(idx, juce::dontSendNotification);
}

void InputPanel::setVolume(int value)
{
    volumeSlider.setValue(static_cast<double>(value), juce::dontSendNotification);
    audioEngine.setGain(static_cast<float>(value) / 100.0f);
    repaint(); // update volume % label
}

void InputPanel::refreshDeviceList()
{
    const juce::String currentName = deviceCombo.getText();

    deviceCombo.clear(juce::dontSendNotification);
    auto devices = audioEngine.getInputDevices();
    for (int i = 0; i < devices.size(); ++i)
        deviceCombo.addItem(devices[i], i + 1);

    // Re-select device by name if still available
    int idx = devices.indexOf(currentName);
    if (idx >= 0)
        deviceCombo.setSelectedItemIndex(idx, juce::dontSendNotification);
    else
        deviceCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
}

//==============================================================================
void InputPanel::paint(juce::Graphics& g)
{
    const float corner = 8.0f;
    auto bounds = getLocalBounds().toFloat();

    // Panel background
    g.setColour(BdgColours::bgPanel);
    g.fillRoundedRectangle(bounds, corner);

    // Panel border
    g.setColour(BdgColours::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // === Header area (40px tall) ===
    const float headerH = 40.0f;
    const float padding = 14.0f;

    // Arrow-into-box icon (signal entering)
    {
        const float iconS = 14.0f;
        const float ix = padding;
        const float iy = (headerH - iconS) * 0.5f;
        g.setColour(BdgColours::primary);

        // Box (open left side)
        juce::Path box;
        box.startNewSubPath(ix + iconS * 0.35f, iy);
        box.lineTo(ix + iconS, iy);
        box.lineTo(ix + iconS, iy + iconS);
        box.lineTo(ix + iconS * 0.35f, iy + iconS);
        g.strokePath(box, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered,
                     juce::PathStrokeType::square));

        // Arrow pointing right (into box)
        float cy = iy + iconS * 0.5f;
        g.drawLine(ix, cy, ix + iconS * 0.7f, cy, 1.5f);
        juce::Path arrow;
        arrow.startNewSubPath(ix + iconS * 0.5f, cy - 3.0f);
        arrow.lineTo(ix + iconS * 0.7f, cy);
        arrow.lineTo(ix + iconS * 0.5f, cy + 3.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered,
                     juce::PathStrokeType::rounded));
    }

    // "ENTRADA" label
    juce::Font headerFont(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
    g.setFont(headerFont);
    g.setColour(BdgColours::textPrimary);
    g.drawText(Strings::get().entrada,
               juce::Rectangle<float>(padding + 22.0f, 0.0f, 100.0f, headerH),
               juce::Justification::centredLeft, false);

    // Header separator line
    g.setColour(BdgColours::border);
    g.drawHorizontalLine((int)headerH, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);

    // ---- Device section label ----
    const float pad  = 14.0f;
    const float labelFont = 10.0f;
    juce::Font sectionFont(juce::FontOptions().withHeight(labelFont));
    g.setFont(sectionFont);
    g.setColour(BdgColours::textMuted);
    g.drawText(Strings::get().dispositivo,
               juce::Rectangle<float>(pad, headerH + 12.0f, bounds.getWidth() - pad * 2.0f, 14.0f),
               juce::Justification::centredLeft, false);

    // ---- VU Meter label ----
    const float vuLabelY = (float)vuMeter.getY() - 18.0f;
    g.drawText(Strings::get().nivel,
               juce::Rectangle<float>(pad, vuLabelY, bounds.getWidth() - pad * 2.0f, 14.0f),
               juce::Justification::centredLeft, false);

    // ---- Volume section label + percentage ----
    const float volLabelY = (float)volumeSlider.getY() - 18.0f;
    g.drawText(Strings::get().volume,
               juce::Rectangle<float>(pad, volLabelY, bounds.getWidth() - pad * 2.0f, 14.0f),
               juce::Justification::centredLeft, false);

    juce::String volPct = juce::String((int)volumeSlider.getValue()) + "%";
    g.setColour(BdgColours::primary);
    g.drawText(volPct,
               juce::Rectangle<float>(pad, volLabelY, bounds.getWidth() - pad * 2.0f, 14.0f),
               juce::Justification::centredRight, false);

}

//==============================================================================
void InputPanel::resized()
{
    const int w       = getWidth();
    const int pad     = 14;
    const int headerH = 40;
    const int innerW  = w - pad * 2;

    int y = headerH + 12;

    // Device label space (14px) + 4px gap
    y += 14 + 4;

    // Device ComboBox (28px)
    deviceCombo.setBounds(pad, y, innerW, 28);
    y += 28 + 16;

    // VU meter label space (14px) + 4px gap
    y += 14 + 4;

    // VU Meter: height = 2 bars (10px each) + 1 gap (6px) + scale (14px)
    const int vuH = 10 + 6 + 10 + 14;
    vuMeter.setBounds(pad, y, innerW, vuH);
    y += vuH + 16;

    // Volume label space (14px) + 4px gap
    y += 14 + 4;

    // Volume slider (24px)
    volumeSlider.setBounds(pad + 20, y, innerW - 20, 24);
    y += 24 + 8;

}
