#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include "BinaryData.h"

// Shared content component for the About and Update dialogs.
//
// Both used to be built from raw `new` children added to a bare Component.
// JUCE does not delete a component's children when the parent goes away, so
// every time either dialog opened, the logo, the labels and the buttons all
// leaked — and the DialogWindow::LaunchOptions object itself was heap
// allocated and never freed, though it is designed to live on the stack.
//
// Here the children are members, so the compiler owns them, and the two
// dialogs stop being ~120 lines of copy-paste apart.
class BdgDialogContent : public juce::Component
{
public:
    struct ButtonSpec
    {
        juce::String text;
        bool primary = false;
        std::function<void()> onClick;   // dialog closes afterwards either way
    };

    BdgDialogContent(const juce::String& subtitle,
                     const juce::String& body,
                     std::vector<ButtonSpec> buttonSpecs)
    {
        setSize(320, 280);

        logo.setImage(juce::ImageCache::getFromMemory(BinaryData::logobdgrec_png,
                                                     BinaryData::logobdgrec_pngSize),
                      juce::RectanglePlacement::centred);
        logo.setBounds(60, 20, 200, 80);
        addAndMakeVisible(logo);

        if (subtitle.isNotEmpty())
        {
            subtitleLabel.setText(subtitle, juce::dontSendNotification);
            subtitleLabel.setFont(juce::FontOptions().withHeight(13.0f));
            subtitleLabel.setColour(juce::Label::textColourId, BdgColours::textMuted);
            subtitleLabel.setJustificationType(juce::Justification::centred);
            subtitleLabel.setBounds(0, 108, 320, 20);
            addAndMakeVisible(subtitleLabel);
        }

        bodyLabel.setText(body, juce::dontSendNotification);
        bodyLabel.setFont(juce::FontOptions().withHeight(subtitle.isNotEmpty() ? 13.0f : 14.0f));
        bodyLabel.setColour(juce::Label::textColourId, BdgColours::textPrimary);
        bodyLabel.setJustificationType(juce::Justification::centred);
        bodyLabel.setBounds(20, subtitle.isNotEmpty() ? 135 : 115,
                            280, subtitle.isNotEmpty() ? 80 : 60);
        addAndMakeVisible(bodyLabel);

        const int count = (int) buttonSpecs.size();
        const int btnW = count > 1 ? 110 : 100;
        const int gap = 20;
        const int totalW = count * btnW + (count - 1) * gap;
        int x = (320 - totalW) / 2;
        const int y = count > 1 ? 195 : 230;

        for (auto& spec : buttonSpecs)
        {
            auto* b = buttons.add(new juce::TextButton(spec.text));
            b->setColour(juce::TextButton::buttonColourId,
                         spec.primary ? BdgColours::primary : BdgColours::bgInput);
            b->setColour(juce::TextButton::textColourOffId,
                         spec.primary ? juce::Colours::white : BdgColours::textPrimary);
            b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
            b->setBounds(x, y, btnW, 30);

            auto action = spec.onClick;
            b->onClick = [this, action]()
            {
                if (action) action();
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->closeButtonPressed();
            };

            addAndMakeVisible(b);
            x += btnW + gap;
        }
    }

    // Builds the window, shows it, and hands ownership of `content` to it.
    // LaunchOptions belongs on the stack — heap-allocating it was the other
    // half of the leak.
    static void launch(const juce::String& title, BdgDialogContent* content)
    {
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(content);
        opts.dialogTitle = title;
        opts.dialogBackgroundColour = BdgColours::bgPanel;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;
        opts.launchAsync();
    }

private:
    juce::ImageComponent logo;
    juce::Label subtitleLabel, bodyLabel;
    juce::OwnedArray<juce::TextButton> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BdgDialogContent)
};
