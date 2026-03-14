#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BdgColours.h"
#include <functional>

class ToastComponent : public juce::Component,
                       public juce::Timer
{
public:
    ToastComponent();
    ~ToastComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void showSuccess(const juce::String& msg, const juce::String& subtitle = "");
    void showError  (const juce::String& msg);
    void showWithActions(const juce::String& msg,
                         const juce::String& action1Label, std::function<void()> onAction1,
                         const juce::String& action2Label, std::function<void()> onAction2);

    void timerCallback() override;

    // Toast dimensions
    static constexpr int toastWidth      = 400;
    static constexpr int toastHeight     = 60;
    static constexpr int toastHeightTall = 80;

private:
    enum class ToastType { Success, Error };

    juce::String  message;
    juce::String  subtitle;
    ToastType     type = ToastType::Success;

    // Action buttons state
    bool          hasActions = false;
    juce::String  action1Label;
    juce::String  action2Label;
    std::function<void()> onAction1;
    std::function<void()> onAction2;

    // Hit rects for action buttons (computed in paint)
    juce::Rectangle<int> action1Rect;
    juce::Rectangle<int> action2Rect;

    void showToast();
    void clearActions();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToastComponent)
};
