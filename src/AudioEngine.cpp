#include "AudioEngine.h"

//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    // Stop recording if active
    if (recording.load())
        stopRecording();

    // Unregister audio callback before destruction
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
}

//==============================================================================
juce::StringArray AudioEngine::getInputDevices()
{
    juce::StringArray names;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        names = type->getDeviceNames(true); // true = input devices

    return names;
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
    float* const* /*outputChannelData*/,
    int /*numOutputChannels*/,
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
