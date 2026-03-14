#include "AudioEngine.h"
#include "Dsp.h"
#include "Strings.h"

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
    auto error = deviceManager.initialiseWithDefaultDevices(2, 0);
    if (error.isNotEmpty())
        DBG("AudioEngine::initialise error: " + error);

    deviceManager.addAudioCallback(this);
    deviceManager.addChangeListener(this);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        DBG("AudioEngine::initialise OK: " + dev->getName());
    else
        DBG("AudioEngine::initialise: no device opened by default");
}

//==============================================================================
juce::StringArray AudioEngine::getInputDevices()
{
    juce::StringArray names;

    // Try current device type first
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        names = type->getDeviceNames(true);
    }

    // Fallback: iterate all available device types
    if (names.isEmpty())
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            auto devNames = type->getDeviceNames(true);
            names.addArray(devNames);
        }
    }

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
    setup.inputChannels.setRange(0, 2, true); // ensure at least 2 input channels requested
    setup.sampleRate = 0; // use device default
    setup.bufferSize = 0; // use device default
    auto error = deviceManager.setAudioDeviceSetup(setup, true);

    if (error.isNotEmpty())
        DBG("setInputDevice error: " + error);
    else
        DBG("setInputDevice OK: " + name);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        DBG("  Active device: " + dev->getName()
            + " SR=" + juce::String(dev->getCurrentSampleRate())
            + " inputs=" + juce::String(dev->getActiveInputChannels().countNumberOfSetBits()));
    else
        DBG("  No active device after setInputDevice!");
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
            nativeChannels = 1;
        DBG("audioDeviceAboutToStart: " + device->getName()
            + " SR=" + juce::String(nativeSampleRate)
            + " ch=" + juce::String(nativeChannels));
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

        samplesInChunk += numSamples;

        if (samplesInChunk >= samplesPerChunk)
        {
            juce::MessageManager::callAsync([this]() {
                if (recording.load())
                    openNextChunk();
            });
        }
    }
}

//==============================================================================
// AsyncUpdater
//==============================================================================
void AudioEngine::handleAsyncUpdate()
{
    float l = rmsL.load();
    float r = rmsR.load();
    static int dbgCount = 0;
    if (++dbgCount % 20 == 0) // log every ~1 second
        DBG("RMS: L=" + juce::String(l, 4) + " R=" + juce::String(r, 4) + " listeners=" + juce::String(listeners.size()));
    listeners.call(&Listener::audioLevelsChanged, l, r);
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
            listeners.call(&Listener::dspError, Strings::get().audioDesconectado);
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
    DBG("AudioEngine::startRecording destFolder=" + destFolder.getFullPathName());
    if (recording.load())
    {
        DBG("  Already recording, returning false");
        return false;
    }

    // Make sure destination folder exists
    if (!destFolder.isDirectory())
        destFolder.createDirectory();

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
        return false;
    }

    // Create timestamped subfolder for chunks
    const juce::String timestamp =
        juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    chunkFolder = destFolder.getChildFile("BDG_rec_" + timestamp);
    chunkFolder.createDirectory();

    // 5 minutes per chunk
    samplesPerChunk = (juce::int64)(5.0 * 60.0 * nativeSampleRate);
    chunkIndex = 0;

    if (!openNextChunk())
    {
        chunkFolder.deleteRecursively();
        return false;
    }

    recording.store(true);
    startTimer(10000); // poll disk space every 10 seconds
    DBG("  Chunk recording started. chunkFolder=" + chunkFolder.getFullPathName()
        + " samplesPerChunk=" + juce::String(samplesPerChunk));
    return true;
}

bool AudioEngine::openNextChunk()
{
    // Flush and close current chunk
    threadedWriter.reset();

    if (writerThread != nullptr)
    {
        writerThread->stopThread(2000);
        writerThread.reset();
    }

    chunkIndex++;

    juce::File chunkFile = chunkFolder.getChildFile(
        juce::String::formatted("chunk_%03d.wav", chunkIndex));

    auto fileStream = std::unique_ptr<juce::FileOutputStream>(
        chunkFile.createOutputStream());

    if (fileStream == nullptr)
    {
        DBG("  Failed to create FileOutputStream for chunk: " + chunkFile.getFullPathName());
        return false;
    }

    rawChunkStream = fileStream.get();

    auto* writer = wavFormat.createWriterFor(
        fileStream.get(),
        nativeSampleRate,
        1,    // mono
        24,   // bit depth
        {},   // metadata
        0);   // quality option index

    if (writer == nullptr)
    {
        DBG("  Failed to create WAV writer for chunk " + juce::String(chunkIndex));
        rawChunkStream = nullptr;
        return false;
    }

    // Transfer ownership of the stream to the writer
    fileStream.release();

    writerThread = std::make_unique<juce::TimeSliceThread>("BDG Audio Writer");
    writerThread->startThread();

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, *writerThread, 65536);

    samplesInChunk = 0;

    DBG("  Opened chunk " + juce::String(chunkIndex) + ": " + chunkFile.getFullPathName());
    return true;
}

