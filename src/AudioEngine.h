#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::AsyncUpdater,
                    public juce::ChangeListener,
                    public juce::Timer
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
        // Task 2: disk space monitoring
        virtual void diskSpaceWarning(int remainingMinutes) {}
        virtual void recordingAutoStopped() {}
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
                          bool compressor,
                          bool deEsser);

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

    //==============================================================================
    // Timer (disk space polling – Task 2)
    void timerCallback() override;

    //==============================================================================
    // Task 3: Post-crash recovery
    juce::Array<juce::File> findOrphanedRecordings(const juce::File& destFolder);
    juce::File recoverRecording(const juce::File& orphanedChunkFolder);
    void discardRecording(const juce::File& orphanedChunkFolder);

private:
    juce::AudioDeviceManager deviceManager;

    std::atomic<float> rmsL{0.0f};
    std::atomic<float> rmsR{0.0f};
    std::atomic<float> gain{1.0f};
    std::atomic<bool>  monitorEnabled{false};

    juce::ListenerList<Listener> listeners;

    // Throttle: fire update at most every 50 ms
    std::atomic<juce::int64> lastUpdateMs{0};

    //==============================================================================
    // Recording members
    std::atomic<bool> recording{false};

    std::unique_ptr<juce::TimeSliceThread>                   writerThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>  threadedWriter;

    juce::WavAudioFormat  wavFormat;

    std::atomic<int>    nativeChannels{0};
    std::atomic<double> nativeSampleRate{0.0};

    // Chunk recording
    juce::File chunkFolder;
    int chunkIndex{0};
    std::atomic<juce::int64> samplesInChunk{0};
    juce::int64 samplesPerChunk{0}; // 5 min * sampleRate
    juce::FileOutputStream* rawChunkStream{nullptr}; // non-owning, for flush access
    std::atomic<bool> chunkRotationPending{false};

    // SpinLock to protect threadedWriter during chunk rotation
    juce::SpinLock writerLock;

    // Prevent dangling this in callAsync lambdas
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    bool openNextChunk();
    juce::File concatenateChunks();

    //==============================================================================
    // DSP background thread
    std::unique_ptr<juce::Thread> dspThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
