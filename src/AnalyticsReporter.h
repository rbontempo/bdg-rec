#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

class AnalyticsReporter : private juce::Timer
{
public:
    AnalyticsReporter();
    ~AnalyticsReporter() override;

    void initialise(juce::ApplicationProperties& props, const juce::String& analyticsUrl);
    void trackEvent(const juce::String& eventType, const juce::var& extra = juce::var());
    void setContext(const juce::String& os, const juce::String& appVersion,
                    const juce::String& hardware, const juce::String& locale);
    juce::String getMachineId() const { return machineId; }
    void flush();

private:
    void timerCallback() override;
    void sendBatch();
    juce::String generateMachineId();
    void loadPendingEvents();
    void savePendingEvents();

    juce::ApplicationProperties* appProps = nullptr;
    juce::String apiUrl;
    juce::String apiKey;
    juce::String machineId;

    juce::String osName;
    juce::String appVersion;
    juce::String hardwareName;
    juce::String localeName;

    juce::CriticalSection queueLock;
    juce::Array<juce::var> eventQueue;
    std::atomic<bool> isSending { false };

    static constexpr int BATCH_INTERVAL_MS = 30000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyticsReporter)
};