juce::File AudioEngine::concatenateChunks()
{
    juce::File finalFile = chunkFolder.getParentDirectory().getChildFile(
        chunkFolder.getFileName() + ".wav");

    auto outStream = std::unique_ptr<juce::FileOutputStream>(
        finalFile.createOutputStream());

    if (outStream == nullptr)
    {
        DBG("  concatenateChunks: failed to create output stream");
        return {};
    }

    auto* finalWriter = wavFormat.createWriterFor(
        outStream.get(),
        nativeSampleRate,
        1,    // mono
        24,   // bit depth
        {},   // metadata
        0);   // quality option index

    if (finalWriter == nullptr)
    {
        DBG("  concatenateChunks: failed to create WAV writer");
        return {};
    }

    outStream.release(); // writer owns the stream

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    for (int i = 1; i <= chunkIndex; ++i)
    {
        juce::File chunkFile = chunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(chunkFile));

        if (reader == nullptr)
        {
            DBG("  concatenateChunks: skipping corrupted chunk " + juce::String(i));
            continue;
        }

        const juce::int64 totalSamples = reader->lengthInSamples;
        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (juce::int64 pos = 0; pos < totalSamples; pos += blockSize)
        {
            int samplesToRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - pos);
            reader->read(&buffer, 0, samplesToRead, pos, true, false);
            finalWriter->writeFromAudioSampleBuffer(buffer, 0, samplesToRead);
        }
    }

    delete finalWriter;

    DBG("  concatenateChunks: final file = " + finalFile.getFullPathName());
    return finalFile;
}

juce::File AudioEngine::stopRecording()
{
    stopTimer();
    recording.store(false);

    // Flush and close current chunk
    threadedWriter.reset();

    if (writerThread != nullptr)
    {
        writerThread->stopThread(2000);
        writerThread.reset();
    }

    rawChunkStream = nullptr;

    // Concatenate all chunks into final file
    juce::File finalFile = concatenateChunks();

    // Clean up chunk folder
    chunkFolder.deleteRecursively();

    return finalFile;
}

bool AudioEngine::isRecording() const
{
    return recording.load();
}

//==============================================================================
// Task 2: Disk space polling timer
//==============================================================================
void AudioEngine::timerCallback()
{
    if (!recording.load()) return;

    auto freeBytes = chunkFolder.getBytesFreeOnVolume();
    int bytesPerSec = (int)(nativeSampleRate * 3.0); // 24-bit mono = 3 bytes/sample
    int remainingSec = (bytesPerSec > 0) ? (int)(freeBytes / bytesPerSec) : 9999;
    int remainingMin = remainingSec / 60;

    listeners.call(&Listener::diskSpaceWarning, remainingMin);

    if (remainingMin < 2)
    {
        stopTimer();
        // Auto-stop on message thread
        juce::MessageManager::callAsync([this]() {
            if (recording.load())
            {
                stopRecording();
                listeners.call(&Listener::recordingAutoStopped);
            }
        });
    }
}

//==============================================================================
// Task 3: Post-crash recovery
//==============================================================================
juce::Array<juce::File> AudioEngine::findOrphanedRecordings(const juce::File& destFolder)
{
    juce::Array<juce::File> orphans;

    if (!destFolder.isDirectory())
        return orphans;

    for (const auto& entry : juce::RangedDirectoryIterator(destFolder, false, "*", juce::File::findDirectories))
    {
        juce::File dir = entry.getFile();
        if (!dir.getFileName().startsWith("BDG_rec_"))
            continue;

        // Check for at least one chunk file (chunk_001.wav is always the first)
        if (dir.getChildFile("chunk_001.wav").existsAsFile())
            orphans.add(dir);
    }

    return orphans;
}

