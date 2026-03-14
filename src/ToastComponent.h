#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"

class ToastComponent : public juce::Component,
                       public juce::Timer
{
public:
    ToastComponent();
    ~ToastComponent() override;

    void paint(juce::Graphics& g) override;

    void showSuccess(const juce::String& msg, const juce::String& subtitle = "");
    void showError  (const juce::String& msg);

    void timerCallback() override;

    // Fixed toast dimensions
    static constexpr int toastWidth  = 400;
    static constexpr int toastHeight = 60;

private:
    enum class ToastType { Success, Error };

    juce::String  message;
    juce::String  subtitle;
    ToastType     type = ToastType::Success;

    void showToast();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToastComponent)
};
