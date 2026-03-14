#include "AudioEngine.h"
#include "Dsp.h"

//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    // Stop DSP thread if active
    if (dspThread != nullptr)
    {
        dspThread->stopThread(10000);
        dspThread.reset();
    }

    // Stop recording if active
    if (recording.load())
        stopRecording();

    // Unregister callbacks before destruction
    deviceManager.removeChangeListener(this);
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

//==============================================================================
void AudioEngine::initialise()
{
    // Request 2 input channels, 0 output channels
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.initialiseWithDefaultDevices(2, 0);
    deviceManager.addAudioCallback(this);

    // Listen for device changes (hot-plug) — Task 19
    deviceManager.addChangeListener(this);
}

//==============================================================================
juce::StringArray AudioEngine::getInputDevices()
{
    juce::StringArray names;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        names = type->getDeviceNames(true); // true = input devices

    return names;
}

juce::String AudioEngine::getCurrentInputDeviceName() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getName();
    return {};
}

void AudioEngine::setInputDevice(const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName  = name;
    setup.useDefaultInputChannels = true;
    deviceManager.setAudioDeviceSetup(setup, true);
}

//==============================================================================
void AudioEngine::setGain(float g)
{
    gain.store(juce::jlimit(0.0f, 2.0f, g));
}

float AudioEngine::getGain() const
{
    return gain.load();
}

void AudioEngine::setMonitorEnabled(bool enabled)
{
    monitorEnabled.store(enabled);

    // Re-open device with output channels when monitoring is on
    auto setup = deviceManager.getAudioDeviceSetup();
    if (enabled)
    {
        setup.outputDeviceName = setup.inputDeviceName;
        setup.useDefaultOutputChannels = true;
    }
    else
    {
        setup.outputChannels.clear();
        setup.useDefaultOutputChannels = false;
    }
    deviceManager.setAudioDeviceSetup(setup, true);
}

float AudioEngine::getRmsL() const
{
    return rmsL.load();
}

float AudioEngine::getRmsR() const
{
    return rmsR.load();
}

//==============================================================================
void AudioEngine::addListener(Listener* l)
{
    listeners.add(l);
}

void AudioEngine::removeListener(Listener* l)
{
    listeners.remove(l);
}

//==============================================================================
// AudioIODeviceCallback
//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        nativeSampleRate = device->getCurrentSampleRate();
        nativeChannels   = device->getActiveInputChannels().countNumberOfSetBits();
        if (nativeChannels == 0)
            nativeChannels = 1; // fallback
    }

    lastUpdateMs = 0;
}

void AudioEngine::audioDeviceStopped()
{
    rmsL.store(0.0f);
    rmsR.store(0.0f);
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (numSamples == 0 || numInputChannels == 0)
        return;

    const float g = gain.load();

    // ---- Compute RMS for L and R (pre-gain, then scale by gain, clamped to 1) ----
    auto computeRms = [&](int channelIndex) -> float
    {
        if (channelIndex >= numInputChannels || inputChannelData[channelIndex] == nullptr)
            return 0.0f;

        const float* data = inputChannelData[channelIndex];
        double sumSq = 0.0;
        for (int i = 0; i < numSamples; ++i)
            sumSq += static_cast<double>(data[i]) * static_cast<double>(data[i]);

        float rms = static_cast<float>(std::sqrt(sumSq / numSamples));
        return juce::jmin(rms * g, 1.0f);
    };

    rmsL.store(computeRms(0));
    rmsR.store(computeRms(numInputChannels > 1 ? 1 : 0));

    // ---- Monitor: route input to output ----
    if (monitorEnabled.load() && numOutputChannels > 0)
    {
        for (int outCh = 0; outCh < numOutputChannels; ++outCh)
        {
            if (outputChannelData[outCh] == nullptr)
                continue;

            int inCh = outCh < numInputChannels ? outCh : 0;
            if (inputChannelData[inCh] != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                    outputChannelData[outCh][i] = inputChannelData[inCh][i] * g;
            }
            else
            {
                juce::FloatVectorOperations::clear(outputChannelData[outCh], numSamples);
            }
        }
    }
    else if (numOutputChannels > 0)
    {
        for (int outCh = 0; outCh < numOutputChannels; ++outCh)
            if (outputChannelData[outCh] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[outCh], numSamples);
    }

    // ---- Throttled UI notification (50 ms) ----
    const juce::int64 now = juce::Time::currentTimeMillis();
    if (now - lastUpdateMs >= 50)
    {
        lastUpdateMs = now;
        triggerAsyncUpdate();
    }

    // ---- Recording pipeline ----
    if (recording.load() && threadedWriter != nullptr)
    {
        // Downmix all input channels to mono, applying gain
        juce::AudioBuffer<float> monoBuffer(1, numSamples);
        monoBuffer.clear();

        float* mono = monoBuffer.getWritePointer(0);
        const float channelScale = 1.0f / static_cast<float>(numInputChannels);

        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            if (inputChannelData[ch] != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                    mono[i] += inputChannelData[ch][i] * channelScale * g;
            }
        }

        threadedWriter->write(monoBuffer.getArrayOfReadPointers(), numSamples);
    }
}

