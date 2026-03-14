#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class OutputPanel : public juce::Component
{
public:
    OutputPanel();
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Getters for treatment toggles
    bool isNormalizeOn()       const { return normalizeOn; }
    bool isNoiseReductionOn()  const { return noiseReductionOn; }
    bool isCompressorOn()      const { return compressorOn; }

    // Folder getter
    juce::File getDestFolder() const { return destFolder; }

private:
    // Folder button (clickable)
    struct FolderButton : public juce::Component
    {
        std::function<void()> onClick;
        void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }
        void mouseDrag(const juce::MouseEvent&) override {}
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
    ToggleRow    normalizeRow  { "Normalizar" };
    ToggleRow    noiseRow      { "Reducao de Ruido" };
    ToggleRow    compressorRow { "Compressor" };

    juce::File   destFolder;

    bool normalizeOn      = false;
    bool noiseReductionOn = false;
    bool compressorOn     = false;

    void openFolderChooser();

    // Folder chooser kept alive
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
