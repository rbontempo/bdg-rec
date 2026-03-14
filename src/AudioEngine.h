#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::AsyncUpdater
{
public:
    //==============================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void audioLevelsChanged(float rmsL, float rmsR) = 0;
    };

    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;

    // Initialise device manager and register as audio callback
    void initialise();

    // Device enumeration / selection
    juce::StringArray getInputDevices();
    void setInputDevice(const juce::String& name);

    // Software gain (0.0 – 2.0)
    void  setGain(float g);
    float getGain() const;

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

private:
    juce::AudioDeviceManager deviceManager;

    std::atomic<float> rmsL{0.0f};
    std::atomic<float> rmsR{0.0f};
    std::atomic<float> gain{1.0f};

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