juce::File AudioEngine::recoverRecording(const juce::File& orphanedChunkFolder)
{
    // Detect sample rate from first valid chunk
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    double detectedSampleRate = 44100.0;
    int detectedChunkCount = 0;

    // Count chunks and detect sample rate
    for (int i = 1; i <= 9999; ++i)
    {
        juce::File chunkFile = orphanedChunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));
        if (!chunkFile.existsAsFile())
            break;

        if (detectedChunkCount == 0)
        {
            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(chunkFile));
            if (reader != nullptr)
                detectedSampleRate = reader->sampleRate;
        }
        detectedChunkCount++;
    }

    if (detectedChunkCount == 0)
    {
        DBG("recoverRecording: no chunks found in " + orphanedChunkFolder.getFullPathName());
        return {};
    }

    // Create output file next to the orphaned folder
    juce::File finalFile = orphanedChunkFolder.getParentDirectory().getChildFile(
        orphanedChunkFolder.getFileName() + ".wav");

    auto outStream = std::unique_ptr<juce::FileOutputStream>(
        finalFile.createOutputStream());

    if (outStream == nullptr)
    {
        DBG("recoverRecording: failed to create output stream");
        return {};
    }

    auto* finalWriter = wavFormat.createWriterFor(
        outStream.get(),
        detectedSampleRate,
        1,    // mono
        24,   // bit depth
        {},
        0);

    if (finalWriter == nullptr)
    {
        DBG("recoverRecording: failed to create WAV writer");
        return {};
    }

    outStream.release(); // writer owns the stream

    for (int i = 1; i <= detectedChunkCount; ++i)
    {
        juce::File chunkFile = orphanedChunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(chunkFile));

        if (reader == nullptr)
        {
            DBG("recoverRecording: skipping corrupted chunk " + juce::String(i));
            continue;
        }

        const juce::int64 totalSamples = reader->lengthInSamples;
        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (juce::int64 pos = 0; pos < totalSamples; pos += blockSize)
        {
            int samplesToRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - pos);
            reader->read(&buffer, 0, samplesToRead, pos, true, false);
            finalWriter->writeFromAudioSampleBuffer(buffer, 0, samplesToRead);
        }
    }

    delete finalWriter;

    // Remove orphaned folder
    orphanedChunkFolder.deleteRecursively();

    DBG("recoverRecording: recovered to " + finalFile.getFullPathName());
    return finalFile;
}

void AudioEngine::discardRecording(const juce::File& orphanedChunkFolder)
{
    orphanedChunkFolder.deleteRecursively();
    DBG("discardRecording: deleted " + orphanedChunkFolder.getFullPathName());
}

//==============================================================================
// DSP processing on background thread
//==============================================================================
void AudioEngine::processRecording(const juce::File& file,
                                   bool doNormalize,
                                   bool doNoiseReduction,
                                   bool doCompressor,
                                   bool doDeEsser)
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
        DspThread(AudioEngine* e, juce::File f, bool norm, bool nr, bool comp, bool de)
            : juce::Thread("BDG DSP"),
              engine(e), file(std::move(f)),
              doNormalize(norm), doNoiseReduction(nr), doCompressor(comp), doDeEsser(de) {}

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

            auto emitError = [this](const juce::String& msg)
            {
                auto* eng = engine;
                juce::MessageManager::callAsync([eng, msg]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspError, msg);
                });
            };

            auto emitFinish = [this](const juce::File& f)
            {
                auto* eng = engine;
                juce::File resultFile = f;
                juce::MessageManager::callAsync([eng, resultFile]()
                {
                    eng->listeners.call(&AudioEngine::Listener::dspFinished, resultFile);
                });
            };

            // 1) Read the WAV file
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
            {
                emitError("Failed to read file: " + file.getFullPathName());
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

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 3) Normalize
            if (doNormalize)
            {
                DBG("DSP: running normalize on " + juce::String(numSamples) + " samples");
                emitStep("normalize");
                Dsp::normalize(buffer, sampleRate);
                DBG("DSP: normalize done");
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 4) Noise reduction
            if (doNoiseReduction)
            {
                DBG("DSP: running noiseReduce on " + juce::String(numSamples) + " samples, SR=" + juce::String(sampleRate)
                    + " (" + juce::String(numSamples / sampleRate, 1) + "s)");
                emitStep("noise_reduction");
                Dsp::noiseReduce(buffer, sampleRate);
                DBG("DSP: noiseReduce done");
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 5) Compressor
            if (doCompressor)
            {
                emitStep("compressor");
                Dsp::compress(buffer, sampleRate);
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 6) De-Esser
            if (doDeEsser)
            {
                emitStep("de_esser");
                Dsp::deEss(buffer, sampleRate);
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 7) Save back to file
            DBG("DSP: saving to " + file.getFullPathName());
            emitStep("saving");

            juce::File tempFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_tmp.wav");

            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::FileOutputStream> outStream(
                    tempFile.createOutputStream());

                if (outStream == nullptr)
                {
                    emitError("Failed to create output file");
                    return;
                }

                auto* writer = wav.createWriterFor(
                    outStream.get(), sampleRate, 1, bitsPerSample, {}, 0);

                if (writer == nullptr)
                {
                    emitError("Failed to create WAV writer");
                    return;
                }

                outStream.release(); // writer owns the stream now
                writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
                delete writer;
            }

            // Replace original with processed file
            bool moved = tempFile.moveFileTo(file);
            DBG("DSP: moveFileTo " + juce::String(moved ? "OK" : "FAILED"));

            // 8) Notify finished
            emitFinish(file);
        }

    private:
        AudioEngine* engine;
        juce::File file;
        bool doNormalize, doNoiseReduction, doCompressor, doDeEsser;
    };

    dspThread = std::make_unique<DspThread>(engine, file,
                                            doNormalize, doNoiseReduction, doCompressor, doDeEsser);
    dspThread->startThread();
}