//==============================================================================
// AsyncUpdater
//==============================================================================
void AudioEngine::handleAsyncUpdate()
{
    listeners.call(&Listener::audioLevelsChanged, rmsL.load(), rmsR.load());
}

//==============================================================================
// ChangeListener – device hot-plug (Task 19)
//==============================================================================
void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // This is called on the message thread by AudioDeviceManager when
    // the device configuration changes (including hot-plug events).

    // If we are recording and the current device is gone, stop recording
    if (recording.load())
    {
        if (deviceManager.getCurrentAudioDevice() == nullptr)
        {
            stopRecording();
            listeners.call(&Listener::dspError,
                           juce::String("Dispositivo de audio desconectado durante gravacao."));
        }
    }

    // Notify all listeners so UI can refresh device list
    listeners.call(&Listener::devicesChanged);
}

//==============================================================================
// Recording pipeline
//==============================================================================
bool AudioEngine::startRecording(const juce::File& destFolder)
{
    if (recording.load())
        return false; // already recording

    // Make sure destination folder exists
    if (!destFolder.isDirectory())
        destFolder.createDirectory();

    // Build filename: BDG_rec_YYYY-MM-DD_HH-MM-SS.wav
    const juce::String timestamp =
        juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    const juce::String filename = "BDG_rec_" + timestamp + ".wav";
    currentRecordingFile = destFolder.getChildFile(filename);

    // Retrieve current device parameters
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        nativeSampleRate = device->getCurrentSampleRate();
        nativeChannels   = device->getActiveInputChannels().countNumberOfSetBits();
        if (nativeChannels == 0)
            nativeChannels = 1;
    }
    else
    {
        // No device open — cannot record
        return false;
    }

    // Create the output WAV file
    auto fileStream = std::unique_ptr<juce::FileOutputStream>(
        currentRecordingFile.createOutputStream());

    if (fileStream == nullptr)
        return false;

    // Create a WAV writer: mono, nativeSampleRate, 24-bit
    auto* writer = wavFormat.createWriterFor(
        fileStream.get(),
        nativeSampleRate,
        1,    // mono
        24,   // bit depth
        {},   // metadata
        0);   // quality option index

    if (writer == nullptr)
        return false;

    // Transfer ownership of the stream to the writer
    fileStream.release();

    // Spin up the background writer thread
    writerThread = std::make_unique<juce::TimeSliceThread>("BDG Audio Writer");
    writerThread->startThread();

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, *writerThread, 65536);

    recording.store(true);
    return true;
}

juce::File AudioEngine::stopRecording()
{
    recording.store(false);

    // Destroying ThreadedWriter flushes and closes the file
    threadedWriter.reset();

    if (writerThread != nullptr)
    {
        writerThread->stopThread(2000);
        writerThread.reset();
    }

    return currentRecordingFile;
}

bool AudioEngine::isRecording() const
{
    return recording.load();
}

