#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "Strings.h"

class OutputPanel : public juce::Component
{
public:
    OutputPanel();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateLanguage();

    // Getters for treatment toggles
    bool isNormalizeOn()       const { return normalizeOn; }
    bool isNoiseReductionOn()  const { return noiseReductionOn; }
    bool isCompressorOn()      const { return compressorOn; }
    bool isDeEsserOn()         const { return deEsserOn; }

    // Folder getter
    juce::File getDestFolder() const { return destFolder; }

    // Settings persistence setters (Task 18)
    void setDestFolder(const juce::File& folder);
    void setNormalize(bool v);
    void setNoiseReduction(bool v);
    void setCompressor(bool v);
    void setDeEsser(bool v);

    // Callback for settings changes (Task 18)
    std::function<void()> onSettingsChanged;

private:
    // Folder button (clickable)
    struct FolderButton : public juce::Component
    {
        FolderButton() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
        std::function<void()> onClick;
        void mouseDown(const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }
        void paint(juce::Graphics& g) override
        {
            // Transparent but needs to be "opaque" enough for hit testing
        }
        bool hitTest(int, int) override { return true; }
    };

    // Toggle button (custom pill)
    struct ToggleRow : public juce::Component
    {
        juce::String labelText;
        bool value = false;
        std::function<void(bool)> onToggle;

        explicit ToggleRow(const juce::String& text) : labelText(text) {}

        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent& e) override;
    };

    FolderButton folderButton;
    ToggleRow    normalizeRow  { Strings::get().normalizar };
    ToggleRow    noiseRow      { Strings::get().reducaoRuido };
    ToggleRow    compressorRow { Strings::get().compressor };
    ToggleRow    deEsserRow    { Strings::get().deEsser };

    juce::File   destFolder;

    bool normalizeOn      = false;
    bool noiseReductionOn = false;
    bool compressorOn     = false;
    bool deEsserOn        = false;

    void openFolderChooser();

    // Folder chooser kept alive
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
