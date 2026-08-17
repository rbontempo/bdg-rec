#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>

class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener,
                    public juce::Timer
{
public:
    // Why a recording ended. Carried through to the listener so the UI can say
    // the right thing without each stop path inventing its own notification.
    enum class StopReason
    {
        UserRequested,
        DiskFull,
        DeviceLost,
        DeviceRateChanged
    };

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
        // The take was merged and written successfully. Always delivered on
        // the message thread, possibly a while after the stop was requested —
        // merging a long recording takes time and now runs in the background.
        virtual void recordingFinished(const juce::File& file, StopReason reason) {}
        // Concatenation failed (usually a full disk). The raw chunks were NOT
        // deleted — they stay in this folder and the orphan-recovery flow can
        // pick them up on the next launch.
        virtual void recordingSaveFailed(const juce::File& preservedChunkFolder) {}
    };

    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;

    // Initialise device manager and register as audio callback
    void initialise();

    // Device enumeration / selection
    // Enumerating audio inputs asks macOS for microphone access, so the
    // result is cached: the app used to rescan on every call, and each scan
    // was another permission window in the user's face. Hot-plug invalidates
    // it, which is the only moment a rescan is actually needed.
    juce::StringArray getInputDevices();
    void setInputDevice(const juce::String& name);
    juce::String getCurrentInputDeviceName() const;

    // Coarse device class ("builtin" / "usb" / "bluetooth" / "other" / "none").
    // Device *names* routinely embed the owner's name ("AirPods do Renato"),
    // so analytics reports this fixed vocabulary instead of the raw string.
    juce::String getCurrentInputDeviceCategory() const;

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

    // Stops and merges in the background; the result arrives via
    // Listener::recordingFinished or Listener::recordingSaveFailed. This is
    // what the UI should call.
    void        stopRecordingAsync(StopReason reason);

    // Blocking variant: stops and merges inline, returning the finished file
    // (or an empty File on failure). Only for shutdown and tests — merging a
    // long take here will block the calling thread for as long as it takes.
    juce::File  stopRecording();

    bool        isRecording() const;

    // Samples the write path could not accept during the last take (disk
    // stalls). Zero on a healthy recording.
    juce::int64 getDroppedSamples() const { return droppedSamples.load(); }

    // Rate the last take was actually recorded at. Comes from the snapshot
    // taken when the take stopped — reading the live nativeSampleRate here
    // would report whatever the device renegotiated to in the meantime,
    // which is the very thing the snapshot exists to avoid.
    double getTakeSampleRate() const { return pendingSave.sampleRate; }

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

    juce::StringArray cachedInputDevices;
    // Set by hot-plug. Without it the app rescanned even right after
    // initialiseWithDefaultDevices(), which had just enumerated everything —
    // a second microphone permission request for no new information.
    bool needsDeviceRescan{true};

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
    std::atomic<bool> chunkRotationPending{false};

    // Set when the device reopens at a rate the current chunks weren't
    // written at. Stops the write path immediately, without clearing
    // `recording` — stopRecording() still has to run to save what we have.
    std::atomic<bool> deviceMismatch{false};

    // Pre-allocated in audioDeviceAboutToStart. Building an AudioBuffer inside
    // the callback meant a malloc/free on the audio thread for every block.
    juce::AudioBuffer<float> monoBuffer;

    // Samples the writer refused (its FIFO backed up because the disk stalled)
    // plus anything a short scratch buffer could not hold. Counted so the loss
    // is reportable instead of silent.
    std::atomic<juce::int64> droppedSamples{0};

    // One always-on timer at 20 Hz: it publishes the VU levels (so the audio
    // thread never has to post a message) and, while recording, checks the
    // rotation flag every tick and the disk every 200th — the original 10 s.
    int timerTicks{0};
    static constexpr int kTimerIntervalMs = 50;
    static constexpr int kDiskCheckEveryTicks = 200;

    // SpinLock to protect threadedWriter during chunk rotation
    juce::SpinLock writerLock;

    // Prevent dangling this in callAsync lambdas
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    // Result of merging the chunk files into the final WAV. `ok` is only true
    // when every sample was written AND the finished file reads back at the
    // expected length — callers must check it before deleting the chunks.
    struct ConcatResult
    {
        juce::File file;
        bool ok = false;
    };

    bool openNextChunk();
    // Everything the merge needs, captured at the moment the take stops.
    // The merge runs on a background thread; reading the live members there
    // let a device reopening mid-shutdown rewrite nativeSampleRate and stamp
    // the final header with a rate the chunks were never recorded at.
    struct SaveJob
    {
        juce::File  chunkFolder;
        int         chunkIndex = 0;
        double      sampleRate = 0.0;
    };

    ConcatResult concatenateChunks(const SaveJob& job);

    // The two halves of stopping: finishWriting() is fast (closes the writer
    // and snapshots the job), finaliseSave() is the expensive merge.
    bool finishWriting();
    juce::File finaliseSave();

    SaveJob pendingSave;

    // Runs finaliseSave() off the message thread and reports back on it.
    class SaveThread : public juce::Thread
    {
    public:
        SaveThread(AudioEngine& e, StopReason r)
            : juce::Thread("BDG Save"), engine(e), reason(r) {}

        void run() override
        {
            auto file = engine.finaliseSave();
            auto aliveFlag = engine.alive;
            auto* eng = &engine;
            const auto why = reason;

            if (file == juce::File())
                return;   // finaliseSave() already reported the failure

            juce::MessageManager::callAsync([eng, aliveFlag, file, why]()
            {
                // Qualified: juce::Thread also has a nested Listener, and
                // inside this class that one wins name lookup.
                if (aliveFlag->load())
                    eng->listeners.call(&AudioEngine::Listener::recordingFinished, file, why);
            });
        }

    private:
        AudioEngine& engine;
        StopReason reason;
    };

    std::unique_ptr<SaveThread> saveThread;

    //==============================================================================
    // DSP background thread
    std::unique_ptr<juce::Thread> dspThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