//==============================================================================
// DSP processing on background thread
//==============================================================================
void AudioEngine::processRecording(const juce::File& file,
                                   bool doNormalize,
                                   bool doNoiseReduction,
                                   bool doCompressor)
{
    // If a previous DSP thread is still running, wait for it
    if (dspThread != nullptr)
    {
        dspThread->stopThread(10000);
        dspThread.reset();
    }

    // Capture what we need for the lambda
    auto* engine = this;

    class DspThread : public juce::Thread
    {
    public:
        DspThread(AudioEngine* e, juce::File f, bool norm, bool nr, bool comp)
            : juce::Thread("BDG DSP"),
              engine(e), file(std::move(f)),
              doNormalize(norm), doNoiseReduction(nr), doCompressor(comp) {}

        void run() override
        {
            auto emitStep = [this](const juce::String& step)
            {
                auto* eng = engine;
                juce::MessageManager::callAsync([eng, step]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspStepChanged, step);
                });
            };

            // 1) Read the WAV file
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
            {
                auto* eng = engine;
                juce::String path = file.getFullPathName();
                juce::MessageManager::callAsync([eng, path]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspError,
                        juce::String("Failed to read file: ") + path);
                });
                return;
            }

            const int numSamples = static_cast<int>(reader->lengthInSamples);
            const double sampleRate = reader->sampleRate;
            const int bitsPerSample = reader->bitsPerSample;

            juce::AudioBuffer<float> buffer(1, numSamples);
            reader->read(&buffer, 0, numSamples, 0, true, false);
            reader.reset(); // close the file

            // 2) Notify DSP started
            {
                auto* eng = engine;
                juce::MessageManager::callAsync([eng]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspStarted);
                });
            }

            if (threadShouldExit()) return;

            // 3) Normalize
            if (doNormalize)
            {
                emitStep("normalize");
                Dsp::normalize(buffer);
            }

            if (threadShouldExit()) return;

            // 4) Noise reduction
            if (doNoiseReduction)
            {
                emitStep("noise_reduction");
                Dsp::noiseReduce(buffer, sampleRate);
            }

            if (threadShouldExit()) return;

            // 5) Compressor
            if (doCompressor)
            {
                emitStep("compressor");
                Dsp::compress(buffer, sampleRate);
            }

            if (threadShouldExit()) return;

            // 6) Final normalize if normalize was on AND we did other processing
            if (doNormalize && (doNoiseReduction || doCompressor))
            {
                emitStep("normalize_final");
                Dsp::normalize(buffer);
            }

            if (threadShouldExit()) return;

            // 7) Save back to file
            emitStep("saving");

            juce::File tempFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_tmp.wav");

            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::FileOutputStream> outStream(
                    tempFile.createOutputStream());

                if (outStream == nullptr)
                {
                    auto* eng = engine;
                    juce::MessageManager::callAsync([eng]()
                    {
                        eng->listeners.call(&AudioEngine::Listener::dspError,
                            juce::String("Failed to create output file"));
                    });
                    return;
                }

                auto* writer = wav.createWriterFor(
                    outStream.get(), sampleRate, 1, bitsPerSample, {}, 0);

                if (writer == nullptr)
                {
                    auto* eng = engine;
                    juce::MessageManager::callAsync([eng]()
                    {
                        eng->listeners.call(&AudioEngine::Listener::dspError,
                            juce::String("Failed to create WAV writer"));
                    });
                    return;
                }

                outStream.release(); // writer owns the stream now
                writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
                delete writer;
            }

            // Replace original with processed file
            tempFile.moveFileTo(file);

            // 8) Notify finished
            {
                auto* eng = engine;
                juce::File resultFile = file;
                juce::MessageManager::callAsync([eng, resultFile]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspFinished, resultFile);
                });
            }
        }

    private:
        AudioEngine* engine;
        juce::File file;
        bool doNormalize, doNoiseReduction, doCompressor;
    };

    dspThread = std::make_unique<DspThread>(engine, file,
                                            doNormalize, doNoiseReduction, doCompressor);
    dspThread->startThread();
}
