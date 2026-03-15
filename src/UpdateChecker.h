#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    /// Call from MainComponent constructor.
    /// If 7+ days since last check, queries GitHub in background.
    /// onUpdateAvailable is called on the message thread with the new version string.
    void checkIfDue(juce::ApplicationProperties& props,
                    std::function<void(juce::String newVersion)> onUpdateAvailable);

private:
    void run() override;

    std::function<void(juce::String)> callback;
    juce::ApplicationProperties* appProps = nullptr;

    static constexpr int CHECK_INTERVAL_SECONDS = 7 * 24 * 60 * 60; // 7 days

    static bool isNewerVersion(const juce::String& remote, const juce::String& local);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
