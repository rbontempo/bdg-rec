#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::AsyncUpdater,
                    public juce::ChangeListener
{
public:
    //==============================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void audioLevelsChanged(float rmsL, float rmsR) = 0;
        virtual void dspStarted() {}
        virtual void dspStepChanged(const juce::String& step) {}
        virtual void dspFinished(const juce::File& file) {}
        virtual void dspError(const juce::String& error) {}
        // Called on the message thread when the device list changes (Task 19)
        virtual void devicesChanged() {}
    };

    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;

    // Initialise device manager and register as audio callback
    void initialise();

    // Device enumeration / selection
    juce::StringArray getInputDevices();
    void setInputDevice(const juce::String& name);
    juce::String getCurrentInputDeviceName() const;

    // Access device manager (for UI components)
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    // Software gain (0.0 – 2.0)
    void  setGain(float g);
    float getGain() const;

    // Monitor: route input to output in real-time
    void setMonitorEnabled(bool enabled);

    // RMS levels (post-gain, 0.0 – 1.0)
    float getRmsL() const;
    float getRmsR() const;

    // Listener management
    void addListener(Listener* l);
    void removeListener(Listener* l);

    //==============================================================================
    // Recording pipeline
    bool        startRecording(const juce::File& destFolder);
    juce::File  stopRecording();
    bool        isRecording() const;

    //==============================================================================
    // DSP processing (runs on background thread)
    void processRecording(const juce::File& file,
                          bool normalize,
                          bool noiseReduction,
                          bool compressor);

    //==============================================================================
    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==============================================================================
    // AsyncUpdater
    void handleAsyncUpdate() override;

    //==============================================================================
    // ChangeListener – receives device-manager change notifications (Task 19)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    juce::AudioDeviceManager deviceManager;

    std::atomic<float> rmsL{0.0f};
    std::atomic<float> rmsR{0.0f};
    std::atomic<float> gain{1.0f};
    std::atomic<bool>  monitorEnabled{false};

    juce::ListenerList<Listener> listeners;

    // Throttle: fire update at most every 50 ms
    juce::int64 lastUpdateMs{0};

    //==============================================================================
    // Recording members
    std::atomic<bool> recording{false};

    std::unique_ptr<juce::TimeSliceThread>                   writerThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>  threadedWriter;

    juce::WavAudioFormat  wavFormat;
    juce::File            currentRecordingFile;

    int    nativeChannels{0};
    double nativeSampleRate{0.0};

    //==============================================================================
    // DSP background thread
    std::unique_ptr<juce::Thread> dspThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
